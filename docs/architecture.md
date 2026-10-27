# CHML architecture — proposed, 2026-10-27

## Mission
Author complete, accessible web applications using standards-compliant C source; target unmodified mainstream browsers with ordinary HTML/CSS and optional JavaScript/WebAssembly output. The project is not a new browser standard and makes no measured performance claims yet.

## Architecture
- **Authoring:** C11-compatible declarative compound literals and an explicit procedural document API. C23 is a future compatibility target; the current CMake configuration uses C11.
- **Document layer:** `ChmlNodeDef` is caller-owned; `chml_document_add` deep-copies into document-owned allocations. The document is released by `chml_document_destroy`.
- **Styling layer:** `ChmlSheetDef` and `ChmlRuleDef` describe CSS selectors, declarations, and a restricted `@media` subset. They are serialized without browser-side runtime code.
- **Backend:** standard HTML and CSS files. A future native C server and optional client-side interop are separate milestones.

## Known limitations
- Element and attribute names are validated as ASCII case-insensitive: void elements are recognised in any case and duplicate attributes are rejected. Content models beyond void/raw-text/RCDATA children, foreign content, URL security, Unicode validity, and complete HTML parsing semantics are not yet supported or validated.
- Text is serialized by context: ordinary and RCDATA (`title`, `textarea`) content is escaped, raw-text (`script`, `style`, `xmp`, `iframe`, `noembed`, `noframes`) content is emitted verbatim. Raw-text content containing a sequence that would close the element early is rejected at materialization rather than escaped or silently rewritten. `<plaintext>` is rejected outright. `noscript` is still treated as ordinary text and is deferred.
- The serializer has a UTF-8 policy: `<meta charset="utf-8">` is injected as the first child of an existing `<head>`, an existing UTF-8 declaration suppresses injection, and a conflicting declaration is rejected before the destination is opened. Documents without a `<head>` are treated as fragments and receive no declaration.
- The declarative `CHML_*` macros are only safe within one function body; their compound literals do not survive a return. See `docs/decisions/ADR-0004-components-as-c-functions.md`.
- CSS support is a deliberately limited subset. Current validation rejects semicolons/braces/newlines, including valid CSS values that contain these characters; no general CSS tokenizer/parser exists. It does not establish full CSS compatibility.
- Both writers are atomic: content is written to a temporary file beside the destination and only replaces it after a complete, flushed, closed write. A failed write removes the temporary and leaves any existing artifact untouched. No allocation-failure injection tests yet.
- Declarative macros require at least one child for `CHML_ELEMENT`; empty elements use `CHML_LEAF`.
- Generated output has not undergone automated cross-browser, accessibility, or performance verification.

See `docs/compatibility-matrix.md` for the per-feature implemented/tested/unsupported/deferred status and the open specification questions.

## Invariants
1. No required handwritten HTML/CSS/JS application source.
2. No proprietary C grammar or mandatory compiler fork.
3. Never claim browser compatibility or speed without measured evidence.
4. Keep security and ownership explicit; reject unsupported operations rather than silently altering semantics.

## Verification

Two scripts exist, one per host family:

- `scripts/linux/dev-verify.sh` — strict GCC and Clang builds, full `ctest`,
  AddressSanitizer and UndefinedBehaviorSanitizer runs, an assertion that the
  POSIX replacement path actually executed, the atomic negative control, and an
  example-output check.
- `scripts/windows/dev-verify.ps1` — the same structure for this host, with
  `gcc -fanalyzer` in place of the sanitizer runs the MinGW toolchain cannot
  link.

Neither script installs dependencies or changes the toolchain. Both exit
non-zero when a required step was skipped, so an incomplete run cannot be
mistaken for a passing one.

`chml_replace_backend()` reports the compiled replacement mechanism
(`win32-movefileex` or `posix-rename`) so verification can prove the intended
branch ran rather than merely compiled.

`tests/atomic_identity_test.c` discriminates replacement from in-place rewriting
by comparing file identity across two writes; it is built twice, and the variant
linked against `tests/support/chml_io_inplace.c` is registered with `WILL_FAIL`.
A suite where that negative control passes is proving nothing.

## Future work
HTML semantics and serializer hardening; CSS tokenization and at-rules; portable C23 builds; browser visual/a11y tests; server rendering; optional Wasm/browser interop.

## Structural depth and lifetime contracts

- **Depth convention:** the root of a document is depth 0. The maximum
  permitted depth is **128**, so the deepest supported tree is 129 levels.
- **Declarative construction** enforces the bound inside `clone`; a deeper
  definition is rejected with `CHML_ERR_ARGUMENT` and nothing is added.
- **Serialization** enforces the same bound independently in preflight, and
  again defensively during emission. A refusal returns `CHML_ERR_ARGUMENT`
  before the atomic writer is opened, so no destination is created and an
  existing one is left byte-identical.
- **Builder construction does not enforce the bound.** An over-deep tree can be
  built and will be refused at write time. This is a deliberate M0 limitation,
  not an oversight.
- This bound is a **structural recursion safeguard**, not a denial-of-service
  guarantee: a shallow tree with very many siblings can still consume
  significant memory and time.

**Declarative input lifetime.** `chml_document_add` deep-copies the supplied
`ChmlNodeDef` tree synchronously; the caller's structures only need to be alive
for the duration of that call. A `ChmlNodeDef` whose `children` pointer refers
to storage that has already expired is invalid input, and constructing one — for
example by retaining a pointer to a loop-local compound literal — is a caller
error that CHML cannot detect.

**No concurrent mutation.** Callers must not mutate or destroy a document, a
builder, or any referenced attribute value during the interval from preflight
through the completion of serialization. The writer does not snapshot the tree
and CHML is not thread-safe.

**Error classification.** `CHML_ERR_ARGUMENT` covers structural problems
(depth, invalid names, void elements with content). `CHML_ERR_POLICY` is
reserved for URL-policy refusal and never indicates a structural fault.

## URL policy decisions

- **Names are matched ASCII case-insensitively**, including `xlink:href`. CHML
  emits HTML; the HTML tokenizer lowercases attribute names and then maps
  `xlink:href` into the XLink namespace. Matching foreign attribute names
  case-sensitively would let `XLINK:HREF` bypass the policy while still acting
  as a link in the browser.
- **SVG `script` is a governed element.** An SVG script element loads and runs
  the resource named by `href`/`xlink:href`; a `data:` value there executes.
  Governing HTML `script[src]` while ignoring the SVG form would leave the
  refusal of `data:` trivially bypassable.
- **Refusal happens in preflight**, before the atomic writer opens a temporary
  file. A policy refusal therefore creates no destination and leaves an
  existing one byte-identical. The check is repeated defensively during
  emission.
- **This is not sanitization.** The policy bounds the scheme of an enumerated
  set of URL-bearing attributes. Attribute content is escaped, never filtered;
  inline script text is emitted as authored; event handlers, CSS `url()`,
  `meta refresh`, `base[href]` and the stylesheet writer are outside it.
  CHML output must not be described as safe for untrusted input on the strength
  of this policy alone.
