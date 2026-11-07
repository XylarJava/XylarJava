#!/usr/bin/env bash
# Re-test a saved EXE on a clean Windows runner. Never compile or repackage.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
source_run=${1:?Usage: scripts/test-windows.sh RUN_ID [windows-package]}
artifact=${2:-windows-package}
[[ "$source_run" =~ ^[0-9]+$ ]] || { echo 'RUN_ID must be numeric' >&2; exit 1; }
[[ "$artifact" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid artifact name' >&2; exit 1; }
repo=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
artifact_id=$(gh api "repos/$repo/actions/runs/$source_run/artifacts" --paginate \
    --jq ".artifacts[] | select(.name == \"$artifact\" and .expired == false) | .id" | head -n 1)
if [ -z "$artifact_id" ]; then
    echo "Run $source_run has no saved '$artifact' artifact. The EXE was not uploaded or has expired." >&2
    echo 'The temporary runner files cannot be restored. No build, packaging or smoke test was started.' >&2
    exit 1
fi
if [ -n "$(git status --porcelain)" ]; then
    echo 'Commit the smoke-test changes before running this command.' >&2
    exit 1
fi
branch=$(git branch --show-current)
[ -n "$branch" ] || { echo 'Check out a branch before testing.' >&2; exit 1; }
git push origin "$branch"
sha=$(git rev-parse HEAD)
request="smoke-$source_run-$(date +%s)-$$"
gh workflow run release.yml --ref "$branch" -f target=windows -f request-id="$request" \
    -f smoke-run-id="$source_run" -f smoke-artifact="$artifact"
run_id=
for ((attempt=0; attempt<60; attempt++)); do
    run_id=$(gh run list --workflow release.yml --event workflow_dispatch --commit "$sha" --limit 30 \
        --json databaseId,displayTitle \
        --jq ".[] | select(.displayTitle == \"XylarJava packages - $request\") | .databaseId" | head -n 1)
    [ -n "$run_id" ] && break
    sleep 5
done
[ -n "$run_id" ] || { echo 'The smoke-test run did not appear.' >&2; exit 1; }
success=0
if gh run watch "$run_id" --exit-status; then success=1; else gh run view "$run_id" --log-failed >&2 || true; fi
gh run download "$run_id" --name windows-smoke-logs --dir "dist-single/smoke-logs/$run_id" || true
[ "$success" = 1 ] || exit 1
gh run download "$run_id" --name release-windows --dir dist-single/XylarJava-Windows
printf '\nVerified EXE: %s/dist-single/XylarJava-Windows/XylarJava.exe\n' "$PWD"
