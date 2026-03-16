# /myplan - Create Implementation Plan

Analyze, design, and create implementation plan for a task. This is **Step 2** of the 5-stage workflow.

## Model Configuration

```
model: opus
thinking: ultrathink
```

**IMPORTANT:** Use extended thinking mode for thorough planning. Consider resource budgets, thread safety, and hardware constraints.

## Usage

```
/myplan $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Feature or task to plan (e.g., "ICM-42688-P SPI driver", "BLE GATT service", "Flutter hit-map view")

## Workflow Position

```
1. Research → [2. PLAN] → 3. Execute → 4. Review → 5. Wrapup
                  ↑
             YOU ARE HERE
```

## Implementation

### Phase 1: Context Loading

**Load project context:**

```
Read TECH_SPEC.md          # Full technical specification
Read CLAUDE.md             # Project conventions
```

**If firmware task, also check:**
```
Read prj.conf              # Current Kconfig
Read boards/*.overlay      # Device tree
Read CMakeLists.txt        # Build config
```

**If Flutter task, also check:**
```
Read spoton-app/pubspec.yaml
Read spoton-app/lib/       # Explore existing structure
```

### Phase 2: Problem Analysis

- **What** needs to be done?
- **Why** is this needed? (trace to TECH_SPEC.md success criteria)
- **Constraints:** MCU resources, power budget, Zephyr API availability
- **Dependencies:** what must exist first?

### Phase 3: Technical Design

- **Architecture:** component changes, data flow
- **Design Decisions:** why this approach, alternatives considered
- **Resource Budget:** Flash/RAM impact, power impact
- **Risk Assessment:** what could go wrong, mitigation

### Phase 4: Implementation Plan

Break into phases with specific file changes:

```markdown
### Phase 1: Core Implementation
- [ ] Task 1 — `{file}`: {changes}
- [ ] Task 2 — `{file}`: {changes}

### Phase 2: Testing & Verification
- [ ] Build test: `west build`
- [ ] Flash test: `west flash` + serial monitor
- [ ] Hardware test: {specific test procedure}

### Phase 3: Integration
- [ ] Integrate with {component}
- [ ] End-to-end verification
```

### Phase 5: Multi-Agent Plan Validation (2/3 Consensus)

After plan is created, run 3 independent validators in parallel:

```
Task(
  subagent_type="general-purpose",
  description="Plan Validator 1 of 3",
  prompt="{plan content}\n\nCritique this implementation plan. Check for: missing error handling, resource budget violations (256KB RAM, 1,524KB RRAM), Zephyr API misuse, thread safety issues, power budget impact, missing test coverage. Return findings as: category, severity (critical/warning/suggestion), description, recommendation."
)
// Same for Validator 2 and 3 — all in single message for parallel execution
```

**Apply 2/3 Consensus Filter:**
1. Group findings by category + description similarity
2. Keep findings flagged by 2+ validators
3. Drop single-validator-only findings (noise)
4. For kept findings: use majority severity

**Handle results:**
- All clear or suggestions only → APPROVED, proceed
- Warnings → auto-add to review checklist, proceed
- Critical findings → ask user: revise / proceed anyway / cancel

### Phase 6: Generate PLAN Document

Save to: `docs/plans/PLAN-{feature-slug}.md`

```markdown
# PLAN: {Feature Name}

**Task:** {description}
**Phase:** {0/1/2/3 from TECH_SPEC.md}
**Created:** {YYYY-MM-DD}

---

## Executive Summary

> **TL;DR:** {one sentence}

### Key Decisions
- **Decision 1:** {description} — because {reason}

### Resource Impact
- **Flash:** ~{N} KB
- **RAM:** ~{N} KB
- **Power:** {impact description}

---

## Review Checklist

- [ ] Resource budget OK? (RRAM: {N}/1,524KB, RAM: {N}/256KB)
- [ ] Power budget OK? (ACTIVE: {N}mA target TBD — pending nRF54L15 verification)
- [ ] Thread safety verified?
- [ ] Zephyr API usage correct?
- [ ] Hardware pin conflicts checked?

---

## Problem Analysis

### What
{description}

### Why
{trace to TECH_SPEC.md success criteria}

### Success Criteria
- [ ] {criterion from TECH_SPEC.md}

---

## Technical Design

### Architecture
{design overview, data flow}

### Design Decisions
1. **{decision}** — rationale: {why}

---

## Implementation Plan

### Phase 1: {name}
- [ ] {file}: {change}

### Phase 2: Testing
- [ ] Build: `west build -b nrf54l15dk/nrf54l15`
- [ ] Flash: `west flash`
- [ ] Hardware test: {procedure}

---

## Testing Strategy

### Build Tests
- `west build` passes

### Hardware Tests
- {test with DK on bench}
- {test with sensor attached to racket}

### Manual Verification
1. {step}
2. {step}

---

## Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| {risk} | {impact} | {mitigation} |

---

## Plan Validation (3-Agent Consensus)

**Method:** 2/3 consensus
**Overall:** APPROVED | NEEDS_REVISION | MAJOR_REVISION
{validation results}
```

### Phase 7: Output Summary

```markdown
## Plan Created

**Task:** {feature}
**Document:** docs/plans/PLAN-{slug}.md
**Validation:** {APPROVED/NEEDS_REVISION/MAJOR_REVISION}

## Next Steps

1. Review the plan document
2. Check the Review Checklist
3. When ready: `/execute {feature}`
```

## Important Notes

- Always check TECH_SPEC.md for resource budgets and constraints
- Firmware plans must consider thread safety and ISR context
- Flutter plans must consider BLE packet format compatibility
- Plan validation catches gaps before any code is written
- Hardware-dependent tasks should include bench test procedures
