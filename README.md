# CFDojo

CFDojo is a desktop C++ application (code editor/runner) included in this repository.

## Requirements
- CMake (>= 3.15)
- C++ compiler (g++/clang++) with C++17 support
- Qt and QScintilla development libraries (project uses Qt widgets)

## Build (Debug)
From the repository root:

```bash
cmake -S . -B build-cfdojo -DCMAKE_BUILD_TYPE=Debug
cmake --build build-cfdojo --config Debug
```

## Run
After a successful build, run the main executable from the build directory:

```bash
./build-cfdojo/CFDojo
```

You can also run tests with CTest from the build directory:

```bash
cmake --build build-cfdojo --target test
# or
cd build-cfdojo && ctest --output-on-failure
```

## Project layout
- `src/` - application source
- `docs/` - documentation and quickstart
- `vendor/` - third-party sources (QScintilla)

## Releases (Qt bundled)

Pre-built binaries with Qt bundled are available on the [GitHub Releases](../../releases) page for **Linux**, **macOS**, and **Windows**. End-users do not need Qt installed.

| Platform | Format | Qt bundled via |
|----------|--------|----------------|
| Linux    | `.AppImage` | `linuxdeployqt` |
| macOS    | `.dmg`      | `macdeployqt`   |
| Windows  | `.exe` installer | `windeployqt` + NSIS |

### Creating a release

Push a version tag and GitHub Actions will build, bundle Qt, and publish a release automatically:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow is defined in `.github/workflows/release.yml`.

## Contributing
Please open issues or pull requests for changes. Follow existing code style and keep changes minimal and focused. Do not request other languages.

## License
See the `LICENSE` file in the repository root.
