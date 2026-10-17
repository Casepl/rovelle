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

test_directory=$(mktemp -d "${TMPDIR:-/tmp}/rovelle-cas.XXXXXX")
trap 'rm -rf "$test_directory"' EXIT HUP INT TERM

store=$test_directory/store
source_file=$test_directory/source.txt
restored_file=$test_directory/restored.txt
printf 'Rovelle CAS smoke test\nsecond line\n' >"$source_file"

hash=$("$rovelle" blob put "$store" "$source_file")

if [ "${#hash}" -ne 64 ]; then
    echo "blob put returned an invalid SHA-256: $hash" >&2
    exit 1
fi

"$rovelle" blob get "$store" "$hash" "$restored_file"
if ! cmp -s "$source_file" "$restored_file"; then
    echo "blob get did not restore the original content" >&2
    exit 1
fi

duplicate_hash=$("$rovelle" blob put "$store" "$source_file")
if [ "$duplicate_hash" != "$hash" ]; then
    echo "repeated blob put returned a different hash" >&2
    exit 1
fi

expect_invalid_hash_rejected()
{
    label=$1
    invalid_hash=$2
    destination=$test_directory/invalid-$label.txt
    error_output=$test_directory/invalid-$label.err

    if "$rovelle" blob get "$store" "$invalid_hash" "$destination" \
        >"$test_directory/invalid-$label.out" 2>"$error_output"; then
        echo "blob get accepted $label hash: $invalid_hash" >&2
        exit 1
    fi

    if [ -e "$destination" ]; then
        echo "blob get published a destination for $label hash" >&2
        exit 1
    fi
}

expect_invalid_hash_rejected short "${hash%????????}"
expect_invalid_hash_rejected uppercase \
    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
expect_invalid_hash_rejected nonhex \
    gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg

object_path=$store/objects/$(printf '%s' "$hash" | cut -c 1-2)/$(printf '%s' "$hash" | cut -c 3-64)
printf 'corrupted object\n' >"$object_path"

sentinel_file=$test_directory/sentinel.txt
sentinel_copy=$test_directory/sentinel.expected
printf 'do not replace this destination\n' >"$sentinel_file"
cp "$sentinel_file" "$sentinel_copy"
corrupt_error=$test_directory/corrupt.err

if "$rovelle" blob get "$store" "$hash" "$sentinel_file" \
    >"$test_directory/corrupt.out" 2>"$corrupt_error"; then
    echo "blob get accepted a corrupted stored object" >&2
    exit 1
fi

if ! grep 'ROVELLE_CORRUPT' "$corrupt_error" >/dev/null; then
    echo "blob get did not report ROVELLE_CORRUPT" >&2
    cat "$corrupt_error" >&2
    exit 1
fi

if ! cmp -s "$sentinel_copy" "$sentinel_file"; then
    echo "blob get changed the destination after corruption" >&2
    exit 1
fi

if find "$test_directory" -name '*.tmp-*' -print | grep . >/dev/null; then
    echo "blob get left temporary destination files behind" >&2
    find "$test_directory" -name '*.tmp-*' -print >&2
    exit 1
fi
