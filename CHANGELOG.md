# CHML changelog

Behaviour-affecting changes, newest first. Test counts are for `ctest` on the
suite as configured in `CMakeLists.txt`.

## Unreleased — M0.4 hardening increment 2 (2026-10-27)

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
