# Bareos – Copilot Coding Agent Instructions

## What This Repository Is
Bareos (Backup Archiving REcovery Open Sourced) is a cross-platform, AGPLv3-licensed enterprise backup solution. It consists of a Director daemon (`dird`), Storage Daemon (`stored`), File Daemon (`filed`/client), a PHP web UI (`webui/`), a Python module (`python-bareos/`), and a FastAPI REST API (`restapi/`). The primary languages are **C++20**, **Python**, **PHP**, and **CMake**.

## Repository Layout
```
core/src/         – C++ source (dird/, filed/, stored/, lib/, cats/, plugins/, tests/)
core/src/include/ – Global headers (bareos.h, config.h)
systemtests/      – Integration/system tests (require running daemons + PostgreSQL)
webui/            – PHP/Laminas web interface
python-bareos/    – Python module (bareos/)
restapi/          – FastAPI REST API (bareos_restapi/)
devtools/         – Developer tooling scripts and pip-tools
cmake/            – CMake modules
debian/           – Debian packaging files
.clang-format     – C/C++ formatting (Google-based, 80-col limit)
.cmake-format.py  – CMake formatting (80-col, 2-space indent)
```

## Building (Linux)
Always build from the **repository root** (not `core/`). In-source builds are rejected.

```bash
# Full build with PostgreSQL (director + storage daemon):
cmake -S . -B cmake-build -G Ninja -DENABLE_GRPC=yes -Ddocs-build-json=yes -Dtraymonitor=on -DENABLE_SANITIZERS=yes

# Build (parallel):
cmake --build cmake-build --parallel
```

Key CMake options:
| Option | Effect |
|---|---|
| `-DENABLE_SANITIZERS=yes` | Enable ASan/UBSan (use with `ASAN_OPTIONS=intercept_tls_get_addr=0`) |

**Requirements**: CMake ≥ 3.17, GCC or Clang with C++20 support, OpenSSL ≥ 1.1.1.

## Running Tests

```bash
# Unit tests only (fast, ~6 seconds, no daemons needed):
ctest --test-dir cmake-build --parallel --exclude-regex "system:" --output-on-failure

# System tests require a full build with PostgreSQL and will start the daemons as required
cd cmake-build && ctest --test-dir cmake-build --parallel --exclude-regex "webui:readonly" --output-on-failure
```
Unit tests use **GoogleTest** and are in `core/src/tests/`. System tests are in `systemtests/tests/`.

## Code Style & Linting

All changed files **must** pass `bareos-check-sources` before merging. Install it once:

```bash
cd devtools && ./install-pip-tools.sh
```

Then run from the repo root (checks uncommitted files):
```bash
bareos-check-sources --diff      # show what would change
bareos-check-sources --modify    # auto-fix formatting
```

What it enforces:
- **C/C++**: `clang-format` (Google style, 80-col, see `.clang-format`)
- **CMake**: `cmake-format` (80-col, 2-space tabs, see `.cmake-format.py`)
- **Python**: `black` formatter
- **PHP** (webui only): PSR-12 via `devtools/php-cs-fixer/run-php-cs-fixer.sh`
- Copyright headers, no trailing whitespace, no DOS line endings, no merge conflict markers

## Copyright Headers
Every new source file **must** include an AGPLv3 copyright header. Use the year of creation. See any existing `.cc` or `.py` file for the canonical header format. The `bareos-check-sources` tool will flag missing or incorrect headers.

## Key Source Locations
| Component | Entry Point |
|---|---|
| Director | `core/src/dird/dird.cc` |
| Storage Daemon | `core/src/stored/stored.cc` |
| File Daemon | `core/src/filed/filed.cc` |
| bconsole (CLI) | `core/src/console/` |
| Shared library | `core/src/lib/` |
| DB/catalog layer | `core/src/cats/` |
| Plugins | `core/src/plugins/{dird,filed,stored}/` |
| Unit tests | `core/src/tests/` |

## CI / Validation Pipeline
GitHub Actions are in `.github/workflows/`. The macOS package build workflow (`build-macos.yml`) is manually triggered. The main validation is done via an external Jenkins/CDash instance (cdash.bareos.org).

Before any PR is merged, these checks must pass locally:
1. `cmake --build cmake-build` – zero errors (warnings are errors: `-Werror`)
2. `ctest` – all tests pass including systemtests
3. `bareos-check-sources --since-merge` – no formatting or copyright violations

## Vue WebUI (webui-vue)

The `webui-vue/` directory contains a Vite/Vue SPA. Its compiled output is
committed under `webui-vue/dist/` and served directly by Apache.

**Always rebuild `dist/` before committing any change to `webui-vue/src/`
or `webui-vue/vite.config.js`**, otherwise the deployed app will not reflect
your changes:

```bash
cd webui-vue
npm install   # only needed the first time or after package.json changes
npm run build
cd ..
git add webui-vue/dist/
```

**Never run `npm run build` with `VITE_*` environment variables set**
when producing the committed dist. The system tests temporarily
rebuild dist/ with test-specific ports; always rebuild clean
(no env overrides) before committing. `dist/.build-env` is a
test-runner artefact and is listed in `.gitignore`.

## Important Notes
- **Warnings are errors**: The build uses `-Werror -Wall -Wextra`. Fix all compiler warnings.
- **Never build inside `core/`**: CMake will abort with a fatal error. Always `cmake -S . -B cmake-build` from the repo root.
- **No in-source builds**: The build directory must differ from the source directory.
- **System tests are heavyweight**: They start real daemons and require PostgreSQL. `--exclude-regex "system:"` for development iteration.
- `devtools/pip-tools/` contains the `check_sources`, `pr-tool`, and `changelog_utils` pip packages.
- Files listed in `.bareos-check-sources-ignore` (e.g. vendored code, generated files, `core/src/ndmp/`) are exempt from source checks.
- All tests (unit and systemtests) need to pass before commiting.
- If possible, create constexpression compile-time tests for functions
- All new functions need to have a test. Either a constexrpession compile-time test or a unit test.
- Use c++20 when creating c++ code
- Git commit header (subject line) must not exceed 60 chars
- Git commit body should not have lines longer than 72 chars
- Do not add a `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>` trailer to commits.
