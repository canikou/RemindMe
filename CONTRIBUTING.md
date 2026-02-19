# Contributing

## Development Workflow

1. Create a feature branch from `main`.
2. Run `./scripts/check-conventions.ps1`.
3. Run `cmake --preset debug`.
4. Run `cmake --build --preset debug --parallel`.
5. Run `ctest --preset debug`.
6. Open a pull request with a clear summary and scope.

## Pull Request Checklist

- [ ] Build passes locally (`debug` or `release`).
- [ ] Tests pass locally.
- [ ] Convention check passes locally.
- [ ] Changes are scoped and documented.
- [ ] New behavior is covered by tests where practical.

## Commit Guidance

- Use small, focused commits.
- Prefer imperative commit messages, for example: `Add tray close behavior`.
