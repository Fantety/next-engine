# 06 Multimodal Files and Chat Input Attachments

This module is responsible for implementing two related capabilities:

- Users can add attachments such as images, PDFs, audio files, and text files in the chat input box and submit them along with the prompt.
- When the model supports multimodal input, the server parses the attachments into message parts that the model can read.

Core principle: Attachments are not plain text. They must go through MIME identification, size limitations, security checks, capability matching, necessary transcoding or uploading, and only then enter `ModelRequest.messages`.

> Source code consistency note: V2 core already has a direction for durable typed prompt attachments/reference; this chapter's ChatDraft, data URL resolver, and ImageNormalizer are designs that replicate the full UI/server attachment pipeline. Some file/media/MCP-resource materialization aspects are still partial/follow-up in V2 parity.

## Module Responsibilities

This module is responsible for:

- Defining the attachment state within the chat input draft.
- Supporting addition methods such as drag-and-drop, file selection, pasting screenshots, and referencing local paths.
- Converting attachments to `MessagePart` before prompt admission.
- Parsing three source types (`path/dataURL/blobID`) on the server.
- Determining whether to allow image, file, audio, etc., inputs based on model capabilities.
- Converting attachments into provider-neutral `ModelMessagePart`.

This module is NOT responsible for:

- Determining history truncation.
- Executing tools.
- Forcibly OCR-ing or summarizing unsupported files unless a preprocessing pipeline is explicitly configured.

## UI Draft Model

The chat input maintains an unsubmitted draft.

```ts
type ChatDraft = {
  text: string
  attachments: DraftAttachment[]
  mode?: "chat" | "plan" | "edit"
}

type DraftAttachment = {
  id: string
  name: string
  mime: string
  size: number
  status: "local" | "uploading" | "ready" | "failed"
  previewURL?: string
  source:
    | { type: "file"; file: File }
    | { type: "clipboard"; file: File }
    | { type: "path"; path: string }
    | { type: "blob"; blobID: string }
  error?: string
}
```

UI Operations:

```ts
interface ChatAttachmentController {
  addFiles(files: File[]): Promise<void>
  addPath(path: string): Promise<void>
  paste(event: ClipboardEvent): Promise<void>
  remove(id: string): void
  submit(): Promise<void>
}
```

## Adding Attachments

### File Selection or Drag-and-Drop

```text
User selects file
  -> UI reads name/mime/size
  -> Local validation of size and type
  -> Generates previewURL
  -> DraftAttachment(status = "local")
```

### Pasting Screenshots

```text
paste event
  -> clipboard item contains image/png
  -> File object
  -> DraftAttachment(source = clipboard)
```

### Local Path Reference

```text
User inputs or drags in a path
  -> DraftAttachment(source = path)
  -> Does not read content on client
  -> Resolved by server based on Location.cwd after submission
```

Design Trade-offs:

- Browser environments cannot arbitrarily read local absolute paths, suitable for upload or blob.
- Desktop/CLI environments can pass paths, but the server must still check path permissions.
- Path references are suitable for large files; dataURLs are suitable for small screenshots and small images.

## Submit Request

Convert the draft into prompt parts upon submission.

```ts
type PromptSubmitRequest = {
  sessionID?: SessionID
  messageID?: MessageID
  text: string
  attachments: SubmittedAttachment[]
  delivery?: "steer" | "queue"
  resume?: boolean
}

type SubmittedAttachment =
  | {
      type: "data"
      id: string
      name: string
      mime: string
      size: number
      dataURL: string
    }
  | {
      type: "path"
      id: string
      name: string
      mime?: string
      path: string
    }
  | {
      type: "blob"
      id: string
      blobID: string
    }
```

Client-side conversion:

```ts
async function buildSubmitRequest(draft: ChatDraft): Promise<PromptSubmitRequest> {
  return {
    text: draft.text,
    attachments: await Promise.all(draft.attachments.map(serializeAttachment)),
  }
}
```

Small files can be converted to data URLs:

```ts
async function fileToDataURL(file: File): Promise<string> {
  return await new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => resolve(String(reader.result))
    reader.onerror = () => reject(reader.error)
    reader.readAsDataURL(file)
  })
}
```

For large files, it is recommended to upload to a local blob store first:

```ts
interface AttachmentBlobStore {
  put(input: {
    name: string
    mime: string
    bytes: Uint8Array
  }): Promise<FileAttachment>

  get(blobID: string): Promise<FileAttachment>
}
```

## Prompt Parts

The server converts the submit request into user message parts.

```ts
type UserPromptPart =
  | { type: "text"; text: string }
  | { type: "attachment"; attachment: FileAttachment }
```

```ts
async function buildPromptParts(request: PromptSubmitRequest): Promise<MessagePart[]> {
  return [
    ...(request.text.trim() ? [{ type: "text", text: request.text }] : []),
    ...(await Promise.all(request.attachments.map(resolveSubmittedAttachment))),
  ]
}
```

## Server-Side Attachment Resolution

```ts
interface AttachmentResolver {
  resolve(input: {
    sessionID: SessionID
    location: Location
    attachment: SubmittedAttachment
  }): Promise<FileAttachment>
}
```

### data URL

```ts
async function resolveDataAttachment(input: DataAttachment): Promise<FileAttachment> {
  const parsed = parseDataURL(input.dataURL)
  assertMimeMatches(input.mime, parsed.mime)
  assertSizeBelowLimit(parsed.bytes.length)
  const sha256 = hash(parsed.bytes)
  return blobStore.put({
    name: input.name,
    mime: parsed.mime,
    bytes: parsed.bytes,
  })
}
```

### path

```ts
async function resolvePathAttachment(input: PathAttachment, location: Location): Promise<FileAttachment> {
  const resolved = await pathPolicy.resolve({ location, path: input.path })
  await pathPolicy.assertReadable(resolved)
  const stat = await fileSystem.stat(resolved.absolute)
  assertSizeBelowLimit(stat.size)
  return {
    id: newID("att"),
    name: input.name,
    mime: input.mime ?? detectMime(resolved.absolute),
    size: stat.size,
    source: { type: "path", path: resolved.absolute },
    createdAt: Date.now(),
  }
}
```

### blob

```ts
async function resolveBlobAttachment(input: BlobAttachment): Promise<FileAttachment> {
  return blobStore.get(input.blobID)
}
```

## MIME to Modality

```ts
type Modality = "text" | "image" | "audio" | "file"

function mimeToModality(mime: string): Modality {
  if (mime.startsWith("image/")) return "image"
  if (mime.startsWith("audio/")) return "audio"
  if (mime.startsWith("text/")) return "text"
  return "file"
}
```

Capability Check:

```ts
function unsupportedParts(input: {
  model: ModelInfo
  parts: MessagePart[]
}): UnsupportedPart[] {
  return input.parts
    .filter((part) => part.type === "attachment")
    .map((part) => ({
      part,
      modality: mimeToModality(part.attachment.mime),
    }))
    .filter((item) => !input.model.capabilities.modalities.input.includes(item.modality))
}
```

If the model does not support it:

- Reject before prompt admission, prompting the user to switch models or remove the attachment.
- Or configure a preprocessor, such as image OCR or PDF text extraction, but the preprocessing results must be explicitly marked as derived text, without pretending the model directly read the original file.

## Image Normalization

Multimodal models often have requirements for image size and format. It is recommended to standardize before entering the model request.

```ts
interface ImageNormalizer {
  normalize(input: {
    attachment: FileAttachment
    maxWidth: number
    maxHeight: number
    outputMime: "image/png" | "image/jpeg" | "image/webp"
  }): Promise<FileAttachment>
}
```

Strategy:

- Keep the original attachment; the normalized result becomes a new blob.
- Scale oversized images by the longest edge.
- Prefer PNG for opaque screenshots, JPEG/WebP for photos.
- Record width/height for UI preview and token estimation.

## Converting to Model Messages

```ts
interface ModelPartBuilder {
  build(input: {
    model: ModelInfo
    part: MessagePart
  }): Promise<ModelMessagePart>
}
```

Conversion Rules:

```ts
async function buildAttachmentPart(input: {
  model: ModelInfo
  attachment: FileAttachment
}): Promise<ModelMessagePart> {
  const modality = mimeToModality(input.attachment.mime)

  if (!input.model.capabilities.modalities.input.includes(modality)) {
    throw new UnsupportedModalityError(modality)
  }

  if (modality === "image") {
    const normalized = await imageNormalizer.normalize({
      attachment: input.attachment,
      maxWidth: 2048,
      maxHeight: 2048,
      outputMime: "image/png",
    })
    return {
      type: "image",
      mime: normalized.mime,
      data: await readAsDataURL(normalized),
    }
  }

  if (modality === "audio") {
    return {
      type: "audio",
      mime: input.attachment.mime,
      data: await readAsDataURL(input.attachment),
    }
  }

  return {
    type: "file",
    mime: input.attachment.mime,
    name: input.attachment.name,
    data: await readAsDataURL(input.attachment),
  }
}
```

The provider adapter can further convert `data` into a provider file upload reference.

## file:// and data URL

It is recommended to distinguish between two semantics internally:

- `file://` or path: Represents a local file reference; the model cannot read it directly; the server must read and convert it.
- `data:`: Represents inline content that can be sent directly to a supported provider, but size/MIME checks are still required.

Do not send local paths directly to the model unless the tool or provider explicitly supports accessing the same file system. Most cloud models cannot see your local paths.

## Media in Tool Results

Tools may also produce multimodal results, such as browser screenshots, image generation, and PDF rendering.

```ts
type ToolExecutionOutput<TOutput> = {
  output: string
  data?: TOutput
  media?: FileAttachment[]
}
```

Whether the model sees this media in the next round depends on:

- Whether the tool result projection places the media into the tool result part.
- Whether the current model supports the corresponding modality.
- Whether the history selection retains that tool result.
- Whether the file size meets provider limitations.

If the model does not support media input, `output` should be written as a sufficiently useful text summary.

## Security Policies

Attachment Security Checks:

- File size limits.
- MIME whitelist or blacklist.
- Paths must pass workspace/root policies.
- Reject image decoding failures; do not trust file extensions.
- Do not parse complex formats like PDF, Office, archives directly in the main process; use isolated workers.
- Do not write attachment content into logs; only save hash, metadata, and blob references.

## Implementation Steps

1. Implement UI draft attachment state and add/remove functionality.
2. Implement three serialization types (data URL, path, blob) upon submission.
3. Implement server-side `AttachmentResolver`.
4. Implement MIME detection, size limits, hashing, and blob store.
5. Convert attachments to message parts before `SessionService.prompt`.
6. Perform model capability checks before constructing the request in the Runner.
7. Implement `ModelPartBuilder` to convert attachment parts to provider-neutral parts.
8. Implement specific mappings for images, PDFs, and audio in the provider adapter.
9. Support tool result media being read by the model in the next round.

## Acceptance Criteria

- The chat input allows adding, previewing, and removing attachments.
- After pasting a screenshot, models that support image input can read the image content.
- Models that do not support image input provide a clear error before submission or execution.
- Local path attachments do not send the path string directly to the cloud model; instead, the content is read and converted.
- Oversized files are rejected or require preprocessing.
- Screenshots generated by tools can be used as multimodal input in the next round.
- Attachment metadata and blobs can still be found after a restart.

## Common Pitfalls

- Sending `C:\path\image.png` to the model as text, assuming the model can read the local file.
- Determining MIME solely by file extension.
- Writing full data URLs to event logs, causing database bloat.
- Sending a PDF base64 to a model that does not support PDF, resulting in provider errors.
- Overwriting the original file during image normalization, losing the user's original attachment.
