# Instructions for AI coding agents

QElectroTech (QET) is a free schematic editor written in C++17 with Qt 6 and
built with CMake. Project files are XML. These instructions are for AI agents
working in this repository, and they summarise the project's guides. If a
guide and this file disagree, the guide wins, so read it fresh for the task:

- Any change: [Fix or add with an AI assistant](https://github.com/qelectrotech/qelectrotech-source-mirror/wiki/contribute_with_ai_assistant)
- Anything posted on GitHub: [Posting with an AI assistant](https://github.com/qelectrotech/qelectrotech-source-mirror/wiki/posting_with_ai_assistant)
- Translations: [Translate with an AI assistant](https://github.com/qelectrotech/qelectrotech-source-mirror/wiki/translating_with_ai)
- C++ code: [Code style](https://github.com/qelectrotech/qelectrotech-source-mirror/wiki/code_style)
- Building: [Building from Source](https://github.com/qelectrotech/qelectrotech-source-mirror/wiki/building)
- Testing: [CONTRIBUTING.md](CONTRIBUTING.md)

## The person decides

- The person you work for reads and approves everything before it goes out.
  Never post, comment, close or merge without being told to for that post.
- Stop and ask before you delete files or install software.
- Maintainers want short, plain posts written for the user's point of view.
  Never claim something was tested when it was not.

## Before you change anything

- Search open and closed issues and pull requests. The feature may exist, or
  the bug may already be fixed.
- Reproduce a bug on the current code first.
- Show a short plan (files to change, any new setting) and wait for an OK.
- Work on a branch of the person's fork, made from the current upstream
  `master`.
- The interface and the code use different words: wire = conductor,
  page = folio, symbol = element.

## Rules for every change

- A change to existing behaviour gets an on/off setting in the settings
  dialog, off by default, so existing projects look the same.
- Old project files must still open and look unchanged.
- Anything the user does to a project goes through a `QUndoCommand` on the
  project's undo stack. Merge ids are listed in
  `dev_doc/ID_of_QUndoCommand.txt`.
- Match the code around you. If a file differs from the code style, the file
  wins inside that file.
- Never reformat lines you are not changing. Never run `clang-format -i` on a
  whole file: the `.clang-format` in the repository does not match the code.
  The code style page gives a matching configuration, to use on your own lines
  only.
- Tabs, 80 columns, `Type *name`, `m_` for new members, `enum class` for new
  enums.
- UTF-8 without a byte-order mark and Unix line endings. New files start with
  the licence block from the code style page.
- Make one logical change per commit, and say why in the message.
- Add tests for new functionality, in `tests/qttest` or `tests/catch`.

## Interface text

- Every string the user can see goes through `tr()`, written in English. Never
  write French or another language in the source.
- Use placeholders such as `%1` and `%n`, never glued-together sentences.
- Use the names menus, buttons and dialogs already have. Check the existing
  `tr()` text instead of guessing.

## Build and test

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPACKAGE_TESTS=ON
cmake --build build --parallel
xvfb-run -a ctest --test-dir build --output-on-failure
```

Leave out `xvfb-run -a` if there is a display. Add `-DBUILD_WITH_KF=OFF` if KDE
Frameworks are not available. Use fewer parallel jobs if the build runs out of
memory. For Microsoft's compiler, run the tests in `RelWithDebInfo`, not
`Debug`.

## Check before a pull request

For a code change, run [qet-verify](https://github.com/ispyisail/qet-verify) on
your last commit and fix every `FAIL`. A `SKIP` is not a pass.

```sh
../qet-verify/verify.sh --qet-dir <QET folder> --binary <built qelectrotech>
```

Then try the change on a real project, and review your own diff the way a
maintainer would. A change the user can see needs a before and an after
screenshot, taken from the same build with one thing changed. Use an example
project, never a customer's or an employer's drawing.

## Translations

- Updating a language touches only `lang/qet_<code>.ts`. A new language touches
  five places: follow the table in the translation guide.
- Run `lupdate sources -ts lang/qet_<code>.ts -source-language en_US` from the
  current `master`. For a new language, add `-target-language <code>`, as the
  translation guide does.
- Keep placeholders, HTML tags, links, the `&` of Alt-key letters and a
  trailing `...` or `…`.
- Leave every translation you write marked `type="unfinished"`. The person
  marks a string done after reading it.
- Run the check with `qet_ts_tool.py` from the translation guide, and
  `lrelease`.
- Never commit a `.qm` file. The build makes it.

## Pull requests

- Open it against `master` of `qelectrotech/qelectrotech-source-mirror`, and
  follow `.github/pull_request_template.md`.
- Start the title with a verb such as Fix, Add or Document. Spell out
  abbreviations, and use no `feat:` or `fix:` tags.
- Stay within the length limits of the posting guide, and include everything
  it requires of a description, such as the qet-verify receipt pasted exactly
  as the script printed it.
- State the build configuration, the commands actually run, their results, and
  anything skipped or not tested.
- Say that an AI assistant helped, and who read the result. For a translation,
  say how many strings are reviewed and how many are still unfinished.
- After a review, push new commits. Do not force-push an open pull request.

## Working notes: what the guides leave out

Kept for this working copy, on top of the instructions above. The guides win
where the two disagree. These are the parts the guides assume and do not say.

### Build

Out-of-source builds only. Non-obvious requirements:

- **Qt6 GuiPrivate headers are mandatory** — configure dies with FATAL_ERROR
  without them. Debian/Ubuntu: `qt6-base-private-dev` (not pulled in by
  `qt6-base-dev`); Fedora: `qt6-qtbase-private-devel`.
- Also required: Qt6 Svg, Sql, PrintSupport, Concurrent, LinguistTools
  (`lrelease` runs on every build) and the system SQLite3 dev package.
- QtPdf (PDF page import) and Qt Qml (JS scripting, `--run`) are optional and
  silently disable features when absent — read the configure output.
- `BUILD_WITH_KF=ON` (default) fetches and *builds* ECM + kcoreaddons +
  kwidgetsaddons from KDE Git at pinned commits. `-DBUILD_KF=OFF` uses system
  KF6 instead; `-DBUILD_WITH_KF=OFF` drops KF entirely (bundled `sources/ui/nokde`).
- First configure needs network: Catch2, pugixml, SingleApplication, KF6 arrive
  via FetchContent pinned to commit hashes. Populated submodules are used as-is
  (offline builds).
- `build/` is NOT in `.gitignore` — never commit it.

Per-distro dependency lists: INSTALL.md. Canonical CI commands:
.github/workflows/linux-build.yml (Ubuntu 26.04 container, Qt 6.10, Ninja, Debug).

### Source layout — the CMake gotcha

- Entrypoint `sources/main.cpp` → `QETApp` (`sources/qetapp.cpp`); everything
  else compiles once into the `qelectrotech_objects` OBJECT library, which the
  executable and app-level tests both link.
- **Nothing is globbed.** New files must be listed by hand:
  - `.cpp`/`.h` → `QET_SRC_FILES` in `cmake/qet_compilation_vars.cmake`
  - `.ui` → `QET_RES_FILES` in the same file (AUTOUIC search path: `sources/ui`)
  - resources → `qelectrotech.qrc` or `ico/icon-themes.qrc`
  - a new translation language → `TS_FILES` in `cmake/qet_compilation_vars.cmake`
  Forgetting this builds in Qt Creator but silently omits the file from CMake builds.

### Tests

New QtTest tests are registered **by hand** in `tests/qttest/CMakeLists.txt`
(also explicit, no glob). Patterns used there, in escalating cost:

- header-only helper → `add_executable(tst_x tst_x.cpp)` + `Qt::Test` only
- a small `.cpp` under test → list that source file in the test target
- needs the app (folio, view, editor window) → `qet_add_app_test(tst_x tst_x.cpp)`;
  links `qelectrotech_objects`, runs `QT_QPA_PLATFORM=offscreen` with its own settings
- CLI regression → `add_dependencies(tst_x qelectrotech)` +
  `target_compile_definitions(tst_x PRIVATE QET_TEST_BINARY_PATH="$<TARGET_FILE:qelectrotech>")`,
  driving the real binary (`--export-*`, `--resave`, `--run`)
- scripting tests exist only inside `if(QET_HAS_SCRIPTING)` (Qt6 Qml present)
- the Python probe tests (`run_*.py`) are registered only with the **Ninja**
  generator and Python3 found

```sh
ctest --test-dir build -C Debug --output-on-failure
ctest --test-dir build -C Debug -R '^tst_qetstrings$' --output-on-failure
xvfb-run -a ctest --test-dir build --output-on-failure   # headless Linux
```

Debug is not optional for the crash regressions: they reproduce only with the
Debug allocator, never at `-O3`. Run Microsoft's tests in `RelWithDebInfo`, not
`Debug`. Never build while the suite runs: an overwritten binary makes tests
fail for no reason of theirs.

Not run by CTest — execute separately:

- `./build/tests/catch/C_unittests` (Catch2; new files go in `tests/catch/CMakeLists.txt`)
- `python3 misc/qet-mcp/test_qet_mcp.py`
- `tests/ipc-regression/run.sh --binary build/qelectrotech` — needs Xvfb,
  openbox, xdotool; starts its own display, do **not** wrap in `xvfb-run`.
  Exit 2 = inconclusive, which is not a pass.
- `tests/modal-quit-regression` (CTest-registered on Linux): needs gdb with
  Python and an unstripped Debug binary; exit 77 = skipped, not passed.

CONTRIBUTING.md ("Testing") is the authoritative long version.

### Verifying a change without the GUI

The binary has a headless CLI (exact usage in `sources/cli_export.h`):
`--export-pdf/-png/-svg/-dxf`, `--export-bom`, `--export-cables`, `--info`,
`--resave` (XML round-trip), `--check-elements <file.elmt>`, `--run <script.js>`.
Use `QT_QPA_PLATFORM=offscreen` on a displayless machine.

`misc/qet-mcp/qet_mcp.py` (Python stdlib only) reads, diffs and edits `.qet`
projects — e.g. `misc/qet-mcp/qet_mcp.py --call qet_diff '{"path": "x.qet"}'`
shows what an edit actually changed in the model; prefer it to screenshots.

### Conventions

- Style: `.clang-format` at the root (tabs, width 4, Allman braces, 80 cols);
  CONTRIBUTING.md describes the same style. Nothing enforces it in CI — never
  reformat code you did not touch.
- Comments here are long "why this exists" documents citing issue/PR numbers;
  match that register and do not trim them as noise.
- Optional features follow one pattern: probe at configure time, define
  `QET_HAS_*` (`QET_HAS_QTPDF`, `QET_HAS_SCRIPTING`, `QET_CRASH_BACKTRACE`) and
  guard in code.
- Dependency pins live in `cmake/fetch_*.cmake`; their comments explain why tags
  are not trusted and how to bump them — read before changing.
- Commits: small, with real log messages (CONTRIBUTING.md).

### Reference docs

- CONTRIBUTING.md — testing on Linux/Windows/macOS, commit expectations
- INSTALL.md — per-distro dependencies and build options
- misc/qet-mcp/README.md — MCP server and its testing environment variables
- tests/ipc-regression/README.md, tests/modal-quit-regression/README.md
- cliff.toml — git-cliff changelog generation
