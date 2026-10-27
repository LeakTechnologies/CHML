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
- An architecture proposal must show both its successful behaviour and at least one **failure mode that distinguishes it from the alternatives**. A test that would also pass on the rejected design is not evidence.
- Preserve the web platform's semantics, CSS cascade, accessibility, and standard-browser operation.
- Keep third-party dependencies minimal and document official/version-specific references for adopted tools.
- Never delete or replace existing work without inspection; commit coherent tested units and report exact verification commands.
- Do not introduce a Chromium fork, custom browser standard, or custom C compiler as a prerequisite.

## Current state

M0.3 baseline with M0.4-A, M0.4-B and M0.4-C evaluated/complete; **M0.4-D is partially executed (Windows only)**. Void-element content and duplicate attributes are rejected case-insensitively at materialization; text is serialized by context (ordinary / RCDATA / raw text) with raw-text end-tag hazards rejected; attributes are semantically typed (string vs boolean); `<meta charset="utf-8">` is injected into an existing `<head>` and conflicting declarations are rejected; both writers are atomic (temporary file plus replace); verification gates exist for both host families and exit non-zero when a step was skipped. Stylesheet output remains a deliberately restricted CSS subset. This is not production-ready. Static example emits HTML and CSS, no client JS. The declarative macros are safe only within a single function body. See `docs/decisions/ADR-0004-components-as-c-functions.md`, `docs/compatibility-matrix.md`.

## Next unit of work

1. **Review ADR-0005 and settle the transaction API.** The builder is implemented and tested but experimental. Open questions are listed in the ADR; do not treat the API as final.
2. **Leak Technologies reference port** — only after the component architecture passes review. The deployed site at `https://leaktechnologies.dev` is the reference; its editable source has not been located. Preserve the homepage's two-variant rotating LEAK composition; do not replace it with a conventional hero.
3. **Allocation-failure injection** for the declarative materialization path.
4. Design an extensible CSS grammar and at-rule model.
5. Repository-history review follow-up: one commit message still contains a local absolute path (not sensitive); rewriting history is not authorized.

M0.4 is complete. The Linux gate runs automatically on every push to `master`.

For each unit, provide changed files, tests, known limitations, and evidence. Do not mark proposed ADRs accepted without review.
