# /research - Research & Best Practices

Research a feature, problem, or technology before planning. This is **Step 1** of the 5-stage workflow.

## Model Configuration

```
model: opus
thinking: ultrathink
```

**IMPORTANT:** Use extended thinking mode for thorough research. Gather comprehensive information before making recommendations.

## Usage

```
/research $ARGUMENTS
```

## Arguments

`$ARGUMENTS` - Topic, feature, problem, or technology to research

## Workflow Position

```
[1. RESEARCH] → 2. Plan → 3. Execute → 4. Review → 5. Wrapup
     ↑
   YOU ARE HERE
```

## Implementation

### Phase 1: Context Gathering

**1. Web Search for Best Practices:**

```
WebSearch("$ARGUMENTS best practices 2025 2026")
WebSearch("$ARGUMENTS implementation patterns")
```

**2. Explore Codebase for Related Patterns:**

```
Task(
  subagent_type="Explore",
  description="Find related code for $ARGUMENTS",
  prompt="Search for existing patterns, implementations, and dependencies related to: $ARGUMENTS",
  model="opus"
)
```

**3. Check Official Documentation (Context7):**

For Zephyr/nRF/sensor topics:
```
mcp__context7__resolve-library-id(libraryName: "zephyr-project")
mcp__context7__query-docs(libraryId: "{id}", query: "$ARGUMENTS")
```

For Flutter/Dart topics:
```
mcp__context7__resolve-library-id(libraryName: "flutter")
mcp__context7__query-docs(libraryId: "{id}", query: "$ARGUMENTS")
```

**4. Check TECH_SPEC.md for Project Context:**

```
Read TECH_SPEC.md  # Always check project spec for constraints
```

### Phase 2: Analysis

Analyze findings with ultrathink depth:

1. **Problem Understanding:**
   - What exactly needs to be solved?
   - What are the constraints (MCU resources, power, size)?
   - What are the success criteria from TECH_SPEC.md?

2. **Approach Identification:**
   - Identify 2-3 possible approaches
   - List pros/cons for each
   - Evaluate complexity, risk, and resource impact

3. **Compatibility Check:**
   - Does this fit within nRF54L15 resource budget (1,524KB RRAM, 256KB RAM)?
   - Power impact on battery life?
   - Compatible with Zephyr / nRF Connect SDK?

### Phase 3: Research Summary

Output a structured research summary:

```markdown
## Research Summary: $ARGUMENTS

### Problem Understanding
**What:** {concise problem description}
**Constraints:** {MCU resources, power, size, Zephyr compatibility}
**Success Criteria:** {from TECH_SPEC.md}

### Best Practices Found

| Source | Key Recommendation | Relevance |
|--------|-------------------|-----------|
| {source 1} | {recommendation} | High/Medium/Low |

### Recommended Approaches

#### Option A: {name} (Recommended)
**Description:** {approach details}
**Pros:** {list}
**Cons:** {list}
**Resource Impact:** Flash: ~{N}KB, RAM: ~{N}KB, Power: {impact}
**Complexity:** Low/Medium/High
**Risk:** Low/Medium/High

#### Option B: {name}
...

### Existing Code Patterns
| File | Pattern | Relevance |
|------|---------|-----------|
| `{path}` | {pattern description} | {how it relates} |

### Sources
- [{title}]({url})

---

## Recommendation

**Proceed with:** Option {A/B}
**Rationale:** {1-2 sentence explanation}

**Next Step:**
/myplan {feature based on recommended option}
```

### Phase 4: Validation

**STOP and validate with user:**

```markdown
## Research Complete

**Topic:** $ARGUMENTS
**Approaches Found:** {N}
**Recommended:** {Option name}

**Questions for you:**
1. Does this research address your needs?
2. Should I explore any approach in more detail?
3. Ready to proceed with `/myplan`?
```

## Integration with MCP Servers

| MCP Server | When to Use |
|------------|-------------|
| Context7 | Zephyr APIs, nRF SDK, Flutter, sensor datasheets |
| Sequential | Complex multi-step analysis (architecture, debugging) |
| WebSearch | Current best practices, Zephyr examples |

## Next Steps

After research approval:
```
/myplan {feature based on research findings}
```
