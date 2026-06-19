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

- `main`: public Genio-based Haikode fork.
- `legacy/scratch-mvp`: backup of the scratch MVP.
- `codex/genio-haikode`: development branch used while landing AI/vibecoding
  slices.

## Current Integration Slice

The Genio-based branch now adds:

- `src/ai/` as a pure C++ AI/vibecoding core.
- `src/ui/AIChatPanel.*` as a native bottom panel.
- A **Window > Haikode AI** action that opens the panel.
- Provider fields for base URL, model, auth mode, API key, local mode, and
  generic OAuth 2.0 settings.
- Native provider preset buttons for OpenAI, OpenRouter, Ollama, LM Studio, and
  llama.cpp.
- A PKCE OAuth helper that can open a provider authorization URL and exchange a
  pasted authorization code or full callback URL for a bearer token.
- OpenAI-compatible HTTP requests for the active project, active selection, and
  up to 10 open project files, with the active file first.
- A native **Explain file** action that sends the active file or selection with
  Haiku-specific explanation guidance.
- A native **Stop** action for in-flight provider tests, OAuth exchanges, and
  AI prompts. Stopped requests cancel the active network transfer where
  possible, re-enable the UI, and ignore any late provider responses.
- Offline project-map context that summarizes source/docs/build files with
  language, role, risk, line count, and TODO markers before sending prompts.
- The AI output log shows how many project-map entries were included and warns
  when extra entries were omitted.
- A **Summarize project** action that uses project-map context to produce a
  project overview without requiring an active file.
- Unified diff detection in AI responses.
- Explicit **Previous file**, **Next file**, **Apply selected file**,
  **Reject selected file**, **Previous hunk**, **Next hunk**, **Apply hunk**,
  **Reject hunk**, **Review patch**, **Apply patch**, and **Reject patch**
  controls with path checks and `.haikode/backups/` copies before writes.
- AI patch application refuses sensitive project metadata such as `.git/`,
  `.hg/`, `.svn/`, `.haikode/`, `.genio`, and Haikode/Genio settings files.
- AI patch application refuses symbolic-link path components so a proposed
  patch modifies the named project file instead of writing through a link.
- Generated unified diffs are saved immediately under `.haikode/patches/` before
  approval, so rejected and partially applied proposals still have an audit file.
- Explicit AI command request parsing with separate **Run command** and
  **Reject command** approval buttons.
- Shell-interpreter command requests such as `sh -c ...` are flagged as not
  runnable inside Haikode and must be reviewed/run manually.
- Plain shell fenced blocks such as `sh`, `bash`, `shell`, or `console` are
  surfaced as pending manual-review command suggestions, not as runnable
  argv-native commands.
- Approved AI command requests run through Haikode's argv-native process
  capture path inside the active project, return stdout/stderr to the AI
  transcript, and save `.haikode/logs/` records. They are not evaluated through
  shell command strings.
- A native pending-actions summary that lists pending patch files, hunks,
  additions/deletions, new-file markers, and command requests before the user
  approves anything.
- A structured patch preview that starts with a changed-file summary, then shows
  the selected file's hunk headers and added/removed/context lines before any
  apply action.
- A native selected patch-file field so a user can approve or reject one named
  file from a multi-file AI patch instead of blindly accepting the first file.
- Previous/next patch-file navigation immediately refreshes the selected-file
  preview, and hunk navigation can inspect one hunk at a time.
- **Review patch** scopes the AI review prompt to the selected patch file when
  the selected file field is set.
- Project-local `.haikode/sessions/` records for successful AI exchanges,
  storing prompts, responses, provider metadata, active file, and pending
  actions without API keys or bearer tokens.
- A supervised Codex CLI bridge: **Codex status**, **Codex login**, and
  **Ask Codex** prepare argv-native commands for explicit approval. The bridge
  uses Codex's own login/session storage and never reads or persists Codex
  tokens inside Haikode.
- Approved **Ask Codex** requests run through an argv-only capture path, return
  Codex output to the Haikode AI transcript, and reuse the existing diff and
  command proposal parsing flow.
- Captured Codex runs are saved under `.haikode/logs/` with argv, cwd, exit
  status, error, timeout/cancel flags, and output for later inspection.
- A lightweight native `.haikode` record browser lists recent sessions, logs,
  patches, and command request files; selecting a row previews the record in
  the AI transcript.
- A native **Open record file** action opens the selected `.haikode` session,
  log, patch, or command record in Genio's editor for normal inspection,
  copying, and search.
- A native **Project files** picker in the AI panel scans the active project for
  text/source files and opens the selected file in Genio's editor, so users can
  see and select files even if the regular project tree is collapsed or stale.

The panel sends prompts to cloud or local endpoints when built with
`HAIKODE_AI_NETWORK=1`. API-key mode stores the key in Haikode settings and
sends `Authorization: Bearer ...`; OAuth mode stores the exchanged bearer token
in Haikode settings; local mode sends no authorization header.
Use **Window > Haikode AI setup** or the **AI Setup** button to paste cloud API
keys inside Haikode, choose a local provider, or configure OAuth token/PKCE
endpoint fields. API key and OAuth token fields are masked in the native UI.
You do not need to export an API key in Terminal. The setup dialog also reports
whether the current binary was built with network AI support. Then use
**Test provider** to verify the endpoint before asking questions or proposing
patches.

The next slices should make the patch review more visual inside native controls
and add filtering/open-in-editor actions to the record browser.
