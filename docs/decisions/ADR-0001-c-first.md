# ADR-0001: ISO C as the authoring language
Status: Proposed

## Decision
CHML source must compile as ordinary ISO C. The current proof targets C11 for compiler coverage, with C23 as the long-term language target. Browser-native HTML and CSS are emitted artifacts, not mandatory authored source files.

## Rationale
Retains standard compiler tooling, the central project premise, and compatibility with existing browsers without a custom rendering engine.

## Trade-offs
C memory ownership, macro ergonomics, error handling, and browser interoperability require careful design. Generated web output can never be assumed faster merely because it originated in C.
