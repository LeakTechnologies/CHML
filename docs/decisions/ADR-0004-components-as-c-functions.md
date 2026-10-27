# ADR-0004 — Reusable components as ordinary C functions

Status: **Proposed** (evaluated 2026-10-27; not accepted)

## Question

Can CHML express reusable declarative components (navigation, cards, layout,
page templates) as ordinary C functions that nest, return stable handles, and
fail safely — while keeping the nested-literal authoring style that makes the
project pleasant?

## Prior evidence (M0.4-A, unchanged)

Measured against the shipped `CHML_*` macros:

1. **Returning a `ChmlNodeDef` by value is unsafe.** `CHML_ELEMENT` creates a
   compound literal with automatic storage in the callee's frame. A returned
   struct carries a `.children` pointer into dead stack; the array changed after
   one further function call. It compiles cleanly under
   `-Wall -Wextra -Wpedantic`.
2. **`CHML_ELEMENT` cannot be used inside a component at all.** A component that
   assigned `storage[0] = CHML_ELEMENT("h2", CHML_TEXT("T"))` produced a node
   whose `.children` pointed at the callee's dead compound literal, and
   `chml_document_add` crashed with an access violation on the dangling pointer.
3. **Text is a child node, not an element field.** `CHML_TEXT` produces a node
   with `tag == NULL`. Any component model must represent both "element with
   `.text`" and "text child", or it will silently drop content.

## Candidates evaluated

| | Design | Storage | Transaction |
| --- | --- | --- | --- |
| A | Arena with append-into-parent | Document-owned arena blocks | None |
| B | Caller-provided fixed storage | Caller arrays, explicit capacity | Mark/rollback |
| C | Two-pass measure-then-emit | None; streams to a sink | None possible |
| D | Transactional builder over arena | Document-owned arena blocks | Mark/rollback |

B deliberately included a transaction so the experiment could test whether
transactionality depends on the storage strategy. It does not.

## Method

One harness runs all four candidates on an identical page: `html > (head >
title), (body > nav, main > section > 2 × card)`, where `project_card` is a
separate function called from `build_section`, which is called from
`build_page`. Each candidate must:

- build that page with the component crossing two function boundaries;
- append to the page *after* a handle has been returned, then re-derive the same
  node from the root and compare addresses;
- report every result through a shared `Metrics` struct;
- serialize with one shared serializer so output can be compared byte-for-byte.

Failure injection:

- **A and D:** allocations go through an instrumented allocator. Block size is
  set to 1 so every node and string allocation becomes an injectable point;
  otherwise a 4 KiB arena makes only 2 allocator calls for the whole page and
  nothing inside the component is injectable.
- **B:** remaining capacity is clamped to `used + j` slots and `used + t` text
  bytes, so exhaustion lands at every point inside the component.
- After each injected failure, the parent's `child_count` and `last_child` are
  compared with their pre-component values.

## Results

GCC 15.2.0, C11, MinGW-w64/UCRT, Windows.

| candidate | handles | nested | address stable | clean rollback | failure points | bytes | ns/page |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A arena, no transaction | yes | yes | yes | **no (4/5 partial)** | 5 | 4152 | 528 |
| B fixed storage | yes | yes | yes | yes | 20 | 1792 | 230 |
| C two-pass | **no** | yes | n/a | **no (38 B partial)** | 0 | 0 | 260 |
| D transactional arena | yes | yes | yes | yes | 5 | 4152 | 550 |

All four emit an identical 240-byte document, so the comparison is not confounded
by output differences. `ns/page` is from `clock()` over 400 000 builds and should
be read as ±20 %.

Negative controls:

- A reallocating contiguous node array (`neg_realloc.c`) moves its base from
  `0x…AD0` to `0x…FE0` and the previously returned handle no longer points into
  the live array. This is the design the project must not choose.
- Candidate D with `chml_rollback` removed (`cand_d_norollback.c`) degrades to
  A's behaviour: 4 of 5 failures leave a partial card attached, and the harness
  fails. The rollback check is therefore discriminating, not decorative.

## Findings

1. **A is not safe enough on its own.** Append-into-parent gives stable handles
   and good ergonomics but leaves a partially built subtree attached whenever an
   allocation fails mid-component. 4 of 5 injected failures did exactly that.
   A component author would have to hand-write undo logic.
2. **Transactionality is orthogonal to storage.** B achieved clean rollback at
   all 20 capacity points using caller storage, because the mark/rollback
   mechanism operates on the parent link, not on the allocator. The transactional
   property comes from the API and failure model, not from the arena.
3. **C is not a component API.** With no tree there are no handles, so a
   component cannot return a node, cannot be appended to later, and cannot be
   reordered. An undersized sink leaves partial output. It is a plausible *final
   serialization* strategy, not an authoring model.
4. **Arena failure is coarse-grained.** A realistic arena made 2 allocator calls
   for a 13-node page. Component-level allocation failure therefore means "block
   exhausted", not "this node failed". Any transactional guarantee is only as
   fine as the block boundary.
5. **B's capacity is a hard cliff.** 1792 bytes reserved for 13 nodes, and
   exhaustion is a runtime error the caller cannot recover from by retrying.
6. **Existing declarative definitions can drive a builder.** A converter over
   `ChmlNodeDef` reproduced the shipped writer's output byte-for-byte
   (`<div><h2>Hello</h2><p>World</p></div>`). The two authoring forms can coexist.
7. **The prototypes do not model attributes.** The shipped writer emits
   `CHML_ATTR` and boolean attributes; the builder prototypes handle tags and
   text only. This is a real gap, not an oversight to paper over.

## Handle stability — definition

Adopt both properties for **committed** nodes, until the document is destroyed:

- **Stable identity:** the node remains the same logical node; its tag, text and
  position do not change because other nodes were added.
- **Stable address:** the `ChmlNode *` returned to a caller remains a valid
  pointer to that node. No design may move committed nodes.

Handles obtained inside a rolled-back transaction, or after
`chml_builder_destroy`, are invalid and must not be used. This excludes any
`realloc`-grown contiguous node array, which is exactly why it is now a negative
control rather than a candidate.

## Recommended architecture

D — a transactional builder over document-owned storage — built on A's arena.
Not because transactions require an arena, but because the arena gives stable
addresses without per-node `malloc`, and D adds the failure model on top.

Sketch (illustrative, not approved for implementation):

```c
typedef struct ChmlBuilder ChmlBuilder;
typedef struct ChmlNode    ChmlNode;
typedef struct ChmlMark    ChmlMark;

ChmlBuilder *chml_builder_create(void);
void         chml_builder_destroy(ChmlBuilder *);

ChmlStatus chml_begin      (ChmlBuilder *, ChmlNode *parent, const char *tag, ChmlNode **out);
ChmlStatus chml_add_text   (ChmlBuilder *, ChmlNode *parent, const char *tag, const char *text);
ChmlStatus chml_add_text_node(ChmlBuilder *, ChmlNode *parent, const char *text);

ChmlMark   chml_mark     (ChmlBuilder *, ChmlNode *parent);
void       chml_rollback (ChmlMark *);
```

Component shape:

```c
static ChmlStatus project_card(ChmlBuilder *b, ChmlNode *parent,
                               const char *name, const char *desc,
                               ChmlNode **out)
{
    ChmlMark mark = chml_mark(b, parent);
    ChmlNode *card = NULL;
    ChmlStatus st = chml_begin(b, parent, "article", &card);
    if (st == CHML_OK) st = chml_add_text(b, card, "h2", name);
    if (st == CHML_OK) st = chml_add_text(b, card, "p", desc);
    if (st != CHML_OK) { chml_rollback(&mark); return st; }
    if (out) *out = card;
    return CHML_OK;
}
```

The existing `chml_document_add` / `CHML_*` path stays as the single-function
convenience form. The builder is the cross-function form.

## Unresolved questions

1. **Attribute support.** How does the builder carry `ChmlAttrDef`, including
   boolean versus string kinds? Production already distinguishes them; the
   prototype does not.
2. **Rollback ergonomics.** Explicit `chml_mark`/`chml_rollback` at every early
   return is correct but easy to forget. A `CHML_TRY` macro can bundle it, at the
   cost of hiding control flow from a reader. C has no destructors, so no option
   is free. Not decided.
3. **Failure granularity.** Should each component start a new arena block so its
   failure boundary is exact, at the cost of wasted tail space?
4. **Block sizing policy.** 4096 bytes gave 4152 bytes for a 13-node page; the
   policy determines both overhead and failure granularity.
5. **Do components accept a `ChmlNodeDef` subtree** as well as builder calls?
   The compatibility check shows the conversion works.
6. **Identity exposure.** The prototype tracked an `unsigned id` for identity
   assertions. Production may not need to expose it.
7. **Coexistence rules.** Can a document be built partly with
   `chml_document_add` and partly with the builder, and what order does
   serialization use?
8. **Thread safety.** Single-threaded by assumption; no locking is designed.

## Evidence location

Experimental code is kept **outside** this repository, alongside its own
`README.md` recording the exact build and run commands and the measured results.

Nothing in this ADR has been implemented in `include/`, `src/`, or `tests/`.

## Not decided here

- Whether the builder replaces or supplements `chml_document_add`.
- Whether `CHML_*` macros remain part of the recommended API.
- Whether component boundaries should be arena-block boundaries.
- Any of the unresolved questions above.
