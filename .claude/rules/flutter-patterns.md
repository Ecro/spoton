---
globs:
  - "spoton-app/**/*.dart"
  - "spoton-app/pubspec.yaml"
  - "spoton-app/pubspec.lock"
---

# Flutter / Dart Coding Patterns

## State Management
- Riverpod with `AsyncNotifier` for async state
- `StateNotifier` for synchronous state
- Use `ref.watch()` in widgets, `ref.read()` in callbacks
- Provider scope at app root

## Database
- sqflite with versioned schema
- Migration via `onUpgrade` with `ALTER TABLE ADD COLUMN`
- Repository pattern: Provider → Repository → sqflite
- Never access DB directly from widgets

## BLE
- `flutter_blue_plus` for BLE communication
- Repository abstraction layer over raw BLE
- Handle connection state changes gracefully
- Implement reconnection logic
- Parse binary payloads with `ByteData` / `Uint8List`

## Architecture
```
Widget (UI) → Provider (State) → Repository (Logic) → Data Source (DB/BLE)
```

## Naming
- Files: `snake_case.dart`
- Classes: `PascalCase`
- Variables/functions: `camelCase`
- Constants: `camelCase` or `UPPER_SNAKE_CASE` for config

## Error Handling
- Use `AsyncValue` for loading/error/data states
- Show user-friendly error messages
- Log errors with context for debugging
- Never swallow exceptions silently
