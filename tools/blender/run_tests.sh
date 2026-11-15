#!/bin/sh
# Test the Mario Golf Character add-on in Blender, without a window.
#
# The add-on is built and installed as an extension into a throwaway Blender
# user folder, so the tests use what an artist installs and leave the
# user's own Blender settings alone.
#
# Usage: tools/blender/run_tests.sh [BLENDER]

set -eu

blender=${1:-${BLENDER:-blender}}
here=$(cd "$(dirname "$0")" && pwd)
repository=$(cd "$here/../.." && pwd)
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

# The tests make Charlie's template from the repository's files
"$repository/.venv/bin/python3" "$repository/tools/character_template.py" \
    --repository "$repository" charlie \
    "$repository/build/character_templates/charlie.json" >/dev/null

"$blender" --factory-startup --command extension build \
    --source-dir "$here/mario_golf_character" --output-dir "$scratch" >/dev/null
export BLENDER_USER_RESOURCES="$scratch/user"
"$blender" --factory-startup --command extension install-file \
    -r user_default "$scratch"/mario_golf_character-*.zip >/dev/null

"$blender" -b --factory-startup --python-exit-code 1 \
    --python "$here/tests/test_mario_golf_character.py" -- "$repository" "$scratch/work"
