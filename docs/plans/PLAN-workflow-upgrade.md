# PLAN: Claude Code Workflow Upgrade — Hook Quality Gates & Improvements

**Task:** Upgrade Claude Code workflow with hook-based quality gates and improvements
**Phase:** Pre-Phase 0 (tooling/infrastructure)
**Created:** 2026-03-17

---

## Executive Summary

> **TL;DR:** Transform passive hook reminders into active quality enforcement, add path-specific rules, fix discovered bugs, and update global CLAUDE.md to match SpotOn's actual agents.

### Key Decisions
- **Active hooks over passive reminders** — hooks should enforce (exit 2 to block) not just remind
- **Keep commands format for now** — skills migration deferred (known `context: fork` issue #17283)
- **No agent memory upgrade** — `memory: project` is not a supported agent frontmatter field (3/3 validator consensus); defer to future
- **Fix bugs first** — wrong board name in hook, stale neuroTerm references in global CLAUDE.md
- **Removed SessionStart hook** — `SessionStart` is not a documented Claude Code hook event (2/3 validator consensus)

### Impact
- **Files changed:** ~12 config files
- **Risk:** Low (all changes are configuration, no firmware/app code)
- **Reversibility:** Full (git revert)

---

## Bugs Found During Research

| # | File | Bug | Severity |
|---|------|-----|----------|
| 1 | `.claude/hooks/post-write-reminder.sh` | References `nrf52840dk/nrf52840` instead of `nrf54l15dk/nrf54l15` | High — misleading build command |
| 2 | `~/.claude/CLAUDE.md` Agent Selection Guide | Lists neuroTerm agents (rust-tauri, react-typescript, ui-reviewer, ux-reviewer, test-runner, ui-test) that don't exist | Medium — confusing context |
| 3 | `~/.claude/CLAUDE.md` Model Selection | Shows `/quick` uses Opus, but actual command uses Sonnet | Low — inaccurate docs |
| 4 | `~/.claude/CLAUDE.md` Session Recovery | References `PLAN_*.md` (underscore) but actual format is `PLAN-*.md` (hyphen) | Low |

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus
**Overall:** APPROVED with revisions applied

### Critical Fixes Applied (from validation)
1. **Removed `SessionStart` hook** (2/3) — not a documented Claude Code event
2. **Fixed `pre-tool-guard.sh` output** (3/3) — block messages must go to stdout, not stderr
3. **Added `jq` guard** (3/3) — all hooks fail-open if `jq` is missing
4. **JSON via `jq`** (3/3) — proper escaping in `post-write-reminder.sh`
5. **Expanded git regex** (3/3) — now catches `--all` in addition to `-A`
6. **Removed Phase 4** (3/3) — `memory: project` not a supported frontmatter field
7. **Fixed glob patterns** (3/3) — `*.overlay` → `**/*.overlay`, removed `models/**` from c-zephyr
8. **Replaced `echo -e`** (2/3) — using `printf` in `session-stop.sh`
9. **Clarified settings merge** (3/3) — merge into existing, don't replace
10. **Phase 3 marked independent** (3/3) — no dependency on Phase 2

### Frontmatter Key Verification Needed
- `paths:` vs `globs:` — validators noted uncertainty. Must verify against Claude Code docs before execution. If the key is `globs:`, update all rules files accordingly.

---

## Implementation Plan

### Phase 1: Bug Fixes & Quick Wins (5 files)

- [x] **1.1** — `.claude/hooks/post-write-reminder.sh`: Fix board name `nrf52840dk/nrf52840` → `nrf54l15dk/nrf54l15`
- [x] **1.2** — `.claude/rules/c-zephyr-patterns.md`: Add `globs:` frontmatter for firmware files only
- [x] **1.3** — `.claude/rules/flutter-patterns.md`: Add `globs:` frontmatter for Flutter files only
- [x] **1.4** — `.claude/rules/git-workflow.md`: No change needed (universal rule, applies to all files)
- [x] **1.5** — `~/.claude/CLAUDE.md`: Fix Agent Selection Guide, Model Selection, Session Recovery to match SpotOn

### Phase 2: Active Hook Quality Gates (3 files)

- [x] **2.1** — `.claude/hooks/post-write-reminder.sh` → **Upgraded to smart build reminder**: Detects file type, uses `jq` for JSON construction
- [x] **2.2** — `.claude/hooks/pre-tool-guard.sh` (NEW): PreToolUse guard — blocks writes to `models/*.h`, blocks `git add -A/--all/.`
- [x] **2.3** — `.claude/settings.json`: Added PreToolUse hook entry (merged into existing)

### Phase 3: Session Stop Upgrade (1 file) — INDEPENDENT

- [x] **3.1** — `.claude/hooks/session-stop.sh`: Added uncommitted changes count, modified file list, current branch info. Uses `printf`.

### Phase 4: Environment & Settings (2 files) — INDEPENDENT

- [x] **4.1** — `.claude/settings.local.json`: Added `env` block with `CLAUDE_AUTOCOMPACT_PCT_OVERRIDE=85`
- [x] **4.2** — `~/.claude/CLAUDE.md`: Updated Environment Variables section to reflect 85 default

---

## Technical Design

### Hook Architecture (After Upgrade)

```
PreToolUse ───→ pre-tool-guard.sh ──→ Block: models/*.h writes, git add -A/--all/.
     │
PostToolUse ──→ post-write-reminder.sh ──→ Smart reminder: west build / flutter analyze
     │
Stop ─────────→ session-stop.sh ──→ Show: active plans + uncommitted changes + branch
```

### Hook: pre-tool-guard.sh (NEW)

```bash
#!/bin/bash
# PreToolUse guard — block dangerous operations
set -euo pipefail

# Fail-open if jq not available
if ! command -v jq &>/dev/null; then
    exit 0
fi

INPUT=$(cat)
TOOL=$(echo "$INPUT" | jq -r '.tool_name // empty' 2>/dev/null) || exit 0

# Guard 1: Block writes to ML model headers (generated files)
if [ "$TOOL" = "Write" ] || [ "$TOOL" = "Edit" ]; then
    FILE=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty' 2>/dev/null) || exit 0
    if echo "$FILE" | grep -qE '(^|/)models/[^/]*\.h$'; then
        echo "BLOCKED: models/*.h files are generated by emlearn training scripts. Do not edit manually."
        exit 2
    fi
fi

# Guard 2: Block git add -A / --all / .
if [ "$TOOL" = "Bash" ]; then
    CMD=$(echo "$INPUT" | jq -r '.tool_input.command // empty' 2>/dev/null) || exit 0
    if echo "$CMD" | grep -qE 'git\s+add\s+(-A|--all|\.)(\s|$)'; then
        echo "BLOCKED: Never use 'git add -A', 'git add --all', or 'git add .'. Stage specific files only."
        exit 2
    fi
fi

exit 0
```

### Hook: post-write-reminder.sh (Upgraded)

```bash
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
```

### Hook: session-stop.sh (Upgraded)

```bash
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
```

### Path-Scoping Rules Frontmatter

**NOTE:** Verify whether the correct key is `paths:` or `globs:` before execution.

```yaml
# c-zephyr-patterns.md
---
globs:
  - "src/**/*.c"
  - "src/**/*.h"
  - "boards/**"
  - "**/*.overlay"
  - "prj.conf"
  - "Kconfig"
  - "CMakeLists.txt"
---

# flutter-patterns.md
---
globs:
  - "spoton-app/**/*.dart"
  - "spoton-app/pubspec.yaml"
  - "spoton-app/pubspec.lock"
---

# git-workflow.md — no frontmatter (universal rule, applies to all files)
```

### Settings Changes

**`.claude/settings.json`** (MERGE into existing, preserve current structure):

Add `PreToolUse` block alongside existing `PostToolUse` and `Stop`:

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Write|Edit|Bash",
        "hooks": [
          {
            "type": "command",
            "command": ".claude/hooks/pre-tool-guard.sh",
            "timeout": 5
          }
        ]
      }
    ],
    "PostToolUse": [
      {
        "matcher": "Write|Edit",
        "hooks": [
          {
            "type": "command",
            "command": ".claude/hooks/post-write-reminder.sh",
            "timeout": 5
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": ".claude/hooks/session-stop.sh",
            "timeout": 10
          }
        ]
      }
    ]
  }
}
```

**`.claude/settings.local.json`** (UPDATE existing, preserve permissions):

```json
{
  "permissions": {
    "allow": [
      "Bash(west build*)",
      "Bash(west flash*)",
      "Bash(minicom*)",
      "Bash(cd spoton-app && flutter*)"
    ]
  },
  "env": {
    "CLAUDE_AUTOCOMPACT_PCT_OVERRIDE": "85"
  }
}
```

### Global CLAUDE.md Fixes

**Agent Selection Guide** — replace neuroTerm agents with SpotOn agents:

```markdown
## Agent Selection Guide

| Situation | Agent | Invocation |
|-----------|-------|------------|
| Zephyr firmware work | zephyr-firmware | When working in src/, boards/, *.overlay |
| Flutter app work | flutter-app | When working in spoton-app/ |
| Code review (general) | code-reviewer | Via /review command (3x consensus) |
| Performance review | performance-reviewer | Via /review command (parallel) |
| Concurrency review | concurrency-reviewer | Via /review (when threads/ISR changed) |
| Walkthrough review | walkthrough-reviewer | Via /review (when logic changed) |
| Side-effect review | side-effect-reviewer | Via /review (when APIs/structs changed) |
| Hardware review | hardware-reviewer | Via /review (when DT/pins/SPI changed) |
| BLE contract review | ble-contract-reviewer | Via /review (when BLE changed) |
| Test quality review | test-quality-reviewer | Via /review (when behavior changed) |
```

**Model Selection** — fix Sonnet/Haiku:

```markdown
## Model Selection

| Task Complexity | Model | Commands |
|-----------------|-------|----------|
| Complex (multi-file, architectural) | Opus + Ultrathink | /research, /myplan, /execute, /review, /wrapup |
| Simple (single-file, trivial) | Sonnet | /quick |
| Status/Info | Haiku | /status |
```

**Session Recovery** — fix plan file pattern:

```markdown
## Session Recovery

If you lose context or return after a break:

```
/status           # See where you are
# Read any active PLAN-*.md file
# Continue from last in-progress todo
```
```

---

## NON-GOALS

- **Do NOT migrate commands → skills format** — known issue with `context: fork` (#17283), commands work fine
- **Do NOT enable Agent Teams** — experimental, not needed for current project phase
- **Do NOT add worktree isolation to agents** — premature for pre-Phase 0
- **Do NOT add prompt-type or agent-type hooks** — start with command hooks, upgrade later if needed
- **Do NOT modify firmware or app source code** — this plan is tooling-only
- **Do NOT add `memory: project` to agents** — not a supported frontmatter field (3/3 validator consensus)
- **Do NOT add `SessionStart` hook** — not a documented Claude Code hook event (2/3 validator consensus)

---

## Testing Strategy

### Pre-Execution Verification

```bash
# 1. Verify jq is installed
command -v jq && echo "OK" || echo "INSTALL JQ FIRST: sudo apt install jq"

# 2. Verify correct frontmatter key (paths: vs globs:)
# Check Claude Code docs or test empirically before Phase 1.2-1.3
```

### Hook Testing Commands

```bash
# Test pre-tool-guard.sh (should BLOCK — exit 2, message on stdout)
echo '{"tool_name":"Write","tool_input":{"file_path":"models/svr_model.h"}}' | bash .claude/hooks/pre-tool-guard.sh
echo "Exit code: $?"

# Test pre-tool-guard.sh (should ALLOW — exit 0)
echo '{"tool_name":"Write","tool_input":{"file_path":"src/sensor.c"}}' | bash .claude/hooks/pre-tool-guard.sh
echo "Exit code: $?"

# Test pre-tool-guard.sh (should BLOCK git add --all)
echo '{"tool_name":"Bash","tool_input":{"command":"git add --all"}}' | bash .claude/hooks/pre-tool-guard.sh
echo "Exit code: $?"

# Test post-write-reminder.sh (firmware — should mention west build)
echo '{"tool_input":{"file_path":"src/sensor.c"}}' | bash .claude/hooks/post-write-reminder.sh

# Test post-write-reminder.sh (flutter — should mention flutter analyze)
echo '{"tool_input":{"file_path":"spoton-app/lib/main.dart"}}' | bash .claude/hooks/post-write-reminder.sh

# Test session-stop.sh
echo '{}' | bash .claude/hooks/session-stop.sh
```

### Path-Scoping Verification

After Phase 1.2-1.3, test by starting a new Claude session:
1. Edit a `.c` file → verify c-zephyr-patterns loaded
2. Edit a `.dart` file → verify flutter-patterns loaded (not c-zephyr)
3. Edit CLAUDE.md → verify only git-workflow loaded

---

## Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| PreToolUse hook blocks legitimate writes | Medium | Guard only `models/*.h` and `git add -A/--all/.` — very narrow scope |
| Hook timeout blocks workflow | Low | All hooks are fast shell scripts (<100ms), 5s timeout |
| `jq` not installed | Medium | Fail-open guard at top of every hook (`exit 0` if missing) |
| Path-scoping frontmatter key wrong (`paths:` vs `globs:`) | Medium | Verify before execution; easy to fix if wrong |
| `jq` parse error on unexpected input | Low | `|| exit 0` after every jq call (fail-open) |

---

## Execution Order

```
Phase 1 (bug fixes)     ← Do first, immediate value
  ↓
Phase 2 (active hooks)  ← Highest ROI, core improvement
  │
  ├── Phase 3 (stop upgrade)   ← INDEPENDENT, can parallel
  └── Phase 4 (env settings)   ← INDEPENDENT, can parallel
```

All phases after Phase 1 are independent and can run in parallel.

---

## Future Improvements (Not In Scope)

1. **Skills migration** — when `context: fork` issue is fixed, migrate commands to `.claude/skills/` for dynamic context injection
2. **Agent Teams** — for Phase 0+ parallel sensor driver implementation
3. **Prompt-type hooks** — semantic safety checks (e.g., "Is this Bash command safe for embedded dev?")
4. **Agent-type Stop hook** — deep validation (read all modified files, verify they compile)
5. **CI integration** — headless Claude Code for automated PR review
6. **`/batch` skill** — fan-out parallel implementation across worktrees
7. **Agent memory** — when/if `memory:` frontmatter becomes supported, add to implementation agents
8. **SessionStart hook** — when/if Claude Code adds this event, inject build status and active plan context
