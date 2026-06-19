# Haikode Fork Direction

Haikode is moving from a scratch prototype to a downstream fork of
[Genio](https://codeberg.org/Genio/Genio), the native Haiku IDE.

## Goal

Keep Genio's editor, project browser, Git, LSP, terminal, and Haiku-native UI,
then add supervised AI coding workflows:

- Cloud OpenAI-compatible providers with API key auth.
- Provider-configurable OAuth where supported.
- Local OpenAI-compatible providers such as Ollama or llama.cpp servers.
- Read-only questions about the active project, file, or selection.
- Patch proposal as unified diff.
- Explicit patch review and apply.
- Command requests that are never run automatically.

## Safety

Haikode should feel like Codex/OpenCode for Haiku, but remain supervised:

- AI may propose code, not silently write it.
- AI may request commands, not silently run them.
- All patch paths must stay inside the active project.
- Sensitive files and generated artifacts should require stronger warnings.
- Credentials are machine-local settings, not project files.

## Current Branches

- `main`: current public scratch MVP until the Genio fork is promoted.
- `legacy/scratch-mvp`: backup of the scratch MVP.
- `codex/genio-haikode`: Genio-based fork work.

## Current Integration Slice

The Genio-based branch now adds:

- `src/ai/` as a pure C++ AI/vibecoding core.
- `src/ui/AIChatPanel.*` as a native bottom panel.
- A **Window > Haikode AI** action that opens the panel.
- Provider fields for base URL, model, auth mode, API key, and OAuth token.
- Prompt preview for active project, selected file, and selected text.

The panel currently previews the provider endpoint and generated prompt. The
next slice should add the OpenAI-compatible HTTP transport for cloud and local
providers, then connect responses to diff preview/apply.
