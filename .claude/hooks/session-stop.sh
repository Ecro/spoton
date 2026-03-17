#!/bin/bash
# Session stop — show pending work summary
INPUT=$(cat)
PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"

HAS_OUTPUT=false

# Active plans
PLAN_FILES=$(find "$PROJECT_DIR/docs/plans" -maxdepth 1 -name "PLAN-*.md" 2>/dev/null | head -5)
if [ -n "$PLAN_FILES" ]; then
    printf 'Session checkpoint:\n'
    printf 'Active plans:\n'
    while IFS= read -r f; do
        printf '  - %s\n' "$(basename "$f")"
    done <<< "$PLAN_FILES"
    HAS_OUTPUT=true
fi

# Uncommitted changes
MODIFIED=$(git -C "$PROJECT_DIR" status --short 2>/dev/null)
if [ -n "$MODIFIED" ]; then
    COUNT=$(echo "$MODIFIED" | wc -l)
    if [ "$HAS_OUTPUT" = false ]; then
        printf 'Session checkpoint:\n'
    fi
    printf 'Uncommitted changes: %d files\n' "$COUNT"
    echo "$MODIFIED" | head -10 | sed 's/^/  /'
    HAS_OUTPUT=true
fi

# Current branch
BRANCH=$(git -C "$PROJECT_DIR" branch --show-current 2>/dev/null)
if [ -n "$BRANCH" ] && [ "$HAS_OUTPUT" = true ]; then
    printf 'Branch: %s\n' "$BRANCH"
fi

exit 0
