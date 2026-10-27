# CHML architecture — proposed, 2026-10-27

## Mission
Author complete, accessible web applications using standards-compliant C source; target unmodified mainstream browsers with ordinary HTML/CSS and optional JavaScript/WebAssembly output. The project is not a new browser standard and makes no measured performance claims yet.

## Architecture
- **Authoring:** C11-compatible declarative compound literals and an explicit procedural document API. C23 is a future compatibility target; the current CMake configuration uses C11.
- **Document layer:** `ChmlNodeDef` is caller-owned; `chml_document_add` deep-copies into document-owned allocations. The document is released by `chml_document_destroy`.
- **Styling layer:** `ChmlSheetDef` and `ChmlRuleDef` describe CSS selectors, declarations, and a restricted `@media` subset. They are serialized without browser-side runtime code.
- **Backend:** standard HTML and CSS files. A future native C server and optional client-side interop are separate milestones.

## Known limitations
- Element and attribute names are validated as ASCII case-insensitive: void elements are recognised in any case and duplicate attributes are rejected. Content models beyond void-element children, raw-text elements, foreign content, URL security, Unicode validity, and complete HTML parsing semantics are not yet supported or validated.
- Raw-text elements (`script`, `style`) are serialized with HTML character references. Browsers do not decode character references there, so author-provided JavaScript or CSS content cannot currently be represented correctly.
- The serializer has no character-encoding policy: output is raw bytes with no BOM and no `<meta charset>`, so browsers default `text/html` to windows-1252 and mangle non-ASCII text.
- The declarative `CHML_*` macros are only safe within one function body; their compound literals do not survive a return. See `docs/decisions/ADR-0004-components-as-c-functions.md`.
- CSS support is a deliberately limited subset. Current validation rejects semicolons/braces/newlines, including valid CSS values that contain these characters; no general CSS tokenizer/parser exists. It does not establish full CSS compatibility.
- CSS writer is not transactional: I/O errors can leave partial files. No allocation-failure injection tests yet.
- Declarative macros require at least one child for `CHML_ELEMENT`; empty elements use `CHML_LEAF`.
- Generated output has not undergone automated cross-browser, accessibility, or performance verification.

## Invariants
1. No required handwritten HTML/CSS/JS application source.
2. No proprietary C grammar or mandatory compiler fork.
3. Never claim browser compatibility or speed without measured evidence.
4. Keep security and ownership explicit; reject unsupported operations rather than silently altering semantics.

## Future work
HTML semantics and serializer hardening; CSS tokenization and at-rules; portable C23 builds; browser visual/a11y tests; server rendering; optional Wasm/browser interop.
