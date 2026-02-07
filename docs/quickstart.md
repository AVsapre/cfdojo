# CF Dojo — Quick Start & Guide

CF Dojo is a local competitive programming IDE focused on fast testing, stress checks, and quick iteration.

## Getting started

1. **Start in the editor**
   - CF Dojo opens directly into the main IDE window.
2. **Open or import a problem**
   - Use **File → Open** for a `.cpack`, or
   - Use Competitive Companion to send a problem directly.
3. **Write your solution**
   - Edit `solution.cpp` in the main editor.
4. **Run tests**
   - Use **Run All** to validate all test cases.
5. **Stress test**
   - Provide `brute.cpp` + `generator.cpp`, then run stress testing.

## File format (.cpack)

`.cpack` is a single problem archive. It stores:

- `solution.cpp` (solution code)
- `template.cpp` (optional, contains `//#main`)
- `brute.cpp` and `generator.cpp` (optional, for stress tests)
- `problem.json` (metadata from Competitive Companion)
- `testcases.json` (tests and timeout)

## Core workflow

### New / Open / Save
- **New** asks for language (`C++`, `Java`, `Python`) and then asks where to save the `.cpack`.
- **Open** supports `.cpack` and plain text files, and auto-detects language from common extensions.
- **Save** writes a `.cpack`. If you opened a text file, you can convert to `.cpack` at any time.

### Plain text mode
Opening a non-`.cpack` file puts the app in **plain text mode** (limited features).
Use the banner in the editor to **Convert to CPack** when you want templates and tests.

### Test cases
- Each test case has input and expected output.
- **Run All** executes every case.
- You can add/remove cases freely.

### Stress testing
Stress testing compares your solution to a brute force reference using randomized input.
Provide:
- `solution.cpp`
- `brute.cpp`
- `generator.cpp`

### Templates
`template.cpp` can include `//#main` to insert your solution.
If transclusion is enabled and the marker is missing, the app will warn you.
The setting **Template view enabled by default** only affects newly created/opened files; it does not change the current file immediately.

### Copy solution
The **copy** button builds the final output using the template and copies it to the clipboard.

### Autosave & recovery
Changes are periodically autosaved. On restart, CF Dojo can restore your last session.

## Troubleshooting

- **“Unsupported CPack format”**: the file isn’t a valid `.cpack`.
- **Template warnings**: add `//#main` or disable transclusion.
- **No tests showing**: open `testcases.json` and verify it’s valid JSON.

---

If you want more features added, open an issue in the repository.
