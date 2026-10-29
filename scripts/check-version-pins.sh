#!/usr/bin/env bash
# Checks that every version pin in this repo (external_components ref:, the matching
# "@vX.Y.Z = this release" comment, *_project_version: substitutions, the "Project : ... | vX.Y.Z"
# file header, and the README version banner/prose) points at one target version.
#
# Run before tagging a release - these pins live in several files and it's easy to bump most of
# them and miss one (it has happened repeatedly: the main example YAML and the README banner/prose
# are the usual stragglers).
#
# Usage: scripts/check-version-pins.sh v2.3.0

set -euo pipefail

target="${1:-}"
if [[ -z "$target" ]]; then
  echo "Usage: $0 vX.Y.Z" >&2
  exit 2
fi
target_num="${target#v}"

cd "$(dirname "$0")/.."

# Files that legitimately mention other versions (changelog history, per-register findings,
# per-FSV maps, error-code docs, license notice) are excluded by NAME - they are not "current
# version" pins. (Excluding by content substring instead would also strip banner lines that just
# happen to link to CHANGELOG.md in their text.)
list_files() {
  find . -type f \( -name '*.yaml' -o -name '*.md' \) \
    -not -path '*/.git/*' \
    -not -name 'CHANGELOG.md' \
    -not -name 'FINDINGS.md' -not -name 'FINDINGS.pl.md' \
    -not -name 'FSV_MAP.md' -not -name 'FSV_MAP.pl.md' \
    -not -name 'ERROR_CODES.md' -not -name 'ERROR_CODES.pl.md' \
    -not -name 'NOTICE'
}

fail=0

check() {
  local pattern="$1" desc="$2"
  local hits
  hits=$(list_files | xargs grep -nE "$pattern" 2>/dev/null || true)
  [[ -z "$hits" ]] && return
  while IFS=: read -r file line content; do
    if [[ "$content" != *"$target"* && "$content" != *"$target_num"* ]]; then
      echo "STALE ($desc): $file:$line: $content"
      fail=1
    fi
  done <<< "$hits"
}

check 'ref:[[:space:]]*v[0-9]+\.[0-9]+\.[0-9]+'                        "external_components ref"
check '@v[0-9]+\.[0-9]+\.[0-9]+[[:space:]]*='                          "ref comment"
check '_project_version:[[:space:]]*"[0-9]+\.[0-9]+\.[0-9]+"'         "project version substitution"
check '# Project[[:space:]]*:.*\|[[:space:]]*v[0-9]+\.[0-9]+\.[0-9]+' "file header version"
check '> \*\*v[0-9]+\.[0-9]+\.[0-9]+\.\*\*'                            "README version banner"

if [[ $fail -ne 0 ]]; then
  echo
  echo "Some version pins do not match $target. Fix them before tagging/pushing."
  exit 1
fi

echo "All version pins match $target."
