# ADR-0002: Declarative-first hybrid API
Status: Proposed

## Decision
Expose caller-owned declarative `ChmlNodeDef` descriptions, materialized into a document-owned tree. Preserve a procedural core for dynamic composition. Style declarations are C data structures rather than handwritten CSS strings.

## Rationale
Readable nested source with explicit C-compatible ownership and reusable construction APIs.

## Open questions
Macro portability; large-tree construction costs; typed vs generic style declarations; structured error reporting; dynamic components.
