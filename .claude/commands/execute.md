# /execute - Implement with Verification

Execute implementation based on a plan, with continuous verification and scope enforcement. This is **Step 3** of the 5-stage workflow.

## Model Configuration

```
model: opus
thinking: ultrathink
```

**IMPORTANT:** Use extended thinking mode for careful implementation. Verify scope, thread safety, and resource budgets at each step.

## Usage

```
/execute $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Feature or task to implement (must have a plan from `/myplan`)

## Workflow Position

```
1. Research → 2. Plan → [3. EXECUTE] → 4. Review → 5. Wrapup
                             ↑
                        YOU ARE HERE
```

## Implementation

### Phase 0: Scope Validation (CRITICAL)

**Load and validate the plan:**

```
Read docs/plans/PLAN-{feature}.md
```

**Extract scope boundaries:**

```markdown
## Scope Confirmation

### From Plan:
- **Single Concern:** {what this change does}
- **Files In Scope:** {list from plan}
- **NON-GOALS:** {what NOT to touch}
- **Thread Boundaries:** {which threads/queues affected}
```

**STOP if no plan exists.** Run `/myplan` first.

### Phase 1: Pre-Implementation Checklist

- [ ] Plan exists and is reviewed
- [ ] Scope boundaries are clear
- [ ] Current build passes: `west build -b nrf54l15dk/nrf54l15`
- [ ] Working directory is clean or changes are intentional
- [ ] Required hardware is connected (if flashing)

### Phase 2: Read Before Write (CRITICAL)

```
Read {every file to be modified}  # ALWAYS read first
```

**Never modify code you haven't read.** Understand existing patterns before changing.

### Phase 3: Implementation Loop

For each task in the plan:

#### 3a. Scope Check (Before Each Edit)

```
□ Is this file in the plan's scope?
□ Does this change align with the Single Concern?
□ Is this file NOT in the NON-GOALS list?
```

**If any answer is NO → STOP, note for separate work.**

#### 3b. Write Test First (If Applicable)

For firmware logic that can be unit-tested:
```bash
# Zephyr unit test
west build -b native_posix -t run -- -DTEST_SUITE={suite}
```

For Flutter:
```bash
cd spoton-app && flutter test test/{test_file}.dart
```

**Skip test-first for:** device tree changes, Kconfig changes, pure hardware init, LED patterns.

#### 3c. Thread Boundary Check (If Applicable)

**Before modifying message queue producers/consumers:**
```
⚠ THREAD BOUNDARY: Verify queue format matches on both sides
- Producer: {thread} → k_msgq_put({queue}, ...)
- Consumer: {thread} → k_msgq_get({queue}, ...)
- Struct size: {N} bytes — must match K_MSGQ_DEFINE slot size
```

**Before modifying ISR handlers:**
```
⚠ ISR CONTEXT: No blocking calls allowed
- Only: k_msgq_put, k_sem_give, atomic ops
- Never: k_sleep, k_msgq_get with timeout, LOG_*
```

#### 3d. Implement

- Follow code patterns from CLAUDE.md and `.claude/rules/c-zephyr-patterns.md`
- Match existing style and conventions
- Stay within declared scope
- Include error handling (check every return code)

#### 3e. Build Verification

```bash
# After each significant change
west build -b nrf54l15dk/nrf54l15
```

**Fix any build errors before proceeding to next task.**

#### 3f. Flash & Test (When Hardware Available)

```bash
west flash
minicom -D /dev/ttyACM0 -b 115200  # Monitor serial output
```

### Phase 4: Full Build Verification

```bash
# Clean build
west build -b nrf54l15dk/nrf54l15 --pristine

# Check resource usage
west build -t rom_report  # Flash usage
west build -t ram_report  # RAM usage
```

**Verify resource budgets from plan are met.**

### Phase 5: Exit Criteria & Scope Check

```markdown
## Exit Criteria Verification

From docs/plans/PLAN-{feature}.md:
- [ ] {criterion 1}
- [ ] {criterion 2}
- [ ] Build passes without errors
- [ ] Resource budget within limits

## Scope Compliance
- [ ] All changes within scope
- [ ] No NON-GOAL items touched
- [ ] Thread boundaries respected
```

### Phase 6: Implementation Summary

```markdown
## Execution Complete

### Summary
**Task:** $ARGUMENTS
**Status:** Complete | Partial | Blocked
**Scope Compliance:** Yes | No

### Changes Made
| File | Type | Description | In Scope? |
|------|------|-------------|-----------|
| `{path}` | {added/modified} | {description} | Yes |

### Thread Boundary Changes (if any)
| Queue/Semaphore | Producer | Consumer | Change |
|-----------------|----------|----------|--------|
| {name} | {thread} | {thread} | {what changed} |

### Build Status
- [x] `west build` — PASS
- [x] Resource budget — Flash: {N}KB, RAM: {N}KB

### Next Steps
- `/review` — code review before committing
- `/wrapup` — commit if confident
```

## Scope Violation Recovery

If you accidentally made an out-of-scope change:
1. Identify the violation
2. Revert if possible: `git checkout -- {file}`
3. Note in summary for separate work

## Error Recovery

If build fails:
1. Read error message carefully
2. Check recent changes with `git diff`
3. Fix incrementally, verify after each fix
4. If stuck, ask user for help
