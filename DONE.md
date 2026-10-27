# CHML done log

## 2026-10-27 — M0.4-D2 Linux verification execution (blocked before execution)

Done:

- `.github/workflows/linux-verify.yml`: runs `scripts/linux/dev-verify.sh` on
  `ubuntu-latest` for pushes and pull requests to `master`, plus manual dispatch.
  Least privilege (`contents: read`), no third-party actions, logs uploaded even
  on failure, and the gate conclusion surfaced explicitly.
- Negative control strengthened: `atomic_identity_test` now prints
  `NEGATIVE_CONTROL_REASON=identity_unchanged|harness_error|none`, and both
  scripts require exit status 1 **and** the `identity_unchanged` marker. A crash
  or unrelated error can no longer satisfy the check.
- Remote `main` removed after verifying the four preconditions.

Blocked:

- The workflow was pushed and triggered, and GitHub **never started the job**:

  ```
  The job was not started because recent account payments have failed or your
  spending limit needs to be increased. Please check the 'Billing & plans'
  section in your settings
  ```

  Run: https://github.com/LeakTechnologies/CHML/actions/runs/40547489801
  Conclusion: `failure` after 3 s, job not started.

Diagnosis (read-only):

- `repos/.../actions/permissions` → `{"enabled":true,"allowed_actions":"all"}`.
  Actions is enabled at repository level; policy is not the blocker.
- Repository visibility is **private**. Private repositories consume paid
  minutes; the account's payment or spending limit is the blocker.
- Billing endpoints require `admin:org` and were not queried further.

Consequence: **no Linux, Clang, ASan, UBSan or POSIX evidence was produced.**
M0.4-D remains open. Nothing was substituted for real execution.

Branch housekeeping:

| Check | Result |
| --- | --- |
| `master` is the default branch | yes (API) |
| `main` holds no unique work | yes: one file, `README.md`, content `# CHML` |
| Open PRs depending on `main` | none (API returned an empty list) |
| Branch protection on `main` | **not determinable.** The API returned 403 "Upgrade to GitHub Pro or make this repository public". That means the query failed, not that protection was verified absent. |
| Initial commit merged into `master` | yes, `9ae8d1c` is an ancestor |

`main` was deleted on the basis of the three verified conditions plus the fact
that it held no unique content. The branch-protection check was unavailable and
is recorded as such; no protection state was modified, and none could have been
left dangling because the branch no longer exists. No force-push and no history
rewriting.

## 2026-10-27 — M0.4-D cross-platform verification (partial: Windows only)

Done:

- In-repository verification gates: `scripts/linux/dev-verify.sh` and
  `scripts/windows/dev-verify.ps1`. Neither installs dependencies or alters the
  toolchain; both exit non-zero when a required step was skipped.
- `chml_replace_backend()` so the POSIX branch can be proven at run time instead
  of inferred from a successful compile.
- Discriminating negative control shipped as part of `ctest`:
  `tests/atomic_identity_test.c` built against `tests/support/chml_io_inplace.c`
  and registered with `WILL_FAIL`.
- Temporary-file cleanup and example-output checks added to both gates.

Windows results (GCC 15.2.0 MinGW-w64/UCRT, CMake 4.2.3):

```
scripts\windows\dev-verify.ps1 -AllowPartial
  gcc build                           PASS
  gcc ctest                           PASS  100% tests passed, 6 tests
  atomic replacement executed         PASS  backend=win32-movefileex
  negative control (in-place write)   PASS  discriminated, rc=1
  temporary cleanup                   PASS
  example ran                         PASS
  gcc -fanalyzer                      PASS
  clang available                     SKIP
  asan/ubsan available                SKIP
  -> M0.4-D (Windows): INCOMPLETE
```

Not executed, and therefore **not claimed**:

- Linux/GCC strict build and suite.
- Linux/Clang strict build and suite.
- POSIX `rename()` replacement path at run time.
- AddressSanitizer and UndefinedBehaviorSanitizer.

Environment limitation: this development host has no Linux, no WSL distribution,
no Docker, no Clang, and a MinGW toolchain without `libasan`/`libubsan`. The
Linux gate has only been syntax-checked (`bash -n`) and has never run.

M0.4 is **not closed**. Running `scripts/linux/dev-verify.sh` on a Linux host is
the outstanding gate.

Also not claimed: crash durability, power-loss durability, concurrent-writer
safety.

## 2026-10-27 — M0.4-C component architecture evaluation

Done:

- Four isolated prototypes built and measured outside the repository:
  A arena + append-into-parent, B caller-provided fixed storage,
  C two-pass measure-then-emit, D transactional builder over an arena.
- Identical scenario across all four: two function boundaries, nesting, growth
  after handle return, allocation/capacity failure injection at every point
  inside the component, and byte-for-byte output comparison.
- Negative controls: reallocating contiguous array invalidates a handle; D with
  the rollback removed degrades to A and fails the harness.
- Compatibility check: an existing `ChmlNodeDef` driven through the builder
  reproduces the shipped writer's output exactly.
- ADR-0004 rewritten with results, the handle-stability definition, a
  recommended architecture and eight unresolved questions.

Verification commands (run from the external experiments directory):

```
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes common.c cand_a.c cand_b.c cand_c.c cand_d.c harness.c -o harness.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes neg_realloc.c -o neg_realloc.exe
gcc -std=c11 -w common.c cand_a.c cand_b.c cand_c.c cand_d_norollback.c harness.c -o neg_rollback.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes -I <repo>/include -I <repo>/src compat_check.c common.c cand_d.c <repo>/src/document.c <repo>/src/chml_io.c -o compat_check.exe
```

Results: harness checks passed; negative controls reproduced their defects;
compatibility check passed. Production suite unchanged at 4/4.

Not done (deliberately): the builder API is **not** implemented in the
production repository. ADR-0004 remains Proposed pending review.

Remaining limitations recorded in the ADR: builder prototypes do not model
attributes; rollback ergonomics in C are unresolved; arena failure granularity
and block sizing are undecided.

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
