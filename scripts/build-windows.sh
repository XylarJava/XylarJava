#!/usr/bin/env bash
# Build the current commit on a Windows runner and fetch the one-file executable.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
command -v gh >/dev/null
gh auth status
if [ -n "$(git status --porcelain)" ]; then
    echo 'Commit your changes before building Windows.' >&2
    exit 1
fi
branch=$(git branch --show-current)
[ -n "$branch" ] || { echo 'Check out a branch before building.' >&2; exit 1; }
git push origin "$branch"
sha=$(git rev-parse HEAD)
request="windows-${sha:0:12}-$(date +%s)-$$"
gh workflow run release.yml --ref "$branch" -f target=windows -f request-id="$request"

run_id=
for ((attempt=0; attempt<60; attempt++)); do
    run_id=$(gh run list --workflow release.yml --event workflow_dispatch --commit "$sha" --limit 30 \
        --json databaseId,displayTitle \
        --jq ".[] | select(.displayTitle == \"XylarJava packages - $request\") | .databaseId" | head -n 1)
    [ -n "$run_id" ] && break
    sleep 5
done
[ -n "$run_id" ] || { echo 'The Windows build did not appear in GitHub Actions.' >&2; exit 1; }
if ! gh run watch "$run_id" --exit-status; then
    gh run view "$run_id" --log-failed >&2 || true
    exit 1
fi
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
gh run download "$run_id" --name release-windows --dir "$stage"
[ -s "$stage/XylarJava.exe" ] || { echo 'The build did not produce XylarJava.exe.' >&2; exit 1; }
output="dist-single/XylarJava-Windows"
mkdir -p "$output"
cp "$stage/XylarJava.exe" "$output/XylarJava.exe"
printf '\nWindows executable: %s/%s/XylarJava.exe\n' "$PWD" "$output"
sha256sum "$output/XylarJava.exe"
