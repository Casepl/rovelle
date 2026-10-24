#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <rovelle-executable>" >&2
    exit 2
fi

rovelle=$1
case "$rovelle" in
    /*) ;;
    *) rovelle=$(cd "$(dirname "$rovelle")" && pwd)/$(basename "$rovelle") ;;
esac

test_directory=$(mktemp -d "${TMPDIR:-/tmp}/rovelle-manifest.XXXXXX")
trap 'rm -rf "$test_directory"' EXIT HUP INT TERM

source_file=$test_directory/source.txt
printf 'Rovelle manifest verification test\n' >"$source_file"

count_objects()
{
    find "$1/objects" -type f 2>/dev/null | wc -l | tr -d ' '
}

valid_store=$test_directory/valid-store
valid_hash=$("$rovelle" blob put "$valid_store" "$source_file")
valid_manifest=$test_directory/valid.rvl
printf 'ROVELLE-MANIFEST-V1\n%s\tserver/index.js\n' \
    "$valid_hash" >"$valid_manifest"

revision=$("$rovelle" manifest publish "$valid_store" "$valid_manifest")

if [ "${#revision}" -ne 64 ]; then
    echo "manifest publish returned an invalid revision: $revision" >&2
    exit 1
fi

missing_hash=0000000000000000000000000000000000000000000000000000000000000000
missing_manifest=$test_directory/missing.rvl
missing_error=$test_directory/missing.err
printf 'ROVELLE-MANIFEST-V1\n%s\tserver/missing.js\n' \
    "$missing_hash" >"$missing_manifest"

objects_before=$(count_objects "$valid_store")

if "$rovelle" manifest publish "$valid_store" "$missing_manifest" \
    >"$test_directory/missing.out" 2>"$missing_error"; then
    echo "manifest publish accepted a missing blob" >&2
    exit 1
fi

if ! grep 'ROVELLE_NOT_FOUND' "$missing_error" >/dev/null; then
    echo "manifest publish did not report ROVELLE_NOT_FOUND" >&2
    cat "$missing_error" >&2
    exit 1
fi

objects_after=$(count_objects "$valid_store")

if [ "$objects_after" -ne "$objects_before" ]; then
    echo "manifest publish stored a manifest with a missing blob" >&2
    exit 1
fi

corrupt_store=$test_directory/corrupt-store
corrupt_hash=$("$rovelle" blob put "$corrupt_store" "$source_file")
corrupt_manifest=$test_directory/corrupt.rvl
corrupt_error=$test_directory/corrupt.err
printf 'ROVELLE-MANIFEST-V1\n%s\tserver/corrupt.js\n' \
    "$corrupt_hash" >"$corrupt_manifest"

object_path=$corrupt_store/objects/$(printf '%s' "$corrupt_hash" | cut -c 1-2)/$(printf '%s' "$corrupt_hash" | cut -c 3-64)
printf 'corrupted object\n' >"$object_path"

objects_before=$(count_objects "$corrupt_store")

if "$rovelle" manifest publish "$corrupt_store" "$corrupt_manifest" \
    >"$test_directory/corrupt.out" 2>"$corrupt_error"; then
    echo "manifest publish accepted a corrupted blob" >&2
    exit 1
fi

if ! grep 'ROVELLE_CORRUPT' "$corrupt_error" >/dev/null; then
    echo "manifest publish did not report ROVELLE_CORRUPT" >&2
    cat "$corrupt_error" >&2
    exit 1
fi

objects_after=$(count_objects "$corrupt_store")

if [ "$objects_after" -ne "$objects_before" ]; then
    echo "manifest publish stored a manifest with a corrupted blob" >&2
    exit 1
fi
