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
- A native AI readiness line plus **Save & Test** flow that tells users whether
  Haikode is ready, missing credentials, built without network transport, or
  failing against the configured provider without exposing API keys or OAuth
  tokens.
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
  controls with path checks and unique `.haikode/backups/` copies before
  writes.
- AI patch application refuses sensitive project metadata such as `.git/`,
  `.hg/`, `.svn/`, `.haikode/`, `.genio`, and Haikode/Genio settings files.
- AI patch application refuses symbolic-link path components so a proposed
  patch modifies the named project file instead of writing through a link.
- Generated unified diffs are saved immediately under `.haikode/patches/` before
  approval, so rejected and partially applied proposals still have an audit file.
- Fenced AI diff responses are extracted as the diff body only, so trailing
  model commentary after the code fence is not saved into patch audit files.
- Whole-response JSON patch objects with `unified_diff`, `diff`, or `patch`
  string fields are accepted and routed into the same review/apply flow.
- Fenced `haikode-edit` JSON with `path`, `original_sha256`, and `replacement`
  is accepted for hash-checked single-file replacements, then converted into
  the same reviewable diff path.
- Complete, untruncated file context includes SHA-256 metadata so models can
  produce hash-checked `haikode-edit` proposals reliably.
- Explicit AI command request parsing with separate **Run command** and
  **Reject command** approval buttons.
- Generic fenced JSON blocks with both `summary` and `argv` are also surfaced
  as pending command requests, so OpenCode/Codex-style model output does not
  need Haikode's custom fence label to remain reviewable.
- Generic JSON blocks may also contain a `commands` array of `{summary, argv}`
  objects; each entry becomes a separate pending command request.
- Whole-response JSON command objects and `commands` arrays are accepted too
  when the model returns structured output without a Markdown fence.
- Shell-interpreter command requests such as `sh -c ...` are flagged as not
  runnable inside Haikode and must be reviewed/run manually.
- Destructive argv-native requests such as recursive forced `rm` and disk
  writing tools are flagged even when the model uses path-qualified executables
  such as `/bin/rm` or splits flags across arguments, for example `rm -r -f`.
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
- Selected-file patch previews now use structured review rows with old/new line
  columns and explicit add/remove markers, giving the native panel a clearer
  review model than raw diff text alone.
- A native selected patch-file field so a user can approve or reject one named
  file from a multi-file AI patch instead of blindly accepting the first file.
- Previous/next patch-file navigation immediately refreshes the selected-file
  preview, and hunk navigation can inspect one hunk at a time.
- **Review patch** scopes the AI review prompt to the selected patch file when
  the selected file field is set.
- Project-local `.haikode/sessions/` records for successful AI exchanges,
  storing prompts, responses, provider metadata, active file, and pending
  actions without API keys or bearer tokens. Session records redact key-like
  strings if they appear inside prompts, responses, or pending-action text.
  Saved AI sessions, command requests, logs, patches, and backups use
  collision-resistant timestamped names so rapid repeated actions keep separate
  audit records.
- A project-local `.haikode/project.json` memory file generated from project
  scans, with project name/root, inferred build/test commands when known,
  discovered file language/role/risk/TODO summaries, and no credentials.
- AI prompts reuse `.haikode/project.json` project-map memory when available
  and fall back to an offline scan when memory has not been created yet.
- Project memory command hints, currently inferred from Makefiles, are included
  in prompts so AI can propose relevant approved commands without claiming they
  were run.
- Fresh projects without `.haikode/project.json` still infer simple Makefile
  command hints for prompts, then fall back to the saved memory once a scan has
  created it.
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
in Haikode settings; local mode sends no authorization header. Local providers
still require `HAIKODE_AI_NETWORK=1` because Haikode talks to them over HTTP.
Use **Window > Haikode AI setup** or the **AI Setup** button to paste cloud API
keys inside Haikode, choose a local provider, or configure OAuth token/PKCE
endpoint fields. API key and OAuth token fields are masked in the native UI.
Provider base URLs must use `http://` or `https://`; other libcurl URL schemes
are rejected before any request is sent.
OpenRouter requests also include non-secret app attribution headers
(`HTTP-Referer`, `X-OpenRouter-Title`, and `X-OpenRouter-Categories`) so the
service can identify Haikode traffic without logging credentials.
OAuth auth and token URLs use the same HTTP(S)-only validation, and OAuth
redirect URIs must be local `http://127.0.0.1` or `http://localhost`
callbacks before Haikode opens the browser or exchanges a code.
You do not need to export an API key in Terminal. The setup dialog also reports
whether the current binary was built with network AI support. Then use
**Save & Test** to verify the endpoint before asking questions or proposing
patches.

The next slices should add richer native filtering for project records and keep
iterating on AI/provider setup ergonomics from Haiku QA feedback.
