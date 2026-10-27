# CHML changelog

Behaviour-affecting changes, newest first. Test counts are for `ctest` on the
suite as configured in `CMakeLists.txt`.

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
