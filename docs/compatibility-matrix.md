# CHML compatibility matrix

Status of the features CHML actually implements, as of 2026-10-27 (M0.4-A).

This file exists so that no HTML5 or CSS3 coverage claim is made implicitly by
proximity. **A row marked Implemented is a statement about CHML's own code, not
about browser conformance.** No row here is backed by browser-level testing.

## Legend

| Status | Meaning |
| --- | --- |
| Tested | Covered by an automated test in `tests/` |
| Implemented | Code path exists and is exercised, but coverage is partial |
| Unsupported | Explicitly rejected with an error; never silently altered |
| Deferred | No policy yet; do not rely on the behaviour |
| Absent | No API exists |

## HTML serialization

| Feature | Status | Evidence / note |
| --- | --- | --- |
| `<!doctype html>` prefix | Tested | Emitted unconditionally by `chml_document_write` |
| Ordinary text escaping (`&` `<` `>`) | Tested | `serialization_test`: `a & b < c > d` |
| Attribute value escaping (`&` `"` `<` `>`) | Tested | `serialization_test`: quoted attribute fixture |
| Void elements (14 names) | Tested | Rejected with text or children, ASCII case-insensitive |
| Raw-text elements: `script`, `style` | Tested | Verbatim output; end-tag hazards rejected |
| Raw-text elements: `xmp`, `iframe`, `noembed`, `noframes` | Implemented | Same policy, no dedicated fixtures |
| `plaintext` | Unsupported | Rejected: swallows its own end tag, cannot be well formed |
| `noscript` | Deferred | Serialized as ordinary text; scripting-dependent semantics unimplemented |
| RCDATA elements: `title`, `textarea` | Tested | Escaped as ordinary text; element children rejected |
| Boolean attributes | Tested | `CHML_BOOLEAN_ATTR` emits `name` with no value |
| String attributes, including ARIA | Tested | `aria-disabled="false"` stays string-valued |
| Duplicate attribute names | Tested | Rejected, ASCII case-insensitive |
| Unknown attribute kind | Tested | Rejected |
| UTF-8 charset declaration | Tested | Injected as first child of an existing `<head>` |
| Conflicting charset declaration | Tested | Rejected before the destination is opened |
| Multiple/duplicate charset declarations | Tested | An existing declaration suppresses injection |
| Comment nodes | Absent | No API |
| Doctype control | Absent | Fixed output |
| SVG / MathML foreign content | Deferred | Case-sensitive names not modelled; see open questions |
| URL scheme security (`javascript:`) | Deferred | No validation |
| Unicode validity / embedded NUL | Deferred | Strings are byte sequences; NUL truncates |
| Full attribute-name conformance | Partial | `[A-Za-z][A-Za-z0-9:_-]*` only |
| Document structure constraints (one `html`, head/body order) | Absent | Author's responsibility |
| Encoding window (declaration within first 1024 bytes) | Deferred | Not enforced |
| Reusable components as C functions | Proposed | Evaluated in ADR-0004; **not implemented**. The declarative macros are single-function-scoped |

## CSS

| Feature | Status | Evidence / note |
| --- | --- | --- |
| Declarations (`property:value;`) | Tested | `style_test` |
| Simple selectors as authored | Tested | Not parsed or validated beyond character rejection |
| `@media` blocks | Implemented | `CHML_MEDIA_RULE`; condition passed through verbatim |
| Any other at-rule | Absent | No API |
| General CSS tokenizer / grammar | Unsupported | Semicolons, braces, newlines in values are rejected even when valid CSS contains them |
| Custom property *declaration* | Implemented | `--accent` works as a plain declaration |
| Cascade, specificity, computed values | Not CHML's layer | Delegated to the browser; no claim |
| Error recovery / partial stylesheet output | Tested | Both writers use a temporary file plus replace; a failed write removes the temporary and leaves the existing artifact intact |
| Atomic write: success path | Tested | `atomic_write_test` |
| Atomic write: temp cleanup on failure | Tested | Coverage fixed at 8 candidate names |
| Atomic write: existing artifact preserved | Tested | Read-only destination refuses replacement on Windows |
| Atomic write: concurrency / multi-process safety | Deferred | No locking; last writer wins |

## Build and verification

| Item | Status | Note |
| --- | --- | --- |
| ISO C11, GCC | Tested | 15.2.0, `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes` |
| ISO C11, Clang | Not verified | Clang is not installed on the current Windows host |
| C23 | Deferred | Target only; not compiled as C23 |
| ASan / UBSan | Not run | Runtime libraries absent from the MinGW toolchain in use |
| Static analysis | Tested | `gcc -fanalyzer` clean on both translation units |
| Fault injection (allocation failure) | Absent | No test harness |
| Browser-level / accessibility / visual parity | Absent | No browser automation yet |

## Open questions

Unresolved specification decisions. Do not treat any of these as settled.

1. **`noscript`** serialization depends on the scripting flag. No policy yet.
2. **`xmp`, `iframe`, `noembed`, `noframes`** are classified as raw text but have
   no fixtures. Should CHML support obsolete raw-text elements at all, or reject
   them like `plaintext`?
3. **Charset when no `<head>` exists.** CHML currently treats such a document as
   a fragment and emits no declaration. Whether the serializer should synthesize
   a `<head>` is undecided; synthesizing changes document structure silently.
4. **`http-equiv` charset detection** is a case-insensitive substring search for
   `charset=`, not an HTML parse. It can be fooled by content inside a comment
   or a quoted parameter.
5. **Encoding window.** HTML requires the declaration within the first 1024
   bytes; CHML does not enforce or measure this.
6. **Author byte encoding.** CHML assumes C string bytes are UTF-8 and does not
   validate them. A source file saved as Latin-1 would produce invalid UTF-8
   under a UTF-8 declaration.
7. **Boolean attribute names.** Only `aria-*` is rejected as a boolean. There is
   no allowlist of HTML boolean attribute names, so `CHML_BOOLEAN_ATTR("title")`
   is currently accepted and would emit a value-less `title`.
8. **Foreign content.** Case-insensitive name handling, as implemented for void
   and raw-text detection, is wrong for SVG/MathML, where names such as
   `viewBox` and `foreignObject` are case-sensitive.
