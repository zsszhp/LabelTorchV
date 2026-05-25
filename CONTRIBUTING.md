# Contributing to LabelTorch (标炬)

Thank you for your interest in contributing to LabelTorch!

## Development Setup

1. **Prerequisites**: Qt 6.11+, MSVC 2022, CMake 3.22+, Python 3.11+
2. **Clone**: `git clone <repo-url>`
3. **Build**: `cmake --preset msvc2022-release && cmake --build --preset msvc2022-release`
4. **Test C++**: `ctest --preset msvc2022-release`
5. **Test Python**: `cd backend && python -m pytest tests/`
6. **Lint Python**: `cd backend && python -m ruff check .`

## Code Style

- **C++**: Follow Qt coding conventions, use Q_INVOKABLE for QML-exposed methods
- **QML**: Use Theme.qml design tokens (colors, fonts, spacing), no hardcoded values
- **Python**: PEP 8, async/await pattern, type hints where practical
- **Commits**: Use `<type>: <中文描述>` format (feat/fix/refactor/docs/test/perf/style/chore/ci)

## Architecture

- Feature-based module organization under `src/features/`
- Each module: Service (.h/.cpp) + Model (.h/.cpp) + QML pages
- Database access via `Database::instance().database()` singleton
- IPC via JSON-RPC over stdin/stdout to Python backend
- Training adapters follow the `TrainingAdapter` base class pattern
- All IDs use UUID (`Id::generate()`), Service dependencies injected via `setXxx()`
- Logging via `ltInfo(LT_LOG_XXX())`, never `qDebug()`

## Git Workflow

- Single developer, only `main` branch, no feature branches
- Each commit must push to both GitHub and Gitee
- Commit format: `<type>: <中文描述>`
- Version tags: `v{major}.{minor}.{patch}`

## Reporting Issues

Please include:
- Qt/Python/OS version
- Steps to reproduce
- Expected vs actual behavior
- Relevant log output
