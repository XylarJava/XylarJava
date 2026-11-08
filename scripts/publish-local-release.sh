#!/usr/bin/env bash
# Upload exactly the Windows EXE and Linux AppImage after building them locally.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
tag=${1:-v0.0.0.5-ALPHA}
exe=${2:-dist-single/XylarJava-Windows/XylarJava.exe}
appimage=${3:-XylarJava-Linux-x86_64.AppImage}
git check-ref-format "refs/tags/$tag"
for artifact in "$exe" "$appimage"; do
    [ -s "$artifact" ] || { echo "Missing release file: $artifact. Build both packages before publishing." >&2; exit 1; }
done
command -v gh >/dev/null
gh auth status

# Reuse an existing tag without moving it to another commit.
remote_tag=$(git ls-remote --tags origin "refs/tags/$tag")
if [ -z "$remote_tag" ]; then
    if ! git show-ref --verify --quiet "refs/tags/$tag"; then
        git tag -a "$tag" -m "XylarJava $tag"
    fi
    git push origin "refs/tags/$tag"
fi
if gh release view "$tag" >/dev/null 2>&1; then
    gh release upload "$tag" "$exe" "$appimage" --clobber
else
    gh release create "$tag" "$exe" "$appimage" --verify-tag --title "XylarJava $tag" --prerelease
fi
