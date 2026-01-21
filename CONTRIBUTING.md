# Contributing to umsg

Thank you for your interest in contributing to `umsg`!

## Philosophy

`umsg` is designed to be:
1. **Simple**: Easy to understand and integrate.
2. **Robust**: Resilient to errors and misuse.
3. **Embedded-first**: No dynamic allocation, minimal dependencies.

Please keep these principles in mind when proposing changes.

## Development Workflow

1. Fork the repository.
2. Create a feature branch.
3. Make your changes.
4. Run the tests locally (`./tests/run.sh`).
5. Submit a Pull Request.

## Testing

All changes must pass the existing test suite. New features should include new tests in `tests/`.

To run tests:
```bash
./tests/run.sh
```

## Coding Style

- **C++ Standard**: C++11.
- **Formatting**: We generally follow a style similar to Google C++ Style Guide or LLVM, but the most important rule is consistency with existing code.
- **Headers**: Maintain the header-only structure.
- **Naming**: `camelCase` for variables/methods, `PascalCase` for types.

## License

By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
