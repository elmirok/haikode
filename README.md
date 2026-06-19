# Haikode

Native AI coding assistant for Haiku OS.

## Status

MVP / experimental.

Haikode is a native C++ Haiku coding workshop. It opens a local project folder,
keeps project memory under `.haikode/`, lets you inspect files, asks an
OpenAI-compatible assistant about selected code, previews generated patches, and
runs build/test commands only after approval.

## Features

- Open project folder with a native folder picker.
- Browse project files in a native tree.
- Preview text files read-only.
- Scan project files and write local metadata.
- Save `.haikode/project.json` project memory.
- Ask AI about selected files with Haiku-aware prompts.
- Generate and preview unified diffs.
- Save patches under `.haikode/patches`.
- Apply patches only after approval.
- Create backups under `.haikode/backups` before patching.
- Run build/test commands only after confirmation.
- Save command logs under `.haikode/logs`.
- Optional BFS `haikode:*` attributes on Haiku.
- Optional Codex CLI bridge for supervised read-only sessions.

## Build On Haiku

Default native/offline build:

```sh
make clean
make
make run
```

This build lets you open folders, scan projects, preview files, apply patches,
and run approved build/test commands. AI is disabled in this mode; click
**AI Setup** in the app for the same instructions shown below.

Run core tests:

```sh
make test
```

Optional OpenAI-compatible network build:

```sh
pkgman install curl_devel jsoncpp_devel openssl_devel
make clean
make AI_NETWORK=1
export HAIKODE_API_KEY=your_api_key
make run
```

Optional Codex bridge build:

```sh
make clean
make CODEX_BRIDGE=1
make run
```

The default build links only Haiku native kits:

```text
-lbe -ltracker
```

The network build additionally links curl, jsoncpp, OpenSSL, and Haiku network:

```text
-lcurl -ljsoncpp -lssl -lcrypto -lnetwork
```

## Configuration

Project memory is stored inside the opened project:

```text
.haikode/
  project.json
  sessions/
  notes/
  patches/
  logs/
  backups/
```

For `AI_NETWORK=1`, API keys are read from environment variables:

```sh
export HAIKODE_API_KEY=...
```

Fallback:

```sh
export OPENAI_API_KEY=...
```

API keys are never stored in `.haikode/project.json`.

Generic OAuth/provider settings remain under:

```text
~/config/settings/Haikode/provider.conf
~/config/settings/Haikode/token.conf
```

Codex bridge settings:

```text
~/config/settings/Haikode/codex.conf
```

Example:

```text
codex_path=codex
```

## Safety Model

Haikode is supervised by default:

- It never applies AI changes automatically.
- It saves generated patches before preview/apply.
- It validates patch paths before apply.
- It rejects absolute paths and `../` traversal.
- It rejects ignored and binary file patching in the MVP.
- It creates backups before modifying files.
- It never runs commands automatically.
- It shows a confirmation before build/test commands.
- Dangerous command patterns show stronger warnings.
- It never stores API keys in project files.

## Limitations

- Diff preview is raw unified diff text.
- The internal patch applier targets simple text-file diffs.
- Multi-file patches are validated and can apply, but large/risky patches should
  be reviewed carefully.
- AI calls require `AI_NETWORK=1` and provider packages.
- Codex bridge requires a working `codex` command at runtime.
- Native Haiku GUI QA must be run on Haiku.

## Roadmap

See [ROADMAP.md](ROADMAP.md).
