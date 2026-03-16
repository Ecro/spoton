# /review - Code Review (Step 4)

Comprehensive code review before committing. This is **Step 4** of the 5-stage workflow.

## Model Configuration

```
model: opus
thinking: ultrathink
```

**IMPORTANT:** Use extended thinking mode for thorough analysis. Consider all safety, correctness, performance, and architecture implications.

## Usage

```
/review $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Optional scope: file path, "recent" for recent changes, or blank for all uncommitted changes

## Workflow Position

```
1. Research → 2. Plan → 3. Execute → [4. REVIEW] → 5. Wrapup
                                          ↑
                                     YOU ARE HERE
```

## Implementation

### Phase 1: Determine Scope

```bash
git diff HEAD --name-only
git status --short
```

### Phase 2: Load Standards

```
Read TECH_SPEC.md       # Resource budgets, data structures, protocol spec
Read CLAUDE.md          # Code conventions
```

**If a plan exists:**
```
Read docs/plans/PLAN-{feature}.md  # Scope boundaries
```

### Phase 3: Parallel Specialized Reviews (2/3 Consensus)

#### Phase 3a: Determine Reviewers

| Change Type | Reviewers | Expected Count |
|-------------|-----------|:-:|
| Full review (default) | 3x code + perf + walk + side + concurrency + hardware + ble + test-quality | 8 |
| Firmware only (no BLE) | 3x code + perf + walk + concurrency + hardware + test-quality | 6 |
| Flutter only | 3x code + perf + walk + side + test-quality | 5 |
| No thread/ISR changes | Same minus concurrency-reviewer | -1 |
| No hardware/DT changes | Same minus hardware-reviewer | -1 |
| No BLE changes | Same minus ble-contract-reviewer | -1 |
| Config/DT only | 1x code-reviewer (no consensus needed) | 0 |

Set `REVIEW_EXPECTED_COUNT` to the number of **specialized reviewers** (NOT counting the 3 code-reviewer instances).

**Fallback rules:**
- No thread/ISR/queue changes → skip concurrency-reviewer
- No DT/pin/SPI/power changes → skip hardware-reviewer
- No BLE GATT/packet changes → skip ble-contract-reviewer
- No exports/APIs/structs changed → skip side-effect-reviewer
- Config-only, docs-only → skip test-quality-reviewer
- No branch logic changed → skip walkthrough-reviewer

#### Phase 3b: Batch 1 — Code Review Consensus + Performance

Spawn **4 agents in parallel**:

| # | Subagent | Prompt |
|---|----------|--------|
| 1 | `general-purpose` | Code reviewer #1 (see template below) |
| 2 | `general-purpose` | Code reviewer #2 (identical prompt) |
| 3 | `general-purpose` | Code reviewer #3 (identical prompt) |
| 4 | `performance-reviewer` | Performance review for: {scope} |

**Code-reviewer prompt template (identical for all 3):**

```
You are a principal embedded systems engineer conducting a code review. Think like a hardware engineer for safety, a reliability engineer for robustness, and an end-user for correctness.

Context:
- Platform: Zephyr RTOS on nRF54L15 (128MHz Cortex-M33, 1,524KB RRAM, 256KB RAM)
- Sensors: ICM-42688-P + ADXL372 via SPI at 800Hz
- Project type: Tennis sensor with ML inference + BLE sync
- Review scope: {scope}
- Standards: TECH_SPEC.md, CLAUDE.md

Review in strict priority order. P0+P1 = 60%+ of effort:

P0 — Safety (blocks release): Stack overflow, ISR misuse, flash corruption, watchdog starvation, buffer overflow, uninitialized memory, integer overflow in sensor math
P1 — Correctness (production bugs): Wrong SPI register addresses, incorrect FIFO parsing, msgq size mismatch, state machine deadlock, BLE packet format errors, ML feature calculation bugs
P2 — Architecture: Thread responsibility, data flow clarity, Zephyr API usage, TECH_SPEC.md compliance, resource leaks
P3 — Maintainability: Long functions, deep nesting, magic numbers, dead code

For each finding use EXACT format:
#### [P0|P1|P2|P3] {title}
- **File:** `path:line`
- **Reasoning:**
  1. [Observe] {concrete code fact — quote verbatim}
  2. [Trace] {follow data/control flow}
  3. [Infer] {why this causes a problem}
  4. [Conclude] {specific failure scenario}
- **Risk:** {what breaks}
- **Current:** {code}
- **Fix:** {corrected code}

After findings:
### Strengths (3-5 genuine positives)
### Scorecard (Safety/Correctness/Architecture/Maintainability — X/10 each)
### Verdict: APPROVED | CHANGES_REQUESTED | NEEDS_WORK

Rules:
1. "No issues found" is valid. Never fabricate issues.
2. Every finding MUST include corrected code.
3. Only review code in the diff.
4. Ignore style/formatting — other tools handle that.
5. P0 and P1 are non-negotiable.

Now review these files: {changed files list}
```

#### Phase 3c: Batch 2 — All Specialist Reviewers

After Batch 1 completes, spawn **all applicable specialists in parallel** (single batch for speed):

| # | Subagent | When | Skip If |
|---|----------|------|---------|
| 5 | `walkthrough-reviewer` | Always for logic changes | Config/docs only |
| 6 | `side-effect-reviewer` | When exports/APIs/structs change | Internal-only changes |
| 7 | `concurrency-reviewer` | When threads/ISRs/queues change | No async/shared state |
| 8 | `hardware-reviewer` | When DT/pins/SPI/power change | Pure logic changes |
| 9 | `test-quality-reviewer` | When behavior changes (new/modified functions) | Config/docs only |
| 10 | `ble-contract-reviewer` | When BLE GATT/packets change | No BLE changes |

**IMPORTANT:** All specialists are independent — spawn them all in a **single message** for maximum parallelism. Do NOT wait for one specialist before starting another.

### Phase 3.5: Consensus Filter & Consolidation

#### 3.5a: Reasoning Tree Consensus (Code Review)

Parse findings from all 3 code-reviewer instances:

**Step 1:** Group by file + line (±10 lines)

**Step 2:** Reasoning similarity scoring (per pair):

| Component | Weight | Score 1.0 | Score 0.0 |
|-----------|:------:|-----------|-----------|
| Root cause (Observe+Trace) | 40% | Same code + same flow | Different code |
| Causal chain (Trace+Infer) | 35% | Same chain + same rule | Different chain |
| Conclusion (Infer+Conclude) | 25% | Same scenario | Different scenario |

**Step 3:** Apply thresholds:

| Score | Tag | Auto-fix? |
|:-----:|-----|:---------:|
| ≥ 0.75 | strong | Yes |
| ≥ 0.50 | standard | Yes |
| < 0.50 | weak | Manual review only |

**Step 4:** Solo findings (1/3 only) — verify reasoning chain:
- [Observe] quotes real code? [Trace] real flow? [Conclude] concrete scenario?
- If verified → `solo-verified` (manual review only)
- If not → DROP

**Step 5:** Determine verdict:
- Any P0/P1 strong/standard → CHANGES_REQUESTED
- Only P2/P3 or weak → APPROVED with suggestions
- Nothing → APPROVED

#### 3.5b: Cross-Dimension Corroboration

Compare code-reviewer findings with specialist findings. Same file:line + same issue = **Corroborated** (highest confidence).

Deduplicate, merge into unified list sorted by priority.

### Phase 4: Generate Report

```markdown
## Code Review Report

### Scope
Files analyzed: {list}

### Scope Compliance
| Check | Status |
|-------|--------|
| Single Concern | PASS/FAIL |
| Files In Scope | PASS/FAIL |
| NON-GOALS Respected | PASS/FAIL |
| Thread Boundaries Correct | PASS/N/A |

### Summary
| Metric | Value |
|--------|-------|
| Grade | A-F |
| Critical Issues | {N} |
| Warnings | {N} |
| Consensus | {N strong+standard} / {N total} |

### Critical Issues (Must Fix)
#### [CRITICAL-1] {title}
**File:** `{path}:{line}`
**Confidence:** {tag}
**Problem:** {description}
**Current:** {code}
**Fix:** {corrected code}

### Warnings (Should Fix)
...

### Weak Findings (Manual Review Required)
...

### Positive Observations
- {good practice}

### Resource Check
| Resource | Used | Budget | Status |
|----------|------|--------|--------|
| Flash | {N} KB | {budget} KB | OK/WARN |
| RAM | {N} KB | {budget} KB | OK/WARN |

## Verdict
**Status:** APPROVED | CHANGES_REQUESTED | NEEDS_WORK
**Next:** `/wrapup` (if approved) or fix issues and re-review
```

## Quick Review Mode

```
/review --quick src/sensor.c
```

Single code-reviewer + performance-reviewer on that file only. For quick iteration during development.

## Next Steps

After review approval:
```
/wrapup {commit description}
```
