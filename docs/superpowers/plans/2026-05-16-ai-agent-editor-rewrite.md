# AI Agent Editor Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the old mixed AI/MCP implementation with a self-contained Godot Editor AI Agent subsystem.

**Architecture:** Keep AI code under `editor/ai_component`, split into Agent, Provider, Context, Storage, Prompt, and UI layers. Remove MCP and the runtime HTTP server/router additions from editor/scene/core registration and builds.

**Tech Stack:** Godot Engine C++, EditorDock UI, EditorSettings, HTTPClient, JSON, SCons.

---

## File Structure

- Modify `editor/ai_component/SCsub` to compile only the new subdirectories.
- Modify `editor/editor_node.h/.cpp` to use `AIAgentDock` and `AIAgentSettingsDialog`.
- Modify `editor/register_editor_types.cpp` to register new AI classes and remove MCP registrations.
- Modify `scene/register_scene_types.cpp` and `core/register_core_types.cpp` to remove old MCP HTTP server/router registrations.
- Delete old MCP/server/router source files that were auto-compiled by wildcard SCsubs.
- Add:
  - `editor/ai_component/agent/*`
  - `editor/ai_component/providers/*`
  - `editor/ai_component/context/*`
  - `editor/ai_component/storage/*`
  - `editor/ai_component/prompts/*`
  - `editor/ai_component/ui/*`

## Tasks

### Task 1: Remove MCP and Runtime HTTP Surface

- [ ] Remove MCP includes/registrations from `editor/register_editor_types.cpp`.
- [ ] Remove HTTP server includes/registrations from `scene/register_scene_types.cpp`.
- [ ] Remove HTTP router includes/registrations from `core/register_core_types.cpp`.
- [ ] Stop compiling `editor/ai_component/mcp`.
- [ ] Delete old auto-compiled runtime HTTP/MCP files from `scene/main` and `core/io`.
- [ ] Verify `rg "MCPHttpServer|MCPToolRegister|McpRouter"` shows no active compile references.

### Task 2: Add Agent Core

- [ ] Add message/state types.
- [ ] Add `AIAgentSession` for conversation state and request lifecycle.
- [ ] Add `AIAgentRunner` for context preparation and Provider coordination.
- [ ] Emit clear request, stream, completion, and error signals.

### Task 3: Add Provider Layer

- [ ] Add Provider config and abstract Provider.
- [ ] Add SSE parser.
- [ ] Add OpenAI-compatible streaming provider without tool injection.
- [ ] Support cancel and timeout.

### Task 4: Add Read-Only Context

- [ ] Add context document and provider base.
- [ ] Add project tree provider with depth/count/length limits.
- [ ] Add text file provider with `res://` and size restrictions.
- [ ] Add editor context provider for safe metadata.

### Task 5: Add Storage

- [ ] Add conversation serializer.
- [ ] Add conversation store using `user://ai_agent/conversations`.
- [ ] Save after user message and assistant completion.

### Task 6: Add New UI

- [ ] Add new dock, conversation list, message list, message bubble, composer, and settings dialog.
- [ ] Wire send/cancel/model selection to `AIAgentSession`.
- [ ] Display missing API key and request errors clearly.

### Task 7: Integrate and Verify

- [ ] Replace old AI types in `EditorNode`.
- [ ] Update AI layout key usage.
- [ ] Run `scons platform=windows`.
- [ ] Commit implementation.
