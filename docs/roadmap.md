# CHML milestone roadmap

- **M0.3 (prototype):** valid C11 API, document materialization, basic escaping, declarative CSS subset, reproducible build.
- **M0.4:** negative tests, memory and sanitizer tests, atomic output, HTML void/raw-text and attribute handling, improved diagnostics.
- **M0.5:** real CSS grammar, media/supports/container at-rules, custom properties, cascade preservation; independent reference fixtures.
- **M1:** cross-browser reference website port, visual diff, accessibility checks, performance measurements with matched assets and hosting.
- **Later:** native C HTTP server, C/Wasm events, JS bridge, third-party ecosystem interop.

No milestone implies full HTML/CSS conformance until conformance tests establish it.

M0.4 incremental evidence: added document negative tests, early rejection of void
elements with content (now ASCII case-insensitive), and rejection of duplicate
attributes. Sanitizers, raw-text elements, boolean attributes, encoding
declaration, atomic output, and CSS parsing remain **open**.
