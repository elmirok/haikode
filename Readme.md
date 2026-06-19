# Haikode

Haikode is a downstream fork of [Genio](https://codeberg.org/Genio/Genio), the
native Haiku IDE. The fork keeps Genio's native editor, project browser, Git,
LSP, terminal, and settings foundation, then adds supervised AI coding workflows
for cloud or local OpenAI-compatible LLMs.

See [HAIKODE.md](HAIKODE.md) for the fork direction and AI/vibecoding safety
model.

## Upstream Genio

![Screenshot](artwork/screenshot/Genio-screenshot-5.0.png)
![Screenshot-Dark](artwork/screenshot/Genio-screenshot-5.0-dark.png)

## Introduction

Genio is a native and fully functional IDE for the [Haiku operating system](https://www.haiku-os.org)

Some of the features of the Genio IDE are:

* LSP Server support:
  * autocompletion
  * signature help
  * go to definition/implementation/declaration
  * find references
  * quick fix
  * code formatting
* Multi-project browser
* Customizable workspace
* Integrated source control with GIT (including opening a remote project)
* Find in files
* Links to file and build errors in Build Log and Console I/O
* Symbols outline view
* "Problems" tab
* Integrated terminal
* Build on save / Save on build
* User templates with placeholders for quickly creating new files and projects
* Rich editor with many features:
  * Multiple tabs
  * Syntax highlighting for many languages
  * Highlight/Trim whitespace
  * Comment/uncomment lines
  * Duplicate current line
  * Delete lines
  * Switch between source and header
* Full screen and Focus mode
* Scripting support (with hey)

Genio started off as a fork of [Ideam](https://github.com/AmosCaster/ideam), and
 the editor is based on [Scintilla for Haiku](https://sourceforge.net/p/scintilla/haiku/ci/default/tree/).

We also took inspiration and code from the editor [Koder](https://github.com/KapiX/Koder).

* strongly recommended for full Genio experience (autocompletion, jump to definition, etc):
  * gcc_syslibs_devel
  * llvm17_clang (or a newer version; tested with llvm22_clang as today)

```bash
pkgman install gcc_syslibs_devel cmd:clang
```

## Goals and roadmap

Genio aims to be an easy, simple yet powerful IDE for Haiku inspired by VS Code and Nova.

Planned features are:

* Integrated debugging
* Implement a Plug-in architecture
* Compiler error parser

## Configuring LSP

For more advanced IDE features, Genio implements the [LSP protocol](https://microsoft.github.io/language-server-protocol/)

* For C and C++ projects you can use clangd. See [Configuring-clangd-lsp.md](Configuring-clangd-lsp.md)
* For Python projects you can install and use [Python LSP Server](https://github.com/python-lsp/python-lsp-server)
* For C# projects you can install and use [OmniSharp](https://github.com/nexus6-haiku/omnisharp-roslyn-haiku)

## Building Genio

### Prerequirements

Genio requires Scintilla and Lexilla to implement various functionalities.
It also requires libgit2 to implement Git features, libyaml_cpp to read yaml files and
editorconfig_core_c to provide support for project wide .editorconfig settings.
The needed development files are available in `libgit2_1.9_devel`, `lexilla_devel`, `yaml_cpp0.8_devel`, `openssl3_devel`
and `editorconfig_core_c_devel` respectively. To install, open a terminal and type:

```bash
pkgman install libgit2_1.9_devel lexilla_devel yaml_cpp0.8_devel editorconfig_core_c_devel openssl3_devel lsp_framework
```

If you would like to try a clang++ build:

* Install `llvm17_clang` and `llvm17_lld` hpkgs from HaikuPorts
* Set `BUILD_WITH_CLANG` to `1` in `Makefile`

### Compiling

Execute `make deps && make` in Genio's top directory.
The executable is created in `app` subdirectory.
After building, copy the `genio_terminal_addon` from libs/terminal into the `data` subdirectory.
Genio can also be built within Genio itself.

## Contributions

We gladly accept contributions, especially for bug fixes. Feel free to submit PRs.
For code contributions, prefer Haiku API over posix, where applicable.
We (try to) stick to the Haiku style for code, although we are a bit less strict sometimes.

## License

Genio is available under the MIT license. See [License.md](License.md).
Outline LSP icons are taken from Visual Studio Code and released under Creative Commons license.
See [Visual Studio Code - Icons](https://github.com/microsoft/vscode-icons)
