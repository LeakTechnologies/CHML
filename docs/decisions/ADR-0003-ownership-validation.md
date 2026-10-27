# ADR-0003 — Definition ownership and validation boundaries

Status: PROPOSED (partially exercised in M0.4)

## Decision

`ChmlNodeDef` and its arrays are borrowed for the duration of `chml_document_add`. The document deep-copies their strings and nested definitions. Callers may release or leave scope after a successful add. `chml_document_destroy` frees all owned nodes. An unsuccessful add must leave the existing document unchanged.

Reject invalid element names and void elements with children or text during materialization, before file output. These checks do not constitute HTML Living Standard conformance.

## Remaining risks

- Allocation-failure injection and fuzzing are not implemented.
- HTML raw-text elements (`script`, `style`), comments, namespaces, doctype control, boolean attributes, and document-level structural constraints need explicit policies.
- Input size, recursion depth, integer overflow, embedded NUL, and Unicode validation need broader coverage.
- Output is not atomic; an I/O failure can leave a truncated file.
- A CSS declaration whitelist/blacklist is not a CSS parser. Current CSS restrictions reject valid syntax and must be replaced.

## Evidence required for acceptance

Fault-injection tests, sanitizer builds, HTML parsing fixtures, and cross-browser conformance tests. Until then this ADR remains proposed.

## Progress (2026-10-27, M0.4-A)

Addressed since this ADR was written: void-element children for any ASCII case
variant, duplicate attribute names, raw-text elements (`script`, `style`, `xmp`,
`iframe`, `noembed`, `noframes`) including rejection of end-tag hazards, RCDATA
elements (`title`, `textarea`), semantic boolean attributes, and an explicit
UTF-8 declaration policy. `<plaintext>` is rejected as unserializable.

Still open, unchanged by that work: allocation-failure injection and fuzzing,
comment nodes, namespaces/foreign content, doctype control, document-level
structural constraints, input size, embedded NUL and Unicode validation, atomic
output, and the CSS restriction listed above. The ADR stays proposed.
