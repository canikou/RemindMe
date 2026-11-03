# Contributing

## Development Workflow

1. Create a feature branch from `main`.
2. Run `cargo fmt` and `cargo clippy --all-targets -- -D warnings`.
3. Run `cargo test`.
4. Open a pull request with a clear summary and scope.

## Pull Request Checklist

- [ ] `cargo build` passes locally.
- [ ] Tests pass locally.
- [ ] `cargo fmt --check` and `cargo clippy` pass locally.
- [ ] Changes are scoped and documented.
- [ ] New behavior is covered by tests where practical.

## Commit Guidance

- Use small, focused commits.
- Prefer imperative commit messages, for example: `Add tray close behavior`.
