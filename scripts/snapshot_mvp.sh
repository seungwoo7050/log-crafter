#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <track> <mvp>" >&2
    exit 1
fi

track="$1"
mvp="$2"

src_dir="work/${track}"
dest_dir="snapshots/${track}/${mvp}"

if [[ ! -d "${src_dir}" ]]; then
    echo "Source directory '${src_dir}' does not exist" >&2
    exit 1
fi

if [[ -e "${dest_dir}" ]]; then
    echo "Destination '${dest_dir}' already exists" >&2
    exit 1
fi

mkdir -p "snapshots/${track}"
cp -a "${src_dir}" "${dest_dir}"
echo "Snapshot created at ${dest_dir}"
