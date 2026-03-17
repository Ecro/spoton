#!/bin/bash
# Smart build reminder — detects file type, uses jq for safe JSON
INPUT=$(cat)

# Fail-open if jq not available
if ! command -v jq &>/dev/null; then
    exit 0
fi

FILE=$(echo "$INPUT" | jq -r '.tool_input.file_path // .tool_input.file // empty' 2>/dev/null) || exit 0

if echo "$FILE" | grep -qE '\.(c|h)$|CMakeLists|\.conf$|\.overlay$|Kconfig'; then
    MSG="Firmware file written. Run: west build -b nrf54l15dk/nrf54l15"
elif echo "$FILE" | grep -qE '\.dart$|pubspec\.yaml$'; then
    MSG="Flutter file written. Run: cd spoton-app && flutter analyze"
else
    MSG="File written."
fi

jq -n --arg msg "$MSG" '{"continue": true, "systemMessage": $msg}'
exit 0
