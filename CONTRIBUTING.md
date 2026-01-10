# Contributing to Goku IR Device

Thank you for your interest in contributing to the project! We welcome all contributions, from bug fixes to new features and documentation improvements.

## 🛠 Development Setup

1.  **Install ESP-IDF**: Ensure you have ESP-IDF v5.x installed.
2.  **Linting**: We use `clang-format` for code style. Please format your code before submitting.

## 📁 Project Structure

The project is modularized into components:
-   **Core**: `components/goku_core`
-   **Peripherals**: `components/goku_peripherals`
-   **Network**: `components/goku_wifi`
-   **IR Engine**: `components/goku_ir`

Please place new features in the appropriate component or create a new one if necessary.

## 🧩 Adding New AC Protocols

To add a new AC brand:
1.  Analyze typical IR signals (use `ir_debugging_guide.md`).
2.  Implement the encoder in `components/goku_ir/src/protocols/ir_<brand>.hpp`.
3.  Register the new protocol in `components/goku_ir/include/ir_ac_registry.hpp`.
4.  See `porting_new_brands.md` for a detailed guide.

## 📝 Pull Request Guidelines

1.  Describe your changes clearly.
2.  Ensure the project builds (`idf.py build`).
3.  Update documentation if you change behavior or add features.

Happy coding!
