#!/usr/bin/env bash
set -euo pipefail

# stdin: JSON with tool_input containing .file_path for Write/Edit tools,
# or .command for Bash tools.
input=$(cat)

file_path=$(echo "$input" | jq -r '.tool_input.file_path // ""')
command=$(echo "$input" | jq -r '.tool_input.command // ""')

message="Blocked: do not modify the KMC toolchain (tools/gcc_kmc), tools/kmc-cc, or the vendored decomp-permuter/asm-differ submodules to change how a file compiles. If an object needs different compiler behavior, add a target-specific profile override in the Makefile's COMPILER PROFILES section instead."

is_blocked() {
  case "$1" in
    tools/gcc_kmc/*|*/tools/gcc_kmc/*|tools/kmc-cc|*/tools/kmc-cc|tools/decomp-permuter/*|*/tools/decomp-permuter/*|tools/asm-differ/*|*/tools/asm-differ/*)
      return 0
      ;;
  esac
  return 1
}

if is_blocked "$file_path"; then
  echo "$message" 1>&2
  exit 2
fi

if [[ -n "$command" ]] && echo "$command" | grep -Eq '(>|>>|tee |sed -i|perl -[0-9A-Za-z]*i|apply_patch|rm |mv |cp |chmod |git checkout|git restore).*(tools/gcc_kmc|tools/kmc-cc|tools/decomp-permuter|tools/asm-differ)'; then
  echo "$message" 1>&2
  exit 2
fi

exit 0
