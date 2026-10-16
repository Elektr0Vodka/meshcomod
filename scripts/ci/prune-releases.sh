#!/usr/bin/env bash
#
# Rolling release retention: keep the newest N GitHub Releases for a given track
# (by tag prefix) and delete the older ones — including their git tags and the
# attached binary assets — so the release list (and the repo's tag namespace)
# stays small while GitHub Releases are the source of truth for the bins.
#
# Tracks are identified by tag prefix, e.g.:
#   companion-v1.16.0.1   -> prefix "companion-"
#   repeater-v1.16.0.1    -> prefix "repeater-"
#   room-server-v1.16.0.1 -> prefix "room-server-"
#   touch-pre-alpha_17    -> prefix "touch-"
#
# Usage:
#   scripts/ci/prune-releases.sh <tag-prefix> [keep_count] [kind]
#
#   kind: which releases on the track are counted and pruned.
#     all         every release (default, the original behaviour)
#     stable      only releases not marked as pre-release
#     prerelease  only pre-releases
#   Calling it once with "stable" and once with "prerelease" keeps a separate
#   window for each, so a run of pre-releases cannot push stable ones out.
#
# Env:
#   GH_TOKEN / GITHUB_TOKEN  required (gh auth) — provided automatically in CI.
#   DRY_RUN=1                print what would be deleted, delete nothing.
#
# Requires the GitHub CLI (`gh`), preinstalled on GitHub-hosted runners.

set -euo pipefail

PREFIX="${1:?usage: prune-releases.sh <tag-prefix> [keep_count] [kind]}"
KEEP="${2:-5}"
KIND="${3:-all}"
DRY_RUN="${DRY_RUN:-0}"

case "$KIND" in
  all)        KIND_FILTER="true" ;;
  stable)     KIND_FILTER=".isPrerelease | not" ;;
  prerelease) KIND_FILTER=".isPrerelease" ;;
  *)
    echo "error: kind must be all, stable or prerelease (got '${KIND}')" >&2
    exit 1
    ;;
esac

if ! command -v gh >/dev/null 2>&1; then
  echo "error: gh (GitHub CLI) not found" >&2
  exit 1
fi

echo "Pruning ${KIND} releases with tag prefix '${PREFIX}', keeping newest ${KEEP} (DRY_RUN=${DRY_RUN})"

# Newest-first list of tags on this track. Sort by createdAt desc so "newest N"
# is deterministic regardless of GitHub's default ordering.
mapfile -t tags < <(
  gh release list --limit 300 --json tagName,createdAt,isDraft,isPrerelease \
    --jq "[.[] | select(.tagName | startswith(\"${PREFIX}\")) | select(${KIND_FILTER})]
          | sort_by(.createdAt) | reverse | .[].tagName"
)

total="${#tags[@]}"
if [ "$total" -le "$KEEP" ]; then
  echo "Found ${total} ${KIND} release(s) on this track; nothing to prune (<= ${KEEP})."
  exit 0
fi

idx=0
pruned=0
for tag in "${tags[@]}"; do
  idx=$((idx + 1))
  if [ "$idx" -le "$KEEP" ]; then
    echo "  keep   ${tag}"
    continue
  fi
  if [ "$DRY_RUN" = "1" ]; then
    echo "  PRUNE  ${tag}  (dry-run, not deleted)"
  else
    echo "  PRUNE  ${tag}"
    # --cleanup-tag also removes the underlying git tag; --yes skips the prompt.
    gh release delete "$tag" --yes --cleanup-tag
  fi
  pruned=$((pruned + 1))
done

echo "Done. ${total} ${KIND} release(s) on track, kept ${KEEP}, pruned ${pruned}."
