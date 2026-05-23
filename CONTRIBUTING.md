# Contributing to LockCracker

Thanks for taking a look! LockCracker is a small hobby/kids-toy firmware
project for the M5Stack Core 2. Contributions are welcome — bug reports,
fixes, and small features.

## Quick start

1. Install [PlatformIO Core](https://platformio.org/install/cli).
2. Clone the repo:
   ```
   git clone https://github.com/marceld23/LockCracker.git
   cd LockCracker
   ```
3. Build and flash to a connected M5Stack Core 2:
   ```
   pio run -e m5stack-core2 -t upload
   pio device monitor
   ```

## Reporting bugs

Open an issue using the **Bug report** template. Please include:

- Your hardware (Core 2 revision if known)
- PlatformIO / M5Unified version
- Steps to reproduce, expected vs. actual behavior
- Serial monitor output if relevant

## Suggesting features

Open an issue using the **Feature request** template. Keep in mind the
project is a toy aimed at kids — features should keep the UI playful
and the interaction simple.

## Submitting a pull request

1. Fork the repo and create a topic branch from `main`.
2. Keep changes focused — one logical change per PR.
3. Match the existing code style in [src/main.cpp](src/main.cpp):
   - `static` functions over classes
   - `constexpr` constants at the top of the file
   - Draw into the off-screen `LGFX_Sprite canvas`, never to
     `M5.Display` directly
4. Test on real hardware if your change affects drawing, audio, IMU, or
   power handling.
5. Update [README.md](README.md) and/or [AGENTS.md](AGENTS.md) if you
   change user-facing behavior or development conventions.
6. Open the PR against `main` and fill in the template.

## Code of Conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md).
By participating, you agree to abide by its terms.

## License

By contributing, you agree that your contributions will be licensed
under the [MIT License](LICENSE).
