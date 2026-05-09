# Contributing to LabelTorch (标炬)

Thank you for your interest in contributing to LabelTorch!

## Development Setup

1. **Prerequisites**: Qt 6.11+, MSVC 2022, CMake 3.21+, Python 3.11+
2. **Clone**: `git clone <repo-url>`
3. **Build**: `cmake --preset msvc2022-debug && cmake --build --preset msvc2022-debug`
4. **Test**: `cd out/build/msvc2022-debug && ctest --output-on-failure`

## Code Style

- **C++**: Follow Qt coding conventions, use Q_INVOKABLE for QML-exposed methods
- **QML**: Catppuccin Mocha dark theme, consistent component structure
- **Python**: PEP 8, type hints where practical
- **Commits**: Use `[PhaseX.Y]` prefix, describe changes in Chinese

## Architecture

- Feature-based module organization under `src/features/`
- Each module: Service (.h/.cpp) + Model (.h/.cpp) + QML pages
- Database access via `Database::instance().database()` singleton
- IPC via JSON-RPC over stdin/stdout to Python backend
- Training adapters follow the `TrainingAdapter` base class pattern

## Pull Request Process

1. Create a feature branch from `main`
2. Make changes with clear commit messages
3. Ensure all tests pass (`ctest --output-on-failure`)
4. Submit PR with description of changes

## Reporting Issues

Please include:
- Qt/Python/OS version
- Steps to reproduce
- Expected vs actual behavior
- Relevant log output
