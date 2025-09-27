#!/usr/bin/env bash
#
# Sequence: SEQ0105
# Track: Shared
# MVP: Step C
# Change: Automate release note creation by prepending entries to CHANGELOG.md
#         using the MAJOR.MINOR.PATCH scheme outlined in VERSIONING.md.
# Tests: manual_usage_new_version
#
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <version> <title> [note ...]" >&2
  exit 1
fi

if [[ ! -f CHANGELOG.md ]]; then
  echo "CHANGELOG.md not found in repository root." >&2
  exit 1
fi

version="$1"
shift
release_title="$1"
shift || true
notes=("$@")

changelog="CHANGELOG.md"
entry_line=$(grep -n '^## \[' "$changelog" | head -n1 | cut -d: -f1 || true)
if [[ -z "$entry_line" ]]; then
  header_end=$(wc -l < "$changelog")
  tail_start=0
else
  header_end=$((entry_line - 1))
  tail_start=$entry_line
fi

tmp_file=$(mktemp)
trap 'rm -f "$tmp_file"' EXIT

if [[ "$header_end" -gt 0 ]]; then
  head -n "$header_end" "$changelog" > "$tmp_file"
else
  : > "$tmp_file"
fi

date_utc=$(date -u +%Y-%m-%d)
{
  echo
  echo "## [$version] - $date_utc - $release_title"
  if [[ ${#notes[@]} -gt 0 ]]; then
    for note in "${notes[@]}"; do
      echo "- $note"
    done
  else
    echo "- (no additional notes provided)"
  fi
  echo
} >> "$tmp_file"

if [[ $tail_start -gt 0 ]]; then
  tail -n +"$tail_start" "$changelog" >> "$tmp_file"
fi

mv "$tmp_file" "$changelog"
trap - EXIT

echo "Added CHANGELOG entry for version $version"
