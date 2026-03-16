# /status - Session Status

Display current session state, pending work, and workflow position.

## Model Configuration

```
model: haiku
thinking: default
```

## Usage

```
/status
/status $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Optional: "git", "todos", "workflow", "files", or blank for full status

## Implementation

### Full Status (Default)

#### 1. Git Status
```bash
# Check if git repo exists first
git rev-parse --is-inside-work-tree 2>/dev/null || echo "⚠ Not a git repo. Run: git init"

git status --short
git log -1 --oneline
git diff --stat HEAD | tail -1
```

#### 2. Build Status
```bash
west build -b nrf54l15dk/nrf54l15 2>&1 | tail -5
```

#### 3. Workflow Position
Determine current stage based on:
- Recent commands used
- Git working directory state
- Presence of PLAN-*.md files

### Status Report Format

```markdown
## Session Status

### Workflow Position
```
1. Research → 2. Plan → [3. EXECUTE] → 4. Review → 5. Wrapup
                             ↑
                        YOU ARE HERE
```

**Current Stage:** {stage}
**Active Plan:** {PLAN file or "None"}

### Git Status
| Metric | Value |
|--------|-------|
| Branch | `{branch}` |
| Last Commit | `{hash} {message}` |
| Modified Files | {N} |

### Build Status
| Check | Result |
|-------|--------|
| west build | PASS / FAIL |

### Recommended Next Action
- {recommendation based on state}
```

## Quick Modes

| Command | Shows |
|---------|-------|
| `/status git` | Git only |
| `/status todos` | Pending tasks only |
| `/status workflow` | Workflow position only |
| `/status files` | Recently modified files |
