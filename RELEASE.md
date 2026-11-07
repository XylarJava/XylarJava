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

To publish just the two distributable files after building them locally:

```bash
scripts/publish-local-release.sh v0.0.0.4-ALPHA
```

This requires `dist-single/XylarJava-Windows/XylarJava.exe` and
`XylarJava-Linux-x86_64.AppImage`. It checks both files before accessing GitHub,
creates the tag only if missing, and creates the release or replaces its two
matching assets. An existing tag keeps its original commit. Optional second and
third arguments override the Windows and Linux file paths respectively.

## Windows

From Linux, build the committed source on a Windows GitHub Actions runner and
download the final executable with:

```bash
scripts/build-windows.sh
```

This needs authenticated GitHub CLI access. It pushes the current branch,
dispatches the Windows build, waits for its checks and downloads only
`dist-single/XylarJava-Windows/XylarJava.exe`. It does not publish a release or
change any version tags. A failed build prints its logs and stops.

The Windows build now saves its EXE as `windows-package` before the separate
`windows-smoke` job runs. Startup diagnostics are retained as `windows-smoke-logs`;
`release-windows` contains the same EXE after successful verification.
To repeat only the smoke test using a saved package from an earlier run:

```bash
scripts/test-windows.sh RUN_ID
```

No client compilation, launcher build, tests or EXE packaging runs in this mode.
The command first checks that `windows-package` exists in the selected run and
stops without starting any job if it is missing. An optional second argument
selects another saved artifact containing `XylarJava.exe`.
Files left only on a completed hosted runner cannot be recovered by this command.
Older runs that uploaded the EXE after a failed smoke test may have no saved
package to reuse.

The smoke test allows 180 seconds for first-run extraction and 60 seconds for a
second launch. It tests without the build SDK on PATH and records stdout, stderr,
the package hash and process/window diagnostics on timeout. The longer cold-start
limit does not establish that a previous timeout was caused by extraction.

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
