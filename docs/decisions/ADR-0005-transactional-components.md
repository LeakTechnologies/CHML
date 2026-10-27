# ADR-0005 — Transactional component construction

Status: **Proposed** (M0.5-A implementation exists; not accepted)

## Context

ADR-0004 accepts document-owned storage and stable committed handles. It does
not decide how a component is built before it becomes part of the document. This
ADR records the model implemented in M0.5-A and the questions it exposed.

## Decision (proposed)

**Detached, transactional construction.**

1. `chml_builder_begin` opens a *session*: a separate arena region. Nothing in
   the session is reachable from the document.
2. Components are ordinary C functions that add nodes inside the open session.
   They may nest freely; they share the session.
3. `chml_builder_attach` links the session root into the document and **splices
   the session's blocks into the document region**. This is the ownership
   transfer; it cannot fail after the link succeeds, and handles keep their
   addresses.
4. `chml_builder_discard` frees the whole session region. A failed or abandoned
   component therefore cannot leave a partial subtree attached, and its memory
   is reclaimed rather than retained.
5. `chml_builder_mark` / `chml_builder_rollback` provide a finer checkpoint
   inside an open session, for a component that wants to fail without discarding
   its siblings' work.

## Constraints that fell out of the implementation

These are not stylistic preferences; each was forced by a defect found while
building M0.5-A.

1. **Construction is session-scoped.** Allowing `chml_builder_element` on an
   already-attached node moves allocation into the document region, where a
   rollback cannot reclaim it without risking committed blocks. The builder now
   rejects construction on document-owned nodes; content is added by attaching a
   new session to them.
2. **`begin` must rewind its own partial work.** `begin` can fail after
   allocating its root (for example when copying the tag string). Because the
   session never opens, the caller has no handle with which to discard it, so the
   block would be unreachable but retained. `begin` now restores the region
   checkpoint before returning.
3. **A mark must capture capacity, not just the pointer.** Restoring
   `children` without `child_cap` leaves `child_count < child_cap` with a NULL
   array, and the next append writes through NULL. The mark records and restores
   both.
4. **Allocation granularity is per block, not per node.** A realistic 4 KiB
   arena performs no allocator call for a small component, so "fail at every
   allocation point" is unreachable unless the block size is forced small. Tests
   set a block size of 1 to make failure points addressable.

## Failure semantics

| Event | Document | Component | Handles |
| --- | --- | --- | --- |
| Component fails before attach | untouched | discarded | invalid |
| Rollback inside a session | untouched | rewound to mark | nodes after the mark invalid |
| Attach fails (allocation) | untouched | session still open | still valid, caller discards |
| Attach succeeds | subtree committed | ownership transferred | stable until destroy |
| Foreign handle passed as parent | rejected | unchanged | unchanged |
| Detached node passed as attach parent | rejected | unchanged | unchanged |

## Unresolved questions

1. **One session at a time.** Nested components share it, so a nested component
   cannot independently commit or discard without affecting its host. Is a stack
   of sessions needed, or is mark/rollback sufficient?
2. **`CHML_TRY` ergonomics.** Callers must remember to discard on every early
   return. A macro can bundle it, at the cost of hiding control flow. Undecided.
3. **Block sizing.** 4096 bytes gives few failure points and some tail waste;
   smaller blocks give finer failure boundaries and more allocator traffic.
4. **Attribute mutation after attach.** Currently impossible; attributes must be
   set inside the session. Is that acceptable for future dynamic use?
5. **Cross-document transfer.** Rejected today. A documented transfer contract
   may be needed for templates shared between documents.
6. **Handle validation.** A discarded handle is invalid, but detecting misuse is
   not attempted; using one is undefined.
7. **Thread safety.** Single-threaded by assumption.

## API review (M0.5-A verification, 2026-10-27)

### Detached handle lifetime and stale handles

A handle from a discarded session is invalid. The API prohibits its use, and
**it is not detected**: the pointer may point into reclaimed or reused heap, so
dereferencing it is undefined behaviour. What the builder *does* reject is a
pointer that belongs to no live region of this builder — a foreign handle, or a
handle from another builder — because that check is a range scan over live
blocks and never dereferences the candidate.

Consequence: `CHML_ERR_ARGUMENT` on a foreign handle is a real guarantee.
"Safe rejection of arbitrary stale pointers" is **not** a guarantee and must not
be claimed. A future revision could carry a generation tag in the handle to make
this detectable; that is out of scope for M0.5-A.

### Attachment atomicity

Ordering inside `chml_builder_attach`:

1. validate `root` against the session root;
2. resolve and validate `parent` (document-owned only);
3. append the root to the parent's child list or the document root list — the
   only fallible step, because it may grow an array;
4. splice the session blocks into the document region — pointer work only;
5. close the session.

Every validation and allocation precedes the commit point, so a failure returns
before step 4 and the document is untouched. This is now asserted directly: at
each of the 15 injected failure points the committed document is re-serialized
and compared byte-for-byte with its pre-component form.

### Memory reclamation

Rollback and discard both return the builder's block accounting to its
pre-component value, and CI runs the same test under ASan (leak detection on)
and UBSan. Caveat: the byte counter measures **block** allocations, not
individual node allocations, so it is a coarse instrument; the sanitizer run is
the stronger evidence.

### Session lifetime

The single-session restriction is retained. Nested components share the open
session, which is what makes composition work; multiple concurrent sessions would
add a stack discipline with no demonstrated need.

### Attribute mutation after attachment

Not supported. Attributes must be set inside the session, before attach. Adding
mutability later would need to preserve the "committed nodes never move"
guarantee while allowing attribute arrays to grow — likely a per-node heap
attribute list rather than a region array.

### Authoring ergonomics observed

- The component functions read well: `project_card_add(b, main, "CHML", "…", "#c")`.
- The failure path is the weak point: every component must remember to roll back,
  and every caller to discard. This is the open question, not the construction
  syntax.
- `chml_builder_mark` taking a parent couples the checkpoint to a specific node;
  a parameterless mark would be simpler but cannot restore the child list.

## Ergonomics investigation (M0.5-B)

Three ways to write the same failing-capable component, all over the existing
builder and all inside `examples/ergonomics/main.c` — no library change:

| Approach | Cleanup sites in the component | Notes |
| --- | --- | --- |
| A. single exit with `goto cleanup` | 1 explicit `chml_builder_rollback` | Portable, obvious; but a newly added early return silently skips cleanup unless it also uses `goto` |
| B. structured macro (`CHML_TRY_ROLLBACK`) | 0 | Portable ISO C; terse; but the macro captures `b` and `mark` from the enclosing scope, so the rollback is invisible at the call site, and forgetting to wrap one call fails silently |
| C. scoped callback (`chml_component_scope`) | 0 | Portable; the body never sees the mark, so it cannot forget; costs one function pointer, one `void *ctx`, and a second function per component |

Executable evidence: each approach was failed at every allocation point inside
the component (23 points with block size forced to 1). All three reclaimed the
session to its baseline byte count at every point.

### Recommendation

Adopt **C** for component bodies that can fail, and keep **A** for callers.
Reject **B** as the primary style: it works, but the failure mode of a missed
wrapper is silent, which is the property we are trying to eliminate.

C does not by itself solve the *caller's* discard obligation. The analogous
step is a session-scope helper that opens a session, runs a body, and either
attaches or discards — removing the last duplicated cleanup from callers. That
should be prototyped against the real site port before being proposed as API.

This is not yet accepted. The investigation shows the shape is viable; it does
not show it is pleasant at site scale with nested components and content from a
real page tree.

## Evidence

`tests/builder_test.c` — exact serialization, byte-identical parity with the
declarative path, discard reclamation, rollback reclamation, 15 injected
allocation-failure points, ownership rejections, stable handles after mutation,
and the three reference components.

Negative control: making `chml_builder_rollback` a no-op fails three checks
(memory reclamation, rolled-back subtree absent, orphaning), so the tests
distinguish rollback from doing nothing.

## Not decided

The public shape of this API. It is experimental until reviewed.
