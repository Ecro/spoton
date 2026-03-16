# /wrapup - Finalize, Document, Commit

Finalize work, update documentation, and commit. This is **Step 5** of the 5-stage workflow.

## Model Configuration

```
model: opus
thinking: ultrathink
```

## CRITICAL: SESSION-ONLY SCOPE

**NEVER commit, stage, restore, or modify files outside your session's changes.**

**ABSOLUTELY FORBIDDEN:**
- `git add -A` or `git add .`
- `git restore .` or `git checkout .`
- `git reset --hard`
- `git clean -f`
- `git stash` without explicit user permission

## Usage

```
/wrapup $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Brief description for commit message (optional)

## Workflow Position

```
1. Research → 2. Plan → 3. Execute → 4. Review → [5. WRAPUP]
                                                      ↑
                                                 YOU ARE HERE
```

## Implementation

### Phase 0: Git Repository Check

```bash
git rev-parse --is-inside-work-tree 2>/dev/null || echo "NOT_A_GIT_REPO"
```

**If not a git repo:**
```
⚠ Not a git repository. Run `git init` first, then:
  git add {files}
  git commit -m "initial commit"
```

**STOP and tell user to initialize git if needed.**

### Phase 1: Pre-Flight Verification

**Build must pass:**

```bash
# Firmware
west build -b nrf54l15dk/nrf54l15

# Flutter (if app changes)
cd spoton-app && flutter build apk
```

**If build fails, STOP and fix first.**

### Phase 1.5: Code Cleanup

```bash
# Check for debug code
Grep "printk\|LOG_DBG" in src/  # Review — keep only necessary debug logs
Grep "TODO\|FIXME\|XXX\|HACK" in src/
```

**Remove before commit:**
- Temporary `printk()` statements
- Commented-out code blocks
- Hardcoded test values
- `#if 0` ... `#endif` blocks

**Keep:**
- `LOG_ERR()` for actual errors
- `LOG_WRN()` for unexpected but handled cases
- `LOG_INF()` for important state transitions

### Phase 2: Review Changes

```bash
git status
git diff --stat
git diff HEAD
```

**Verify:**
- [ ] All changes are intentional
- [ ] No debug code left in
- [ ] No hardcoded test values
- [ ] Changes match the planned scope

### Phase 3: Document Updates

**Update TECH_SPEC.md if:**
- New data structure added
- Pin assignment changed
- Resource budget significantly affected
- BLE protocol changed
- State machine modified

**Update CLAUDE.md if:**
- New file added to source tree
- New development command needed
- New convention established

### Phase 4: Stage Changes

**Only stage files YOU changed in this session:**

```bash
git status  # Review what's changed

# Stage specific files only
git add src/sensor.c src/sensor.h
git add boards/nrf54l15dk_nrf54l15.overlay
git add docs/plans/PLAN-{feature}.md
```

**DO NOT use `git add -A` or `git add .`**

### Phase 5: Create Commit

```bash
git commit -m "$(cat <<'EOF'
{type}({scope}): {description}

- {change 1}
- {change 2}
- {change 3}

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

**Commit Types:** feat, fix, refactor, docs, test, chore, perf, hw
**Scopes:** sensor, ml, ble, session, power, calibration, app, dt, build

### Phase 6: Push (Ask First)

```bash
git push
```

**Always ask before pushing.**

### Phase 7: Completion Summary

```markdown
## Wrapup Complete

### Commit
| Field | Value |
|-------|-------|
| Hash | `{hash}` |
| Message | {first line} |

### Statistics
- Files changed: {N}
- Insertions: +{N}
- Deletions: -{N}

### Documents Updated
| Document | Changes |
|----------|---------|
| TECH_SPEC.md | {changes or "No changes"} |
| CLAUDE.md | {changes or "No changes"} |

### Next Steps
- Continue with next task: `/myplan {next feature}`
- Or start new work: `/research {topic}`
```

## Quality Gates

**Do NOT commit if:**
- `west build` fails
- Debug/test code still present
- Changes don't match planned scope
- Staging files you didn't modify

**OK to commit if:**
- Build passes
- Changes match scope
- `/review` returned APPROVED
