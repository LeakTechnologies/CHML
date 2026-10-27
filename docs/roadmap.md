# CHML milestone roadmap

- **M0.3 (prototype):** valid C11 API, document materialization, basic escaping, declarative CSS subset, reproducible build.
- **M0.4:** negative tests, memory and sanitizer tests, atomic output, HTML void/raw-text and attribute handling, improved diagnostics.
- **M0.5:** real CSS grammar, media/supports/container at-rules, custom properties, cascade preservation; independent reference fixtures.
- **M1:** cross-browser reference website port, visual diff, accessibility checks, performance measurements with matched assets and hosting.
- **Later:** native C HTTP server, C/Wasm events, JS bridge, third-party ecosystem interop.

No milestone implies full HTML/CSS conformance until conformance tests establish it.

M0.4 incremental evidence:

- Increment 2: document negative tests, case-insensitive void-element rejection,
  duplicate-attribute rejection.
- M0.4-A: context-aware text serialization (ordinary / RCDATA / raw text),
  raw-text end-tag hazard rejection, semantic boolean attributes, UTF-8
  declaration policy, hostile/corrupt input fixtures, negative controls.
  Evidence: 3/3 tests, 32 serialization fixtures, `-fanalyzer` clean.
- M0.4-B: atomic output for both writers, temporary cleanup on failure,
  artifact-preserving replacement, write-failure fixtures. Evidence: 4/4 tests,
  8 atomic-write checks, both controls reproduced the original defect.
- M0.4-C: component architecture **evaluated, not decided.** Four candidates
  measured outside the repository; D (transactional builder over an arena)
  recommended; ADR-0004 revised and still Proposed. No production code changed.
- M0.4-D: **satisfied.** Windows/GCC and Linux/GCC and Linux/Clang strict
  builds pass, ASan and UBSan run with proven instrumentation, the POSIX
  `rename` path executes (`backend=posix-rename`), and the atomic negative
  control discriminates for the expected reason. Runs `40548472675` and
  `40548731375` on Ubuntu 24.04.5, GCC 13.3.0, Clang 18.1.3.

M0.4 is complete. Remaining, not part of M0.4:

- ADR-0004 decision, and only then component implementation.
- Allocation-failure injection in the production materialization and write paths.
- CSS parsing remains M0.5.

See `docs/compatibility-matrix.md` for per-feature status.
