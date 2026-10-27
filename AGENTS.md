# CHML agent instructions

## Mission

CHML is a deliberately absurd but technically serious **ISO C-authored web platform**. Human-authored application pages, styles, and behavior should be possible entirely in C. Output must use ordinary browser standards. Preserve the joke without compromising correctness, accessibility, or security.

## Start here

Read `README.md`, `docs/architecture.md`, `docs/compatibility-matrix.md`, `docs/roadmap.md`, `docs/references.md`, and all ADRs before making changes. Inspect the actual code and run the test suite. Treat proposed architecture as provisional; tests are not certification.

## Non-negotiable rules

- Source is genuine ISO C (C11 baseline today, C23 target), not a C-like DSL. Compile with GCC and Clang under strict warnings.
- Declarative-first authoring over a small procedural core. Keep explicit ownership and error contracts.
- HTML/CSS/JS may be generated browser artifacts; do not require handwritten HTML/CSS/JS for CHML apps.
- Never claim full HTML/CSS compatibility, security, browser parity, or performance superiority without evidence.
- Preserve the web platform's semantics, CSS cascade, accessibility, and standard-browser operation.
- Keep third-party dependencies minimal and document official/version-specific references for adopted tools.
- Never delete or replace existing work without inspection; commit coherent tested units and report exact verification commands.
- Do not introduce a Chromium fork, custom browser standard, or custom C compiler as a prerequisite.

## Current state

M0.3 baseline with M0.4-A and M0.4-B complete. Void-element content and duplicate attributes are rejected case-insensitively at materialization; text is serialized by context (ordinary / RCDATA / raw text) with raw-text end-tag hazards rejected; attributes are semantically typed (string vs boolean); `<meta charset="utf-8">` is injected into an existing `<head>` and conflicting declarations are rejected; both writers are atomic (temporary file plus replace). Stylesheet output remains a deliberately restricted CSS subset. This is not production-ready. Static example emits HTML and CSS, no client JS. The declarative macros are safe only within a single function body (ADR-0004). See `docs/compatibility-matrix.md` for per-feature status and open questions.

## Next unit of work

1. **M0.4-C component architecture:** review ADR-0004, compare lifetime-safe builder designs, prototype nested components. This is a design gate: do not implement a public builder API without a decision.
2. **M0.4-D verification:** Linux sanitizer runner and strict Clang build; Clang is not installed and ASan/UBSan runtimes are absent on the current Windows/MinGW host. This also unblocks the POSIX atomic-write branch, which has never been executed.
3. **Allocation-failure injection** for the materialization and write paths.
4. Design an extensible CSS grammar and at-rule model; don't keep extending ad-hoc character rejection.
5. Port a licensed reference page; measure semantic, accessibility, visual, and payload parity across browsers.

Reviewed proposals awaiting a decision: ADR-0004 (build-into-document component API).

For each unit, provide changed files, tests, known limitations, and evidence. Do not mark proposed ADRs accepted without review.
