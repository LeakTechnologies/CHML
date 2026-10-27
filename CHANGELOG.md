# CHML changelog

## Unreleased — verification hygiene: final-newline check

### Added

- **`scripts/check-final-newline.sh`** (bash) and
  **`scripts/check-final-newline.ps1`** (Windows): every tracked `.c`, `.h`,
  `.md` and `.sh` file must end with a line feed. One rule covers both
  conventions — an LF file ends with 0x0A and a CRLF file also ends with 0x0A —
  while a file ending in a bare CR, a space or any other byte is a violation.
  Empty files are accepted; a file that cannot be read is a failure, not a pass.
  The `.sh` script is **bash**, not POSIX sh: it uses `BASH_SOURCE`, `read -d`,
  parameter expansion and `local`, and both gates invoke it as `bash <script>`.
- Both host gates run the check and its controls as their own
  `repository hygiene` section, before the build. A failure is a gate failure;
  the current build, test and sanitizer requirements are unchanged.

### What is checked (contract)

- Paths are enumerated from **Git's index** (`git ls-files -z`), never from a
  filesystem walk, so build output and untracked files cannot influence the
  result. Paths are NUL-separated: spaces, quotes and non-ASCII bytes survive.
- The bytes read are the **working-tree files** at those paths. Committed blob
  contents are not read directly. A violation introduced by an uncommitted edit
  is therefore reported, and a clean checkout can differ from the last commit
  only by line-ending conversion.
- LF and CRLF both pass, because both end with byte `0x0A`; a bare CR does not.
- A tracked file that is unreadable, or absent from the working tree, is a
  verification failure (exit 2), never a pass. Exit 1 means "violations found",
  exit 0 means "checked and clean".
- **Filenames are decoded as UTF-8 on both hosts.** PowerShell 5.1 decodes
  native stdout with the console code page, which corrupted UTF-8 path bytes and
  made a tracked `café.md` look absent (exit 2 on a compliant file). The Windows
  implementation now reads git's stdout as raw bytes and decodes UTF-8
  explicitly, so no console state is read or written.

### Controls

Both scripts carry a `controls` mode; both print the same 19 case lines and the
two outputs are compared programmatically:

- Accepted: LF, CRLF, empty, and a compliant non-ASCII name.
- Refused: missing LF, bare CR, and names containing a space, a quote or
  non-ASCII bytes.
- Ignored: an out-of-scope extension.
- **Three-way tracked classification**, in a temporary Git repository: a
  compliant tree exits 0, a missing final LF exits 1 and is reported, and a
  tracked-but-absent file exits 2 and is classified as absent rather than clean.
- **Mutation meta-controls**: two mutated copies of the checker (one that always
  accepts, one that always rejects) must make these controls fail. This keeps
  proving that the controls detect a broken checker rather than merely exercising
  their own fixtures.

### Why

Run `40595446406` of the Linux gate failed on Clang's `-Wnewline-eof`, a warning
GCC does not enable. That class of defect is invisible to the Windows build, and
invisible to every build once the file is Markdown. It is now checked directly
and deterministically on both host families.

### Limits

- `.ps1`, `.yml` and other extensions are out of scope; extending the check is a
  one-line change in each script (the extension lists must be kept in step).
- This is a hygiene check on file endings. It says nothing about content
  correctness, encoding validity or trailing whitespace.
- A non-ASCII filename is classified correctly on both hosts, but the name is
  *rendered* through the console code page when output is redirected, so a log
  may show a replacement character. The verdict, counts and exit status are
  unaffected.

## Unreleased — M0-S2b: context-sensitive URL policy (security)

### Added

- **URL validation for designated URL-bearing attributes**, applied before any
  output is written: `a/area/link[href]`, `img/script/iframe/source/track/embed/audio/video/input[src]`,
  `form[action]`, `button/input[formaction]`, `video[poster]`, and SVG
  `a/use/image/script[href|xlink:href]`.
- **`CHML_ERR_POLICY = 4`**, appended so existing enum values are unchanged. A
  policy refusal is now distinguishable from a malformed argument.
- Namespace-aware traversal: `svg` is SVG for itself and its children,
  `foreignObject` is SVG for itself and HTML for its children,
  `math`/`annotation-xml` are handled per the restricted model, and MathML
  URL-bearing attributes fail closed.
- `tests/policy_test.c`: 47 assertions across nine groups, every negative case
  run through both write APIs with status, file-creation and destination
  preservation asserted.

### Intentional breaking changes

These values were previously accepted and are now rejected with
`CHML_ERR_POLICY`:

- schemes `javascript:`, `vbscript:`, `data:`, `blob:`, `file:`;
- protocol-relative URLs (`//host/path`);
- any backslash in a URL value;
- leading whitespace or control characters;
- empty values on all designated attributes except `a[href]`, `area[href]` and
  `form[action]`;
- the attributes `srcset`, `srcdoc` (any value, including empty), and
  `object[data]` (any value).

`mailto:` and `tel:` remain allowed on navigation attributes only.

### Explicit limitations

This is **not** HTML sanitization. It does not cover event-handler attributes,
CSS `url()` or `@import`, `meta http-equiv=refresh`, `base[href]`,
`object` beyond `data`, or any attribute outside the designated matrix. The
stylesheet writer is a separate surface and is not subject to this policy.
CHML output must not be described as safe for untrusted input on the strength
of this change alone.

### Review remediation (independent adversarial review of PR #1)

Four defects were found after the first Linux gate passed, and are fixed here:

- **`xlink:href` was matched case-sensitively** while every other attribute name
  was matched case-insensitively. Because the HTML tokenizer lowercases
  attribute names before mapping `xlink:href` into the XLink namespace,
  `XLINK:HREF="javascript:..."` on an SVG `a`, `use` or `image` was not
  classified as a URL attribute at all, and serialized unchanged. Confirmed in
  Chromium: the emitted attribute acted as a link and executed the script on
  click. All name comparisons are now ASCII case-insensitive.
- **SVG `script[href]`/`script[xlink:href]` were ungoverned.** Confirmed in
  Chromium: `<svg><script href="data:text/javascript,...">` loads and runs the
  resource. The policy refused `data:` on HTML `script[src]` but not here, which
  made that refusal bypassable. SVG `script` is now a governed element.
- **`audio[src]`, `video[src]` and `input[src]` were ungoverned** while
  `img[src]` and `source[src]` were. These are the same resource-loading class
  and are now governed. `link[imagesrcset]` is now refused, for the same reason
  `srcset` is: it carries a candidate list that needs its own parser.
- **Documentation contradicted itself**: the security-boundary summary still
  stated "No scheme policy" and "not yet implemented" while the status table
  stated "Enforced". The summary is reconciled and the exclusions are now
  enumerated explicitly.

Two smaller consistency fixes: the declarative clone bound now uses the shared
`CHML_MAX_DEPTH` constant instead of a second literal `128`, and the two
Markdown files that lacked a final newline now have one.

Known follow-up, not addressed here: a policy refusal detected during emission
(rather than preflight) would be reported as `CHML_ERR_IO`, because the writer
maps any writer failure to I/O. The path is currently unreachable — preflight
runs the same logic first — but the classification would be wrong if the two
ever diverge.


Behaviour-affecting changes, newest first.

## Unreleased — M0.5-A transactional component builder (2026-10-27)

### Added

- `include/chml/builder.h`: experimental transactional builder — detached
  session construction, elements, text nodes, string and boolean attributes,
  marks with rollback, attach with ownership transfer, discard, atomic write.
- `src/builder.c`: region-based storage. Each session is its own arena region;
  attach splices its blocks into the document region; discard frees them.
- `src/chml_node.h`: one shared node representation and one serializer for both
  front-ends. `chml_write_node_list` is now the single write path used by
  `chml_document_write` and the builder.
- `examples/components/`: `PageLayout`, `Navigation`, `Card` reference
  components and a site example.
- `tests/builder_test.c`: 7 test groups, 15 deterministic allocation-failure
  points, ownership rejections, and byte-identical parity with the declarative
  path.
- ADR-0005 (Proposed) recording the transaction model and its open questions.
  ADR-0004 moved to **Accepted** for its principles only.

### Changed

- `struct ChmlNode.children` is now an array of pointers rather than a
  contiguous node array, so builder and declarative nodes share one layout.
  Serialization and escaping are unchanged; parity is asserted byte-for-byte.
- Construction is session-scoped: document-owned nodes accept content only
  through `chml_builder_attach`, never by direct mutation.

### Defects found and fixed during implementation

1. Rollback restored `children` but not `child_cap`, so the next append wrote
   through a NULL array (segfault).
2. `chml_builder_begin` could fail after allocating its root, leaving a block in
   a session that never opened and therefore could never be discarded.

### Evidence

Strict GCC 15.2.0 C11 clean; 7/7 tests. Negative control: making rollback a
no-op fails three checks. Rollback and discard both return the builder to its
pre-component byte count.

## Unreleased — M0.4-D4 Linux verification executed (2026-10-27)

M0.4-D is satisfied: the Linux gate has real execution evidence.

### Changed

- Repository visibility changed from private to **public** (user-authorised) to
  remove metered Actions minutes from the blocker. No billing settings, payment
  methods or spending limits were touched. The account's billing restriction
  itself remains unresolved.
- Sanitizer verification strengthened: builds run verbosely and a sanitizer step
  must show the flag in the compiler command **and** runtime evidence in the
  binary (`ldd` for dynamic, `__asan_`/`__ubsan_` symbols for static). Previously
  a build that silently dropped `-fsanitize` would have passed.
- Local absolute paths removed from documentation before publication.

### Evidence

Ubuntu 24.04.5, kernel 6.17.0-1022-azure, CMake 3.31.6, GCC 13.3.0,
Clang 18.1.3. Runs `40548472675` and `40548731375`, both `success`.

Strict GCC and Clang builds clean; 6/6 tests in all four configurations;
`posix-rename` backend confirmed at run time; negative control discriminated for
the expected reason; no temporary residue; example output correct; ASan and UBSan
instrumentation proven present.

### Not claimed

Crash durability, power-loss durability, concurrent-writer safety, browser
conformance, performance. The workflow's failure path has not been exercised.

## Unreleased — M0.4-D2 Linux verification execution (2026-10-27)

### Added

- **`.github/workflows/linux-verify.yml`** — runs `scripts/linux/dev-verify.sh`
  on `ubuntu-latest` for pushes and pull requests to `master`, plus manual
  dispatch. `contents: read` only, no third-party actions, logs uploaded on
  failure, and the gate conclusion printed explicitly.
- Negative-control reason reporting, described below.

### Changed

- `tests/atomic_identity_test.c` prints
  `NEGATIVE_CONTROL_REASON=identity_unchanged` (exit 1, intended),
  `harness_error` (exit 2) or `none` (exit 0). Both verification scripts now
  require exit 1 **and** the `identity_unchanged` marker, so a crash or an
  unrelated failure can no longer pass as a successful discrimination.
- Remote `main` deleted. Verified before deletion: `master` is the default
  branch, `main` held only a one-line README, no open PRs depended on it, and
  the initial commit is an ancestor of `master`. The branch-protection query
  returned 403 (unavailable on a private free repository), so absence of
  protection was **not** verified — recorded as an unavailable check rather than
  a confirmed absence.

### Blocked

The workflow triggered but GitHub did not start the job:

> The job was not started because recent account payments have failed or your
> spending limit needs to be increased.

Run `40547489801` concluded `failure` after 3 s with zero steps executed.
Repository Actions policy is `enabled`/`all`, so the blocker is account billing
for a **private** repository, not a workflow defect.

**No Linux evidence exists.** M0.4-D stays open.

## Unreleased — M0.4-D cross-platform verification (2026-10-27)

### Added

- **`scripts/linux/dev-verify.sh`** — Linux verification gate: toolchain check
  without installing anything, strict GCC and Clang builds, full `ctest`,
  AddressSanitizer and UndefinedBehaviorSanitizer runs (probed before use), a
  run-time assertion that the POSIX replacement path executed, the atomic
  negative control, temporary-file cleanup checks, and an example-output check.
  Records compiler versions, log paths, per-step status and exit codes.
- **`scripts/windows/dev-verify.ps1`** — the same structure for Windows, using
  `gcc -fanalyzer` where the MinGW toolchain cannot link sanitizer runtimes.
  Default is strict; `-AllowPartial` downgrades missing tools to `SKIPPED` and
  still exits non-zero with `INCOMPLETE`.
- **`chml_replace_backend()`** in `src/chml_io.h` — reports
  `win32-movefileex` or `posix-rename`, so verification can prove the intended
  branch ran. Compilation alone is not evidence.
- **`tests/atomic_identity_test.c`** — compares file identity across two writes.
  Built twice: against the real library (must exit 0) and against
  `tests/support/chml_io_inplace.c` (must exit 1, registered `WILL_FAIL`).

### Changed

- `ctest` is now 6 tests. `chml_atomic_identity_negative` passing means the
  in-place writer was correctly rejected; it is not a bug.

### Evidence

- Windows/GCC 15.2.0: strict build clean, 6/6 tests, backend asserted as
  `win32-movefileex`, negative control discriminated (`rc=1`), no temporary
  residue, example emits `index.html` + `style.css` with one charset
  declaration, `gcc -fanalyzer` clean on three translation units.
- **Not executed:** Linux/GCC, Linux/Clang, ASan, UBSan, and the POSIX
  `rename` path. No Linux host or WSL distribution is available here; Clang and
  sanitizer runtimes are absent from the MinGW toolchain. The Windows gate
  therefore reports `INCOMPLETE`, not `PASSED`.

## Unreleased — M0.4-C component architecture evaluation (2026-10-27)

**No production code changed in this increment.** `include/`, `src/`, `tests/`
and `CMakeLists.txt` are untouched; the test suite is unchanged at 4/4.

### Changed (documentation)

- `docs/decisions/ADR-0004` rewritten with the four-candidate evaluation:
  measured results, negative controls, the handle-stability definition, a
  recommended architecture (D, transactional builder over an arena), and eight
  unresolved questions. Status remains **Proposed**.
- `AGENTS.md` gained a governance rule: an architecture proposal must show a
  failure mode that distinguishes it from the alternatives.

### Evidence

- Experimental code, kept outside the repository and not committed:
  (`README.md` holds the exact build/run commands and recorded results).
- All four candidates emit byte-identical HTML (240 bytes).
- Transactional D rolled back cleanly at all 5 injected allocation failures;
  non-transactional A left a partial card attached in 4 of 5.
- A `realloc`-grown array moved its base and invalidated a previously returned
  handle (negative control), which is why that design is excluded.
- Removing D's rollback call degrades it to A's behaviour and fails the harness,
  so the rollback check is discriminating.
- A real `ChmlNodeDef` driven through the builder reproduces the shipped
  writer's output byte-for-byte.

## Unreleased — M0.4-B atomic output (2026-10-27)

### Added

- **`src/chml_io.c`, `src/chml_io.h`:** a shared writer that serializes into a
  temporary file beside the destination and only replaces the destination after
  a complete, flushed, closed write. Temporary names are opened with the C11
  exclusive mode `"wbx"`, so a stale temporary is never silently truncated.
- **`tests/atomic_write_test.c`:** 8 checks covering the success path, clean
  truncation on overwrite, missing directory, directory destination, read-only
  destination, temporary cleanup on failure, the stylesheet writer, and
  replacement-versus-rewrite identity.

### Changed

- `chml_document_write` and `chml_stylesheet_write` both go through the atomic
  writer. A failed write removes its temporary file and leaves any existing
  artifact untouched.

### Platform note

POSIX uses ISO C `rename()`. Windows uses `MoveFileExA` with
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`, because ISO C `rename()`
does not replace an existing destination there. This is an OS call behind
`#ifdef _WIN32`, not a compiler extension; the translation unit remains valid
ISO C11.

### Evidence

- Strict GCC C11 build clean; `ctest` 4/4.
- 8 atomic-write checks pass on the Windows/MinGW host.
- Negative controls: reverting to a direct in-place write fails only the identity
  check; skipping temporary cleanup fails the two cleanup checks.

### Limitations

- The atomicity check relies on file identity (`nFileIndex` on Windows,
  `st_dev`/`st_ino` on POSIX). The POSIX branch has not been executed.
- No fault injection mid-write. The design covers a partial write by never
  touching the destination until the temporary is complete, but a failure
  injected inside the serializer is not yet exercised.
- No locking: concurrent writers to the same path are last-writer-wins.
- `MOVEFILE_WRITE_THROUGH` does not guarantee the directory entry survives a
  power loss on every filesystem; the guarantee here is against partial content,
  not against hardware failure.

## Unreleased — M0.4-A serialization correctness (2026-10-27)

### Added

- **Context-aware text serialization.** Elements are classified as ordinary,
  RCDATA (`title`, `textarea`) or raw text (`script`, `style`, `xmp`, `iframe`,
  `noembed`, `noframes`). Ordinary and RCDATA content is character-escaped; raw
  text is emitted verbatim. Text is normally supplied as child nodes, so the
  context is carried down the tree rather than read off the parent's `.text`.
- **Raw-text end-tag hazard rejection.** Raw-text content is concatenated and
  rejected if it contains `</` + the element's own name + one of TAB, LF, FF,
  SPACE, `/` or `>`, matching the tokenizer's end-tag-name state. `</scriptx>`,
  `</scrip` and a trailing `</script` (followed by the serializer's own `<`) are
  correctly *not* treated as hazards.
- **Semantic attributes.** `ChmlAttrDef` gained `ChmlAttributeKind`
  (`CHML_ATTR_STRING` / `CHML_ATTR_BOOLEAN`) and `CHML_BOOLEAN_ATTR(name)`.
  Boolean attributes serialize as `name`; string attributes keep
  `name="value"`. `aria-disabled="false"` is never reinterpreted as a boolean.
- **UTF-8 encoding policy.** `chml_document_write` injects
  `<meta charset="utf-8">` as the first child of an existing `<head>` and
  suppresses injection when the document already declares UTF-8.
- **`docs/compatibility-matrix.md`** with implemented/tested/unsupported/deferred
  status and the open specification questions.

### Changed

- `<plaintext>` is rejected at materialization: it consumes every following
  byte including its own end tag, so it cannot be serialized into a well-formed
  document.
- Raw-text and RCDATA elements reject element children; they hold text only.
- `CHML_ELEMENT`/`CHML_WITH_ATTRS` are documented as valid only inside a single
  function body, because their compound literals do not survive a return
  (see ADR-0004).

### Fixed

- `chml_document_add` root overflow guard used `==`; changed to a `>=` test that
  also accounts for the increment.

### Evidence

- `gcc 15.2.0` C11, `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wstrict-prototypes`: clean build.
- `ctest`: 3/3 pass (`style`, `document`, `serialization`).
- 32 fixtures in `tests/serialization_test.c`; every one was negative-controlled
  by reverting the corresponding code and observing the fixture fail.
- `gcc -fanalyzer`: clean on both translation units.
- The example now emits exactly one `charset` declaration and preserves its
  em dash.

### Public API change

`ChmlAttrDef` gains a trailing `kind` field. `CHML_ATTR` remains
source-compatible because the zero value is `CHML_ATTR_STRING`. Code that
initialises `ChmlAttrDef` positionally with two values still compiles but
should be updated.

### Not done in this increment

Atomic output (M0.4-B), component builder API (M0.4-C), sanitizers and Clang
verification (M0.4-D), CSS grammar.

## M0.4 hardening increment 2 (2026-10-27)

### Fixed

- **Void-element content rejection was case-sensitive.** `is_void()` compared tag
  names with `strcmp`, so `CHML_ELEMENT("IMG", ...)` and `CHML_ELEMENT("Img", ...)`
  were accepted and serialized as `<IMG>child</IMG>`. HTML element names are ASCII
  case-insensitive, so this produced markup the parser rejects. Comparison now uses
  an explicit ASCII fold that does not depend on the process locale. Tag names are
  still emitted as authored, which leaves room for case-sensitive SVG/MathML
  foreign content later.
- **Duplicate attribute names were accepted.** `class="a" class="b"` is invalid and
  an HTML parser silently drops the second occurrence, losing author data. Such
  definitions are now rejected with `CHML_ERR_ARGUMENT` before any output.

### Added

- Regression tests for both defects, including case-insensitive duplicate detection
  (`class` + `CLASS`) and an output check that a rejected `add` leaves no residue.
- `docs/decisions/ADR-0004-components-as-c-functions.md` (Proposed) recording
  measured evidence on reusable components.

### Evidence

- `gcc 15.2.0` C11, `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wstrict-prototypes`: clean build.
- `ctest`: 2/2 pass.
- Negative controls: reverting each fix makes the corresponding assertion fail
  (`document_test.c` lines 25 and 31), so the new tests are not vacuous.
- `gcc -fanalyzer` on `src/document.c` and `src/style.c`: no diagnostics.
- **Not verified:** Clang (not installed on this host) and ASan/UBSan (runtime
  libraries are not shipped with the MinGW toolchain in use).

### Known defects found but not fixed

- Raw-text elements (`script`, `style`) are HTML-escaped, which breaks their
  content because browsers do not decode character references there.
- The serializer emits no `<meta charset>` and no BOM, so non-ASCII text is
  decoded by browsers as windows-1252 by default.
- CSS validation is still ad-hoc character rejection, not a parser.
