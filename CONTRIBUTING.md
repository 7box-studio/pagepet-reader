# Contributing to PagePet Reader

Thank you for helping improve PagePet Reader. The project currently targets the **Xteink X3** and is designed around physical buttons, a small memory budget, and a quiet optional PagePet companion.

## Before you start

1. Read the [README](README.md) and the [development priorities](docs/ROADMAP.md).
2. Clone the repository with its submodules:

   ```sh
   git clone --recurse-submodules git@github.com:7box-studio/pagepet-reader.git
   cd pagepet-reader
   ```

3. Build the X3 environment:

   ```sh
   pio run -e x3
   ```

The firmware uses [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) and [PagePet](https://github.com/7box-studio/pagepet) as pinned submodules. Keep changes to those dependencies in their own repositories unless a submodule revision needs to be updated here.

## Making a change

- Keep the primary interface in English; add Russian text to the matching translation path when user-facing text changes.
- Use the existing button-only navigation model. Every new flow needs a complete path for selection, confirmation, Back, and return to reading where relevant.
- Treat the ESP32-C3 memory budget as a hard constraint. Explain new allocations and avoid repeated allocations in render or input loops.
- Keep user-facing strings in the project's translation system. Do not add hardcoded UI text.
- Use the HAL and existing UI helpers instead of calling low-level hardware or storage APIs directly.
- Update the relevant documentation when behaviour, controls, storage formats, or supported hardware changes.

## Testing checklist

Before opening a pull request:

- Run `pio run -e x3`.
- Run the relevant host checks or scripts for the changed area.
- If hardware is available, test the full button path on an Xteink X3, including recovery and return to reading.
- Report hardware testing separately from build testing. Do not claim device support without a device check.
- For reading or storage changes, follow the [X3 benchmark checklist](docs/X3-BENCHMARK.md) when applicable.

## Pull requests

Use a short imperative title and explain the user problem, the chosen behaviour, and how it was verified. Include screenshots or serial logs when they make an e-ink or recovery issue easier to understand. Keep unrelated refactors out of feature changes.

Please do not commit credentials, private logs, build output, or generated files that are listed in `.gitignore`.
