# CHML — This should not exist. Unfortunately, it works.

[![linux-verify](https://github.com/LeakTechnologies/CHML/actions/workflows/linux-verify.yml/badge.svg)](https://github.com/LeakTechnologies/CHML/actions/workflows/linux-verify.yml)

Experimental C-native web authoring prototype. Write HTML document structure and CSS style declarations as **ordinary ISO C**; generate browser-native `index.html` and `style.css`.

**Status:** Early proof of concept, not a fully compliant web platform. The current build targets C11; C23 is a long-term goal. No JavaScript or WebAssembly is required for this static example.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
mkdir -p output
cd output
../build/chml_hello
```

Open `output/index.html` in a browser. `output/style.css` is generated from C declarations, not a handwritten CSS file.

## Verification

```sh
scripts/linux/dev-verify.sh        # Linux: GCC, Clang, ASan, UBSan, POSIX path
scripts\windows\dev-verify.ps1     # Windows: GCC, analyzer, Win32 path
```

The Linux gate also runs in CI on every push and pull request to `master`
(`.github/workflows/linux-verify.yml`), on Ubuntu 24.04 with GCC 13.3.0 and
Clang 18.1.3.

Both scripts refuse to install dependencies, use isolated build directories, and
exit non-zero when a required step was skipped, so a partial run cannot look
like a pass. `ctest` currently reports 6 tests; one of them,
`chml_atomic_identity_negative`, is **expected to fail by design** and is
registered with `WILL_FAIL`. It proves that the suite can tell atomic
replacement from in-place rewriting.

## Documentation

- [Architecture and limitations](docs/architecture.md)
- [Compatibility matrix](docs/compatibility-matrix.md)
- [Milestone roadmap](docs/roadmap.md)
- [Official references](docs/references.md)
- [ADR-0001: C-first](docs/decisions/ADR-0001-c-first.md)
- [ADR-0002: Declarative-first hybrid](docs/decisions/ADR-0002-declarative-hybrid.md)
- [ADR-0003: Ownership and validation boundaries](docs/decisions/ADR-0003-ownership-validation.md)
- [ADR-0004: Components as ordinary C functions (proposed)](docs/decisions/ADR-0004-components-as-c-functions.md)
- [Changelog](CHANGELOG.md)

## Security and compatibility notice

This prototype is **not production-ready**. HTML validation and CSS parsing are incomplete. Never assume the current output is safe for untrusted content or equivalent to the HTML/CSS Living Standards. Browser interoperability and performance have not yet been measured.

## Agent handoff

Read [`AGENTS.md`](AGENTS.md) for OpenCode/agent onboarding. M0.4 has only partial validation hardening; it is **not** complete.
