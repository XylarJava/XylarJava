#!/usr/bin/env bash
# Run after committing the release changes; builds packages on GitHub before publishing them.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
tag=${1:?Usage: scripts/publish-release.sh vX.Y.Z}
[[ "$tag" =~ ^v[0-9]+(\.[0-9]+){2,3}([.-][A-Za-z0-9.-]+)?$ ]] || { echo 'Use a tag such as v0.0.0.5-ALPHA or v1.0.0' >&2; exit 1; }
command -v gh >/dev/null
gh auth status
if [ -n "$(git status --porcelain)" ]; then
    echo 'Commit your changes before publishing.' >&2
    exit 1
fi
sha=$(git rev-parse HEAD)
if git show-ref --verify --quiet "refs/tags/$tag"; then
    [ "$(git rev-list -n 1 "$tag")" = "$sha" ] || { echo 'Tag points to another commit' >&2; exit 1; }
else
    git tag "$tag"
fi
git push origin HEAD
git push origin "refs/tags/$tag"

run_id=
for ((attempt=0; attempt<60; attempt++)); do
    run_id=$(gh run list --workflow release.yml --commit "$sha" --event push --limit 20 \
        --json databaseId,headBranch --jq ".[] | select(.headBranch == \"$tag\") | .databaseId" | head -n 1)
    [ -n "$run_id" ] && break
    sleep 5
done
[ -n "$run_id" ] || { echo "No build found for $tag. Check GitHub Actions." >&2; exit 1; }
gh run watch "$run_id" --exit-status

output="dist/release/$tag"
mkdir -p "$output"
download=$(mktemp -d)
trap 'rm -rf "$download"' EXIT
gh run download "$run_id" --name release-windows --dir "$download/windows"
gh run download "$run_id" --name release-linux --dir "$download/linux"
for filename in XylarJava.exe XylarJava-Linux-x86_64.AppImage XylarJava-Linux-x86_64.tar.gz; do
    mapfile -t matches < <(find "$download" -type f -name "$filename")
    [ "${#matches[@]}" = 1 ] || { echo "Missing or ambiguous artifact: $filename" >&2; exit 1; }
    cp "${matches[0]}" "$output/$filename"
done
(cd "$output" && sha256sum XylarJava.exe XylarJava-Linux-x86_64.AppImage XylarJava-Linux-x86_64.tar.gz > SHA256SUMS)
release_flags=()
[[ "$tag" != *-* ]] || release_flags+=(--prerelease)
gh release create "$tag" \
    "$output/XylarJava.exe" "$output/XylarJava-Linux-x86_64.AppImage" \
    "$output/XylarJava-Linux-x86_64.tar.gz" "$output/SHA256SUMS" \
    --verify-tag --title "XylarJava $tag" --generate-notes "${release_flags[@]}"
