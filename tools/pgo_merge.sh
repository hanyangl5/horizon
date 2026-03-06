#!/usr/bin/env bash

# Usage: tools/pgo_merge.sh <profile_dir> [llvm-profdata]
set -euo pipefail

PROFILE_DIR="${1:?Usage: $0 <profile_dir> [llvm-profdata]}"
PROFDATA="${2:-llvm-profdata}"
OUTPUT="${PROFILE_DIR}/merged.profdata"

if [ ! -d "${PROFILE_DIR}" ]; then
    echo "Profile directory does not exist: ${PROFILE_DIR}" >&2
    exit 1
fi

shopt -s nullglob
RAW_FILES=("${PROFILE_DIR}"/*.profraw)
if [ "${#RAW_FILES[@]}" -eq 0 ]; then
    echo "No .profraw files found in ${PROFILE_DIR}" >&2
    exit 1
fi

"${PROFDATA}" merge -output="${OUTPUT}" "${RAW_FILES[@]}"
echo "Merged profile written to: ${OUTPUT}"
