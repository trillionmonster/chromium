# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **Chromium** source repository — the open-source browser engine behind Google Chrome. It is a large, primarily C++ codebase with components in Rust, Java (Android), Objective-C++ (macOS/iOS), and Python (build tooling). The repository uses `gclient` (from depot_tools) for checkout — not `git clone`.

## Build System

Chromium uses **GN** (Generate Ninja) as its meta-build system, which generates **Ninja** build files.

```bash
# Generate build files (do this once, or after changing build args)
gn gen out/Default

# Edit build arguments (opens editor)
gn args out/Default

# Build everything (autoninja auto-selects parallelism)
autoninja -C out/Default chrome

# Build a specific target
autoninja -C out/Default base_unittests

# Check GN dependency correctness
gn check out/Default
```

Key GN concepts:
- Build configuration is in `BUILD.gn` files throughout the tree
- Root build config: `build/config/BUILDCONFIG.gn`
- Build args are declared via `declare_args()` blocks; override in `gn args`
- `//` in GN paths refers to the source root
- `exec_script` is strongly discouraged; additions require allowlisting in `.gn`

## Running Tests

Test targets follow the pattern `<component>_unittests`:

```bash
# Build and run unit tests for a component
autoninja -C out/Default base_unittests
./out/Default/base_unittests

# Run a single test case
./out/Default/base_unittests --gtest_filter="ClassName.TestName"

# Run tests matching a pattern
./out/Default/base_unittests --gtest_filter="*StringUtil*"

# Common test targets
autoninja -C out/Default unit_tests          # Chrome unit tests
autoninja -C out/Default browser_tests       # Chrome browser integration tests
autoninja -C out/Default net_unittests
autoninja -C out/Default content_unittests
autoninja -C out/Default components_unittests
```

Testing uses **Google Test (gtest)** for C++. Test files must use `_unittest.cc` (singular, not `_unittests`).

## Code Review & Submitting Changes

Chromium uses **Gerrit** for code review via depot_tools:

```bash
# Create a new branch
git new-branch my-feature

# Upload a CL for review (runs CheckChangeOnUpload presubmit checks)
git cl upload

# Commit after approval (runs CheckChangeOnCommit checks, verifies tree is open)
git cl commit
```

- Every directory has an `OWNERS` file controlling who can approve changes
- Commits require a `Bug:` field in the description
- Changes to `DEPS` require running `roll-dep` (e.g., `roll-dep src/third_party/foo`)

## Presubmit Checks

`PRESUBMIT.py` at the root runs 60+ automated checks. Key ones to be aware of:

- **No `#pragma once`** — use include guards instead
- **No `<iostream>` in headers** — use `base/logging.h`
- **No `UNIT_TEST` in source files** — test code goes in `*_unittest.cc`
- **No production code calling test-only functions**
- **No relative includes** — use paths from the source root
- **Feature flags must start with `k`** (e.g., `kMyFeature` in `BASE_FEATURE` macros)
- **No hardcoded Google hosts in lower layers** (net/, components/)
- **Security-sensitive changes** require `SECURITY_OWNERS` approval
- **New header files** must have a corresponding `BUILD.gn` change
- **Maximum file size**: large files (>750KB) trigger warnings
- **Path length limit**: paths over 198 chars are rejected (Windows compatibility)

Run presubmit checks locally:
```bash
git cl presubmit
```

## Code Style & Formatting

### C++
- Style: Chromium style (based on Google C++ Style Guide), enforced by `.clang-format`
- C++11 standard formatting, braces always required (`InsertBraces: true`)
- Format: `git cl format` or `clang-format -i <file>`
- Include ordering is strict: Windows headers → C system → C++ stdlib → other libs
- Use `base::Reversed()` instead of `std::ranges::reverse_view`

### Rust
- Edition 2024, style edition 2021
- Third-party code excluded (except `third_party/blink/`)
- Format: `rustfmt`

### Python
- Python 3 exclusively (via `vpython3`)
- Linted with pylint

## Architecture Overview

### Source Organization Principle
Top-level directories are for **products** (Chrome, Android WebView, Ash/ChromeOS). Component code lives in subdirectories. Key layers (bottom to top):

- **`base/`** — Foundational primitives (threading, strings, memory, files). Almost everything depends on this.
- **`net/`** — Networking stack (HTTP, DNS, TLS, QUIC)
- **`gpu/`** — GPU abstraction and command buffer
- **`content/`** — Multi-process browser engine (renderer, browser, GPU process separation). Embeddable by any browser.
- **`components/`** — Shared components used across products (autofill, sync, omnibox, etc.)
- **`chrome/`** — The Chrome browser product itself (UI, features, platform integration)
- **`ui/`** — UI frameworks (views, aura window system, gfx primitives)

### Key Subsystems (as git submodules / subtrees)
- **`v8/`** — JavaScript engine
- **`third_party/blink/`** — Blink rendering engine (HTML/CSS/DOM)
- **`third_party/angle/`** — ANGLE (OpenGL ES to platform graphics translation)
- **`third_party/dawn/`** — WebGPU implementation
- **`third_party/skia/`** — 2D graphics library
- **`net/third_party/quiche/`** — QUIC/HTTP3 implementation
- **`third_party/boringssl/`** — TLS/crypto

### Multi-Process Architecture
Chromium runs as multiple processes: **browser** (main UI + orchestration), **renderer** (per-tab, sandboxed, runs Blink+V8), **GPU** (graphics compositing), **utility** (network, audio, etc.). IPC between processes uses **Mojo** (defined via `.mojom` files).

### Platform Conditionals
The codebase compiles for Linux, macOS, Windows, Android, iOS, Fuchsia, and ChromeOS. GN args like `is_android`, `is_ios`, `is_linux`, `is_win`, `is_chromeos`, `is_fuchsia` control platform-specific compilation. Platform-specific files use suffixes: `_linux.cc`, `_mac.mm`, `_win.cc`, `_android.cc`, `_ios.mm`.

## Dependency Management

Dependencies are managed in the `DEPS` file via gclient. To update a dependency:
```bash
git new-branch depsroll
roll-dep src/third_party/foo_package/src foo_package.git
git commit -a
git cl upload
```

The `DEPS` file supports conditional checkout via variables like `checkout_android`, `checkout_ios`, `checkout_fuchsia`. A `"small"` checkout configuration is available to skip non-essential dependencies.

## AI Usage Policy

Per the Code of Conduct: AI is acceptable for code authoring and bug finding. However, **do not use AI to respond to human code review comments** — the account holder must reply to reviewer feedback personally. The account holder is fully responsible for all AI-authored actions.
