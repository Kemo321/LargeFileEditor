#!/usr/bin/env bash
#
# generate_test_file.sh — build a large, torture-test text file for LargeFileEditor.
#
# The editor's selling point is that it never loads the whole file into RAM, so a
# realistic test file has to be big AND structurally nasty. This script weaves
# together every code path the editor cares about:
#
#   * Sheer size            -> mmap / lazy LineManager cache (default 1 GiB).
#   * Extremely long lines  -> the 4096-char "hard soft-wrap" + horizontal scroll.
#   * UTF-8 multibyte runs  -> byte-boundary snapping in getLineChunk().
#   * Many short lines       -> virtual-line counting and vertical scrolling.
#   * Embedded sentinels    -> deterministic targets for Find / Replace / Replace All.
#   * Edge cases            -> empty lines, trailing tabs, an unterminated final line.
#
# It stays text-only on purpose: the editor rejects binary files on open.
#
# Usage:
#   ./generate_test_file.sh [OUTPUT_PATH] [SIZE_MIB]
#
#   OUTPUT_PATH   where to write the file        (default: ./bigfile.txt)
#   SIZE_MIB      approximate target size in MiB (default: 1024 = 1 GiB)
#
# Examples:
#   ./generate_test_file.sh                       # 1 GiB -> ./bigfile.txt
#   ./generate_test_file.sh /tmp/huge.txt 4096     # 4 GiB
#   ./generate_test_file.sh small.txt 16           # quick 16 MiB smoke test

set -euo pipefail

OUTPUT="${1:-./bigfile.txt}"
SIZE_MIB="${2:-1024}"

if ! [[ "$SIZE_MIB" =~ ^[0-9]+$ ]] || [[ "$SIZE_MIB" -eq 0 ]]; then
    echo "error: SIZE_MIB must be a positive integer (got '$SIZE_MIB')" >&2
    exit 1
fi

TARGET_BYTES=$(( SIZE_MIB * 1024 * 1024 ))

# A reusable ~1 MiB content block is built once, then appended in a loop. Generating
# the block in shell and repeating it with cat is far faster than emitting line-by-line.
BLOCK="$(mktemp)"
# The long line lives in its own file so we can splice an >4096-char run without
# tripping over shell argument limits.
LONGLINE="$(mktemp)"
trap 'rm -f "$BLOCK" "$LONGLINE"' EXIT

# --- one very long line: ~20000 chars, no interior newline ----------------------
#     Forces the LineManager to hard-wrap a single logical line into ~5 virtual
#     lines at the 4096-char boundary, exercising horizontal scrolling + getLineChunk.
{
    printf 'LONGLINE_START '
    for _ in $(seq 1 400); do
        printf 'the_quick_brown_fox_jumps_over_the_lazy_dog_0123456789 '
    done
    printf 'LONGLINE_END\n'
} > "$LONGLINE"

# --- the repeating content block ------------------------------------------------
{
    printf 'ASCII paragraph: the quick brown fox jumps over the lazy dog.\n'
    printf 'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do.\n'
    printf '\n'                                                        # empty line
    printf 'UTF-8 Polish: zażółć gęślą jaźń — ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż.\n'   # 2-byte runs
    printf 'UTF-8 emoji/CJK: 🚀🔥✅ 日本語 中文 한국어 — ohýbej můj.\n'    # 3/4-byte runs
    printf 'Tabs\tand\tcolumns\tand\ttrailing\twhitespace   \n'
    printf 'NEEDLE_HAYSTACK find-me sentinel for search tests.\n'      # Find target
    printf 'REPLACE_ME token appears here and is easy to swap out.\n'  # Replace target
    cat "$LONGLINE"
    printf 'Short.\n'
    printf 'a\nb\nc\nd\ne\n'                                          # tiny lines
} > "$BLOCK"

BLOCK_BYTES=$(wc -c < "$BLOCK")
REPEATS=$(( TARGET_BYTES / BLOCK_BYTES ))
[[ "$REPEATS" -lt 1 ]] && REPEATS=1

echo "Generating ~${SIZE_MIB} MiB -> ${OUTPUT}"
echo "  block size : ${BLOCK_BYTES} bytes"
echo "  repeats    : ${REPEATS}"

# --- header sentinel, body, footer sentinel -------------------------------------
{
    printf '===== LargeFileEditor TORTURE TEST FILE — top of file =====\n'
    printf 'FIRST_LINE_SENTINEL: jump-to-top should land here.\n\n'
} > "$OUTPUT"

PROGRESS_STEP=$(( REPEATS / 20 ))
[[ "$PROGRESS_STEP" -lt 1 ]] && PROGRESS_STEP=1
for (( i = 0; i < REPEATS; ++i )); do
    cat "$BLOCK" >> "$OUTPUT"
    if (( i % PROGRESS_STEP == 0 )); then
        printf '\r  progress   : %d%%' $(( i * 100 / REPEATS ))
    fi
done
printf '\r  progress   : 100%%\n'

# Footer: the LAST line is deliberately left WITHOUT a trailing newline to test
# the editor's handling of an unterminated final line.
printf '\n===== bottom of file =====\n' >> "$OUTPUT"
printf 'LAST_LINE_SENTINEL_NO_NEWLINE: jump-to-end should land here.' >> "$OUTPUT"

FINAL_BYTES=$(wc -c < "$OUTPUT")
echo "Done."
echo "  path  : ${OUTPUT}"
echo "  size  : ${FINAL_BYTES} bytes (~$(( FINAL_BYTES / 1024 / 1024 )) MiB)"
echo
echo "Try in the editor:"
echo "  - Open it (mmap, must stay responsive)."
echo "  - Find 'NEEDLE_HAYSTACK' and 'FIRST_LINE_SENTINEL'."
echo "  - Replace All 'REPLACE_ME' -> 'REPLACED_OK'."
echo "  - Scroll the 'LONGLINE_START ... LONGLINE_END' line horizontally."
