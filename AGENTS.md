# CHML agent instructions

## Mission

CHML is a deliberately absurd but technically serious **ISO C-authored web platform**. Human-authored application pages, styles, and behavior should be possible entirely in C. Output must use ordinary browser standards. Preserve the joke without compromising correctness, accessibility, or security.

## Start here

Read `README.md`, `docs/architecture.md`, `docs/roadmap.md`, `docs/references.md`, and all ADRs before making changes. Inspect the actual code and run the test suite. Treat proposed architecture as provisional; tests are not certification.

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

M0.3 baseline with partial M0.4 hardening. The document tree deep-copies definitions; void-element content and duplicate attributes are rejected case-insensitively at materialization. HTML escaping is basic, and the declarative macros are safe only within a single function body (ADR-0004). Stylesheet output supports a deliberately restricted CSS subset. This is not production-ready. Static example emits HTML and CSS, no client JS, and no encoding declaration. See `docs/decisions/ADR-0003-ownership-validation.md` and `ADR-0004` for unresolved risks.

## Next unit of work

1. Add sanitizer/negative tests, fault injection, and robust output failure handling. Sanitizers are currently unavailable on the Windows/MinGW host; verify on Linux or install LLVM.
2. Specify and implement HTML5 serialization semantics (a) raw-text elements so `script`/`style` content is not escaped, (b) boolean attributes, (c) an explicit character-encoding declaration, with independent fixtures.
3. Design an extensible CSS grammar and at-rule model; don't keep extending ad-hoc character rejection.
4. Port a licensed reference page; measure semantic, accessibility, visual, and payload parity across browsers.

Reviewed proposals awaiting a decision: ADR-0004 (build-into-document component API).

For each unit, provide changed files, tests, known limitations, and evidence. Do not mark proposed ADRs accepted without review.
