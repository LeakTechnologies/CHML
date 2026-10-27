# CHML done log

## 2026-10-27 — M0.4-B atomic output

Done:

- Shared atomic writer (`src/chml_io.c`), used by both the document and stylesheet
  writers.
- Temporary file beside the destination, opened with C11 exclusive mode `"wbx"`,
  replaced only after a complete, flushed, closed write.
- Failure paths remove the temporary and leave any existing artifact untouched.
- Platform replace: `MoveFileExA` on Windows (ISO C `rename()` cannot replace an
  existing destination there), `rename()` elsewhere.

Verification:

```
cmake -S . -B build-gcc -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes"
cmake --build build-gcc
ctest --test-dir build-gcc --output-on-failure
```

Result: clean strict build, 4/4 tests pass, 8 atomic-write checks pass.

Negative controls:

- Direct in-place write instead of temp+replace: only the file-identity check
  fails. Without that check the suite would have passed a non-atomic
  implementation, which is why it was added.
- Skipping temporary cleanup on a failed replace: both cleanup checks fail.

Environment limitations:

- The POSIX `rename()` branch was not executed; this host is Windows.
- No mid-write fault injection; allocation-failure injection remains outstanding.

## 2026-10-27 — M0.4-A serialization correctness

Done:

- Context-aware text serialization: ordinary, RCDATA (`title`, `textarea`),
  raw text (`script`, `style`, `xmp`, `iframe`, `noembed`, `noframes`).
- Raw-text end-tag hazard rejection per the tokenizer's end-tag-name state,
  including non-hazard lookalikes (`</scriptx>`, trailing `</script`).
- `<plaintext>` rejected; raw-text and RCDATA elements reject element children.
- Semantic attributes: `ChmlAttributeKind`, `CHML_BOOLEAN_ATTR`, ARIA never
  treated as boolean.
- UTF-8 policy: one `<meta charset="utf-8">`, injected only when absent,
  conflicting declarations rejected before opening the destination.
- Root-count overflow guard corrected.
- `docs/compatibility-matrix.md` with status rows and open spec questions.

Verification:

```
cmake -S . -B build-gcc -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes"
cmake --build build-gcc
ctest --test-dir build-gcc --output-on-failure
gcc -std=c11 -fanalyzer -Wall -Wextra -I include -c src/document.c
gcc -std=c11 -fanalyzer -Wall -Wextra -I include -c src/style.c
```

Result: clean strict build, 3/3 tests pass (32 serialization fixtures),
analyzer clean.

Negative controls run: raw-text context, hazard check, boolean emission,
charset injection, ARIA boolean guard, and the plaintext guard. Each was
reverted individually and the matching fixture failed.

Environment limitations:

- Clang is not installed; the Clang half of the build matrix was not run.
- ASan/UBSan runtimes are absent from the MinGW toolchain; no sanitizer result.
- No browser-level verification was performed.

Deliberately not done: atomic output, component builder API, CSS grammar.

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
