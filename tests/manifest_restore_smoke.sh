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

test_directory=$(mktemp -d "${TMPDIR:-/tmp}/rovelle-restore.XXXXXX")
trap 'rm -rf "$test_directory"' EXIT HUP INT TERM

store=$test_directory/store
source=$test_directory/source
expected=$test_directory/expected
restored=$test_directory/restored
manifest=$test_directory/release.rvl


mkdir -p "$source"
printf 'console.log("Rovelle restore test");\n' >"$source/app.js"
printf 'body { color: purple; }\n' >"$source/style.css"


app_hash=$("$rovelle" blob put "$store" "$source/app.js")
css_hash=$("$rovelle" blob put "$store" "$source/style.css")

printf 'ROVELLE-MANIFEST-V1\n%s\tserver/index.js\n%s\tpublic/style.css\n' \
    "$app_hash" "$css_hash" >"$manifest"

revision=$("$rovelle" manifest publish "$store" "$manifest")

mv "$source" "$expected"

"$rovelle" manifest restore "$store" "$revision" "$restored"

cmp "$expected/app.js" "$restored/server/index.js"
cmp "$expected/style.css" "$restored/public/style.css"

unsafe_manifest=$test_directory/unsafe.rvl
unsafe_destination=$test_directory/unsafe-restored
unsafe_error=$test_directory/unsafe.err
escaped_file=$test_directory/escaped.txt

printf 'ROVELLE-MANIFEST-V1\n%s\t../escaped.txt\n' \
    "$app_hash" >"$unsafe_manifest"

unsafe_revision=$("$rovelle" blob put "$store" "$unsafe_manifest")

if "$rovelle" manifest restore \
    "$store" "$unsafe_revision" "$unsafe_destination" \
    >"$test_directory/unsafe.out" 2>"$unsafe_error"; then
    echo "restore accepted an unsafe logical path" >&2
    exit 1
fi

if ! grep -F 'ROVELLE_INVALID_ARGUMENT' "$unsafe_error" >/dev/null; then
    echo "restore did not reject the unsafe logical path" >&2
    cat "$unsafe_error" >&2
    exit 1
fi

if [ -e "$escaped_file" ]; then
    echo "restore wrote outside the destination directory" >&2
    exit 1
fi

conflict_error=$test_directory/conflict.err

if "$rovelle" manifest restore "$store" "$revision" "$restored" \
    >"$test_directory/conflict.out" 2>"$conflict_error"; then
    echo "restore accepted an existing destination" >&2
    exit 1
fi

if ! grep -F 'ROVELLE_CONFLICT' "$conflict_error" >/dev/null; then
    echo "restore did not report ROVELLE_CONFLICT" >&2
    cat "$conflict_error" >&2
    exit 1
fi

cmp "$expected/app.js" "$restored/server/index.js"
cmp "$expected/style.css" "$restored/public/style.css"
