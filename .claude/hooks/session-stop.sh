#!/bin/bash
# Session stop hook for SpotOn
# Shows pending work when Claude Code finishes responding

# Read stdin JSON (required even if not used)
INPUT=$(cat)

# Get project directory
PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"

# Check for active PLAN files
PLAN_FILES=$(find "$PROJECT_DIR/docs/plans" -maxdepth 1 -name "PLAN-*.md" 2>/dev/null | head -5)
if [ -n "$PLAN_FILES" ]; then
    echo "Session checkpoint:"
    echo "  Active plans:"
    echo "$PLAN_FILES" | while read -r f; do
        basename "$f" | sed 's/^/    - /'
    done
fi

exit 0
