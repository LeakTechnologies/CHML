# CHML done log

## 2026-10-27 — M0.4 hardening increment 2

Done:

- Imported the M0.4 handoff package as the repository baseline (first commit).
- Reproduced the GCC build and the 2/2 test result under stricter warnings than
  originally reported.
- Fixed case-insensitive void-element content rejection.
- Fixed duplicate attribute name acceptance.
- Added regression tests for both, verified non-vacuous by negative control.
- Added `.gitignore`, `CHANGELOG.md`, `DONE.md`, and proposed ADR-0004.

Deliberately not done (needs a decision, not a patch):

- Raw-text element serialization policy (`script`/`style`).
- Character-encoding declaration in generated documents.
- Atomic/transactional output writing.
- CSS grammar, at-rules, and cascade model.
- Reusable-component API (see ADR-0004; proposal only).
- Cross-browser, accessibility, and payload measurements.

Verification commands used:

```
cmake -S . -B build-gcc -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes"
cmake --build build-gcc
ctest --test-dir build-gcc --output-on-failure
gcc -std=c11 -fanalyzer -Wall -Wextra -I include -c src/document.c
gcc -std=c11 -fanalyzer -Wall -Wextra -I include -c src/style.c
```

Result: build clean, 2/2 tests pass, static analyzer clean.
