# Cloud Sync for AI Configuration

## Purpose

Allow a signed-in user to carry AI configuration across machines and projects.

## User Experience

After logging in, the user can choose to sync:

- model profiles
- MCP server definitions
- rules
- skills
- NEXT preferences

When they open NextEngine on another machine, these settings are available without manual reconfiguration.

## Existing Basis

- `editor/user_system` provides user session and authentication client code.
- AI model, MCP, rules, and skills settings already exist locally.
- Settings pages already provide structured data that can be serialized.

## Proposed Design

Add a user-scoped AI settings sync layer.

Sync should be opt-in and transparent:

- local settings remain the source when sync is disabled
- user can push local settings to cloud
- user can pull cloud settings
- conflicts are shown before overwrite

## Data Categories

Recommended sync categories:

- model profile metadata, excluding raw API keys unless secure secret sync exists
- MCP server display config, with sensitive environment values handled carefully
- rules
- prompt/context skills
- UI preferences

## Security Rules

- Do not upload API keys silently.
- Mark sensitive fields clearly.
- Prefer local secret storage unless there is an approved encrypted sync design.
- Let users review what will be synced.

## Acceptance Criteria

- Logged-in user can export/import AI settings through the user system.
- Sync does not break offline use.
- Sensitive fields are excluded or explicitly confirmed.
- Settings schema version is stored for future migrations.

## Risks

- Syncing credentials incorrectly is a serious security issue.
- Cloud and local settings conflicts can confuse users if not shown clearly.

## First Implementation Step

Implement a local "AI settings bundle" serializer first. Cloud transport can use that bundle later.
