# /quick - Quick Fix

Fast implementation for simple, well-defined tasks. Bypasses the full 5-stage workflow.

## Model Configuration

```
model: sonnet
thinking: default
```

## Usage

```
/quick $ARGUMENTS
```

## When to Use

| Use /quick | Use Full Workflow |
|------------|-------------------|
| Single-file changes | Multi-file coordination |
| Kconfig toggle | New driver implementation |
| Fix typo in comment | Architecture changes |
| Update a constant | New thread or queue |
| Fix build warning | BLE protocol changes |
| Simple DT pin change | ML feature changes |

## Implementation

### Step 1: Understand

Read the task. Identify target file(s) and exact change.

### Step 2: Read Before Write

```
Read {target-file}
```

**Never modify code you haven't read.**

### Step 3: Make the Change

Implement the fix directly.

### Step 4: Verify Build

```bash
west build -b nrf54l15dk/nrf54l15
```

### Step 5: Report

```markdown
## Quick Fix Complete

**Task:** $ARGUMENTS
**File:** `{path}`
**Change:** {1-line description}
**Build:** PASS

### Next Steps
- `/review --quick {file}` — if you want a quick review
- `/wrapup` — if ready to commit
```

## Escalation

If the task is more complex than expected:

```markdown
## Escalation

This task is more complex than expected.
**Reason:** {why}
**Recommendation:** Run `/myplan {task}` instead
```
