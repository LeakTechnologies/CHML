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
| Raw-text elements: `script`, `style` | Tested | Verbatim output; end-tag hazards rejected. **Content is never sanitized**: a `script` body is emitted as authored. Requiring trusted input is the caller's responsibility and is not enforced by the API |
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
| SVG / MathML foreign content | Partial | Security classification is namespace-aware (`svg`, `foreignObject`, MathML fails closed). Serialization still does not case-correct foreign names (`viewbox` stays `viewbox`); see open questions |
| URL scheme security (designated attributes) | **Enforced** | Context-sensitive: http/https and relative allowed; `javascript:`, `vbscript:`, `data:`, `blob:`, `file:`, protocol-relative, backslashes and leading control characters refused with `CHML_ERR_POLICY` before any file is opened. Applies only to the designated attribute matrix (extended in review remediation to audio/video/input `src` and SVG `script`) — not general sanitization |
| Unicode validity / embedded NUL | Deferred | Strings are byte sequences; NUL truncates |
| Full attribute-name conformance | Partial | `[A-Za-z][A-Za-z0-9:_-]*` only |
| Document structure constraints (one `html`, head/body order) | Absent | Author's responsibility |
| Encoding window (declaration within first 1024 bytes) | Deferred | Not enforced |
| Reusable components as C functions | Experimental | `include/chml/builder.h`; ADR-0004 Accepted for principles, ADR-0005 Proposed for the transaction API |
| Detached construction + attach | Tested | `chml_builder_begin` / `attach`; ownership transfer by region splice |
| Builder rollback | Tested | mark/rollback; negative control fails 3 checks |
| Builder allocation-failure injection | Tested | 15 points, byte count returns to baseline |
| Builder vs declarative parity | Tested | byte-identical output for the same markup |

## CSS

| Feature | Status | Evidence / note |
| --- | --- | --- |
| Declarations (`property:value;`) | Tested | `style_test` |
| Simple selectors as authored | Tested | Not parsed or validated beyond character rejection |
| `@media` blocks | Implemented | `CHML_MEDIA_RULE`; condition passed through verbatim |
| Any other at-rule | Absent | No API |
| General CSS tokenizer / grammar | Unsupported | Semicolons, braces, newlines in values are rejected even when valid CSS contains them. The restriction is **syntax containment only**, not sanitization: it stops stylesheet breakout, and does not constrain values semantically (`url(...)`, `content`, `@import` through a value) |
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
| ISO C11, GCC (Windows) | Tested | 15.2.0 MinGW-w64/UCRT, strict flags, 6/6 tests |
| ISO C11, GCC (Linux) | Tested | 13.3.0 on Ubuntu 24.04.5, strict flags, 6/6 tests |
| ISO C11, Clang (Linux) | Tested | 18.1.3 on Ubuntu 24.04.5, strict flags, 6/6 tests |
| POSIX atomic replacement (`rename`) | Tested | `backend=posix-rename`, file identity changes across writes |
| Win32 atomic replacement (`MoveFileExA`) | Tested | `chml_atomic_identity_test` asserts identity changes |
| Atomic negative control (in-place write) | Tested | `chml_atomic_identity_negative`, fails with reason `identity_unchanged` |
| GitHub Actions Linux gate | Tested | runs 40548472675, 40548731375 (success) |
| C23 | Deferred | Target only; not compiled as C23 |
| ASan | Tested | instrumentation proven: flag in command + `__asan_` symbols; 6/6 |
| UBSan | Tested | instrumentation proven: flag in command + `__ubsan_` symbols; 6/6 |
| Static analysis | Tested | `gcc -fanalyzer` clean on `document.c`, `style.c`, `chml_io.c` (Windows) |
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
8. **Foreign content.** Serialization does not case-correct foreign names:
   `viewbox` is emitted as authored, so it will not become `viewBox`. Security
   classification is namespace-aware and matches names ASCII case-insensitively,
   which is correct for HTML parsing (the tokenizer lowercases attribute names
   first) but is *not* a claim of XML conformance. Writers targeting XML
   serialization would need exact case handling.

## Security boundary summary

These are the guarantees the implementation actually provides, as of M0-S2b.
They are deliberately narrow, and each row states what is enforced rather than
what would be desirable.

| Property | Guarantee | Type |
| --- | --- | --- |
| Text and attribute escaping | `&`, `<`, `>` escaped in text; `"` also escaped in attribute values | Enforced on every path |
| URL attributes (designated set) | Scheme policy enforced before any file is opened; see the attribute table below | Enforced, enumerated set only |
| URL attributes (everything else) | **No policy.** Attributes outside the designated set serialize unchanged | Absent |
| Script content (HTML and SVG) | Emitted verbatim; never sanitized | Absent by design |
| Structural validity | Rejected at materialization; nothing is written before rejection | **Runtime**, not compile-time |
| CSS | Syntax containment (no `{`, `}`, `;`, control characters) | Runtime, semantic-free |
| Charset | UTF-8 declaration injected; conflicts rejected before the destination opens | Enforced |
| URL policy status | `CHML_ERR_POLICY = 4` returned for policy refusal, distinct from malformed arguments | Enforced |
| Atomic output | Temp file plus replace; failure leaves the previous artifact intact | Enforced |

The C type system does **not** prevent constructing an invalid document. Invalid
trees compile; they are rejected when added to a document or when serialized.

### URL policy is attribute-specific

A single allowlist is not sufficient, so the policy is enumerated per attribute
and per namespace. **Implemented** (M0-S2b):

| Attribute | Enforced policy |
| --- | --- |
| `a`, `area`, `link` `href` | http, https, relative; `mailto`/`tel` on navigation only |
| `img`, `script`, `iframe`, `source`, `track`, `embed`, `audio`, `video`, `input` `src` | http, https, relative |
| `form` `action`, `button`/`input` `formaction` | http, https, relative; empty only on `form` |
| `video` `poster` | http, https, relative |
| SVG `a`, `use`, `image`, `script` `href`/`xlink:href` | http, https, relative |
| `srcset`, `imagesrcset`, `srcdoc`, `object` `data` | Refused at any value, including empty |
| Anything in MathML | `href`, `src`, `xlink:href` refused (fail closed) |

Every name comparison is ASCII case-insensitive, including `xlink:href`: the
HTML tokenizer lowercases attribute names before mapping `xlink:href` into the
XLink namespace, so matching it case-sensitively would let a capitalization
change bypass the policy.

**Not covered** — these remain the caller's responsibility:

| Surface | Status |
| --- | --- |
| Event-handler attributes (`onclick`, ...) | No policy; attribute content is escaped, not filtered |
| CSS `url()`, `@import` in a `style` attribute or stylesheet | No policy; the stylesheet writer is a separate surface |
| `meta http-equiv=refresh` `content` | No policy |
| `base` `href` | No policy; it changes resolution for every relative URL in the document |
| `object` attributes other than `data` | No policy |
| Inline script text, including SVG `script` | Emitted verbatim; never sanitized |
| Any attribute not listed above | No policy |

Normalization follows browser parsing where it matters, not prefix matching.
Leading and embedded ASCII whitespace and control characters are **refused**
rather than trimmed; scheme matching is case-insensitive; named schemes are
compared over the whole first path segment. Percent-encoding is deliberately
**not** decoded, because browsers do not percent-decode a scheme: `javascript%3a`
is a relative path, not a script URL. Entity-looking values are not decoded by
the classifier either; they are neutralized later by attribute escaping, which
is why `jav&#x61;script:` serializes as `jav&amp;#x61;script:` and stays inert.
