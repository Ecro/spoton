# Git Workflow

## Branch Naming
- `feat/{description}` — new feature
- `fix/{description}` — bug fix
- `refactor/{description}` — code restructuring
- `docs/{description}` — documentation only
- `hw/{description}` — hardware/PCB related

## Commit Format (Conventional Commits)
```
{type}({scope}): {description}

{bullet points}

Co-Authored-By: Claude <noreply@anthropic.com>
```

### Types
feat, fix, refactor, docs, style, test, chore, perf, hw

### Scopes
sensor, ml, ble, session, power, calibration, app, dt, build

## Rules
- Never `git add -A` or `git add .`
- Stage only files changed in current session
- Never `git restore .` or `git reset --hard` without asking
- Run `west build` before committing firmware changes
- Run `flutter build` before committing app changes
