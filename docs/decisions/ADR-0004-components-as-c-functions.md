# ADR-0004 — Reusable components as ordinary C functions

Status: Proposed

## Question

Can CHML express reusable declarative components (navigation, cards, layout,
page templates) as ordinary C functions, while keeping the nested-literal syntax
that makes the authoring style pleasant?

## Evidence

Measured on 2026-10-27 with GCC 15.2.0, C11, against the current
`ChmlNodeDef` / `CHML_*` macro layer.

1. **Returning a `ChmlNodeDef` by value does not work.** `CHML_ELEMENT` builds
   `(const ChmlNodeDef[]){...}` as a compound literal with automatic storage
   duration. A function that returns such a struct returns a `.children` pointer
   into its own dead frame. In a probe, the children array changed after one
   further function call. This is undefined behaviour that happens to compile
   cleanly.
2. **`CHML_ELEMENT` cannot be used inside a component function at all.** A
   component that assigned `storage[0] = CHML_ELEMENT("h2", CHML_TEXT("Title"))`
   produced a `ChmlNodeDef` whose own `.children` pointed at the callee's dead
   compound literal. The subsequent `chml_document_add` dereferenced that pointer
   and the probe crashed with an access violation.
3. **Components do work in two shapes.** (a) Append straight to the document with
   `chml_document_add`; this is safe but cannot nest the result inside a caller's
   tree. (b) Fill caller-owned `ChmlNodeDef` storage and use `.text` on leaves
   instead of a `CHML_TEXT` child; this nests correctly and needs no library
   change, but abandons the nesting syntax and requires the caller to size arrays.

Conclusion: the declarative macro layer is single-function-scoped. Ordinary C
functions are mechanically usable as components only by defeating that layer.

## Proposed decision

Reusable components are ordinary C functions. They are **not** permitted to
return a `ChmlNodeDef` by value, and the `CHML_*` macros are documented as valid
only inside a single function body.

To keep components pleasant, add a build-into-document API so a component appends
into document-owned storage and returns a stable handle:

```c
ChmlNode *nav = chml_node_append(doc, parent, "nav", attrs, attr_count);
chml_node_append_text(doc, nav, "Projects");
```

Nodes would live in an arena owned by `ChmlDocument`, which removes per-node
`malloc` churn, makes allocation failure reportable once, and gives components a
lifetime that outlives the calling frame. The existing `ChmlNodeDef` path can
remain as the convenience form for single-function pages.

## Consequences

- Component functions become safe and nestable at the cost of a second authoring
  form; the API surface grows.
- An arena makes `chml_document_add`'s per-node allocation and the 128-depth
  recursion limit revisitable.
- This does not by itself make components pleasant; that must be judged by porting
  a real multi-page site, per M0.4/M0.5 exit criteria.

## Not decided here

Whether the macro layer should be kept at all, and whether components may take
arguments that are themselves nodes. Both need the site-port evidence.
