#!/bin/bash
# Post-write verification reminder hook for SpotOn
# Outputs a system message after firmware/app files are written

# Read stdin JSON (required even if not used)
INPUT=$(cat)

# Output JSON system message matching Claude Code hook format
cat << 'EOF'
{
  "continue": true,
  "systemMessage": "File written. Consider running `west build -b nrf52840dk/nrf52840` (firmware) or `flutter analyze` (app) to verify changes."
}
EOF

exit 0
