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
