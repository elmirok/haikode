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
- Provider fields for base URL, model, auth mode, API key, local mode, and
  generic OAuth 2.0 settings.
- Native provider preset buttons for OpenAI, Ollama, and LM Studio.
- A PKCE OAuth helper that can open a provider authorization URL and exchange a
  pasted authorization code for a bearer token.
- OpenAI-compatible HTTP requests for active project, selected file, and
  selected text.
- Offline project-map context that summarizes source/docs/build files with
  language, role, risk, line count, and TODO markers before sending prompts.
- The AI output log shows how many project-map entries were included and warns
  when extra entries were omitted.
- A **Summarize project** action that uses project-map context to produce a
  project overview without requiring an active file.
- Unified diff detection in AI responses.
- Explicit **Apply first file**, **Reject first file**, **Review patch**,
  **Apply patch**, and **Reject patch** controls with path checks and
  `.haikode/backups/` copies before writes.
- Generated unified diffs are saved immediately under `.haikode/patches/` before
  approval, so rejected and partially applied proposals still have an audit file.
- Explicit AI command request parsing and a separate **Run command** approval
  button.
- Shell-interpreter command requests such as `sh -c ...` are flagged as not
  runnable inside Haikode and must be reviewed/run manually.
- A native pending-actions summary that lists pending patch files, hunks,
  additions/deletions, new-file markers, and command requests before the user
  approves anything.
- A structured patch preview that prints each changed file, hunk header, and
  added/removed/context lines before any apply action.
- Project-local `.haikode/sessions/` records for successful AI exchanges,
  storing prompts, responses, provider metadata, active file, and pending
  actions without API keys or bearer tokens.

The panel sends prompts to cloud or local endpoints when built with
`HAIKODE_AI_NETWORK=1`. API-key mode stores the key in Haikode settings and
sends `Authorization: Bearer ...`; OAuth mode stores the exchanged bearer token
in Haikode settings; local mode sends no authorization header.
Use **Window > Haikode AI setup** or the **AI Setup** button to paste cloud API
keys inside Haikode, choose a local provider, or configure OAuth token/PKCE
endpoint fields. API key and OAuth token fields are masked in the native UI.
You do not need to export an API key in Terminal. Then use **Test provider** to
verify the endpoint before asking questions or proposing patches.

The next slice should improve patch review from text output into a richer
native per-file/per-hunk UI, then add a more complete Codex/OAuth bridge where
Codex CLI is available on Haiku.
