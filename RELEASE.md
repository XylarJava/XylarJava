# Release packages

Builds target Windows x64 and Linux x86_64. The workflow in
`.github/workflows/release.yml` builds the client jar, launcher and packages,
runs the launcher tests, and checks package startup before uploading artifacts.

## Publish

Commit the changes, then run from the repository with GitHub CLI authenticated:

```bash
git pull --rebase origin main
scripts/publish-release.sh v0.0.5-alpha.1
```

Use a new version tag for each release. The script pushes the current commit and
tag, waits for the matching GitHub Actions build, downloads its Windows and Linux
packages, generates `SHA256SUMS`, and creates the release. Tags containing a hyphen
are published as prereleases. No release is created if a build or startup check fails.
The downloaded files remain in `dist/release/<tag>/`.

## Windows

The only distributed Windows file is `XylarJava.exe`. It includes .NET and a ZIP
of the launcher, Qt, native dependencies, plugins and client jar. On first launch
it extracts into `%LOCALAPPDATA%\XylarJava\versions\<payload-hash>\app` and keeps
a copy of the outer EXE beside that folder. Desktop and Start menu shortcuts
point to that installed EXE, so deleting the original download does not break them.
Both executable icons use `program_info/XylarJava.ico`.

Build on Windows after configuring CMake and building the client jar:

```powershell
./scripts/build-windows-single-exe.ps1 -BuildDir build -Configuration Release
```

The output is `dist-single/XylarJava-Windows/XylarJava.exe`. Native dependencies
are extracted at runtime; they do not need to be distributed beside the download.
Launcher data uses the normal persistent user location, outside the versioned payload.

## Linux

Build with the dependencies described in `CONTRIBUTING.md`, Qt plugins and
`appimagetool` installed:

```bash
cmake --preset linux
scripts/build-linux-appimage.sh
```

Outputs:

- `XylarJava-Linux-x86_64.AppImage`
- `dist/XylarJava-Linux-x86_64.tar.gz`

For an offline build, set `XYLARJAVA_APPIMAGE_RUNTIME` to a local x86_64 AppImage
runtime file. `XYLARJAVA_BUILD_JOBS` controls parallel compilation.

Start an AppImage after downloading:

```bash
chmod +x XylarJava-Linux-x86_64.AppImage
./XylarJava-Linux-x86_64.AppImage --appimage-extract-and-run
```

The extraction flag also works on systems without FUSE. For the tarball, extract
it into a new folder and run `./AppRun` there.

On first launch either package installs a persistent copy under
`${XDG_DATA_HOME:-$HOME/.local/share}/XylarJava/application`, then registers its icon
and `.desktop` entry in the user's XDG directories. The menu entry works after
the downloaded file or extracted folder is removed. This supports desktop
environments and application launchers that read XDG desktop entries; a window
manager without an application menu does not gain one. Downloading a file alone
cannot install a menu entry: it must first be executed. No administrator access
is needed.

Check integration with `python3 tests/packaging/test_linux_integration.py`.
