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

test_directory=$(mktemp -d "${TMPDIR:-/tmp}/rovelle-cli-help.XXXXXX")
trap 'rm -rf "$test_directory"' EXIT HUP INT TERM

root_help=$test_directory/root-help.txt
if ! "$rovelle" --help >"$root_help" 2>&1; then
    echo "help command failed: rovelle --help" >&2
    cat "$root_help" >&2
    exit 1
fi

if ! grep -i 'usage:' "$root_help" >/dev/null; then
    echo "help output has no Usage section: rovelle --help" >&2
    cat "$root_help" >&2
    exit 1
fi

commands=$test_directory/commands.txt
awk '
    /^Usage:/ { in_usage = 1; next }
    in_usage && /^$/ { exit }
    in_usage {
        sub(/^[[:space:]]*rovelle[[:space:]]+/, "")
        sub(/[[:space:]]+<.*/, "")
        print
    }
' "$root_help" >"$commands"

if [ ! -s "$commands" ]; then
    echo "root help advertises no commands" >&2
    exit 1
fi

expected_commands=$test_directory/expected-commands.txt
cat >"$expected_commands" <<'EOF'
help
version
hash
blob put
blob get
manifest publish
EOF

if ! cmp -s "$expected_commands" "$commands"; then
    echo "root help command inventory changed" >&2
    echo "Expected:" >&2
    cat "$expected_commands" >&2
    echo "Actual:" >&2
    cat "$commands" >&2
    exit 1
fi

while IFS= read -r command; do
    output=$test_directory/help-output.txt
    set -- $command

    if [ "$command" = "help" ]; then
        if ! "$rovelle" "$@" >"$output" 2>&1; then
            echo "help command failed: rovelle $command" >&2
            cat "$output" >&2
            exit 1
        fi
    elif ! "$rovelle" "$@" --help >"$output" 2>&1; then
        echo "help command failed: rovelle $command --help" >&2
        cat "$output" >&2
        exit 1
    fi

    if ! grep -i 'usage:' "$output" >/dev/null; then
        echo "help output has no Usage section: rovelle $command" >&2
        cat "$output" >&2
        exit 1
    fi
done <"$commands"
