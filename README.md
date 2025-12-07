# FIFO Board Indicator — PlatformIO Setup

This guide explains how to set up and build the project using PlatformIO CLI and how to generate a `compile_commands.json` (compiledb) file so your editor's C/C++ language server (clangd / IntelliSense) can find Arduino and library headers (e.g. `Wire.h`, `Adafruit_GFX.h`, etc.).

This repository target platform: `esp32-s3-devkitc-1` with `framework = arduino`.

## Prerequisites

- Python 3.7+ (for PlatformIO CLI)
- Git (optional but recommended)
- A terminal (macOS / Linux / Windows PowerShell / Windows CMD)
- An editor (VS Code, Zed, or other). If you use VS Code with the PlatformIO extension, some intellisense steps below are optional.

## 1) Install PlatformIO CLI

Recommended using pip.

```/dev/null/commands.sh#L1-8
# Install (user-wide)
python3 -m pip install -U platformio

# or (if you prefer)
pip install -U platformio

# Verify installation
pio --version
```

Notes:
- If `pio` is not found after installation, ensure your Python user scripts path is in `PATH`. On macOS/Linux this is typically `~/.local/bin` (or the path returned by `python3 -m site --user-base` + `/bin`).
- You can also use the PlatformIO extension inside VS Code, but installing the CLI is recommended for terminal builds and generating compiledb.

## 2) Ensure `platformio.ini` is configured

Open `platformio.ini` and make sure:
- `framework = arduino` is present.
- Required libraries are declared under `lib_deps` or installed via `pio lib install`.

A minimal example `platformio.ini` snippet (this project already includes one — if you modify it, ensure you add the libraries your code uses):

```/dev/null/platformio.ini#L1-40
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
  adafruit/Adafruit GFX Library
  adafruit/Adafruit SSD1306
  Keypad
```

If you prefer to install libraries manually (instead of relying on `lib_deps`), run:

```/dev/null/commands.sh#L1-6
pio lib install "Adafruit GFX Library"
pio lib install "Adafruit SSD1306"
pio lib install "Keypad"
```

Running `pio lib install` will add libraries to the project `.pio/libdeps` folder (or to the global library storage depending on your settings). Using `lib_deps` in `platformio.ini` lets PlatformIO fetch them automatically during `pio run`.

## 3) Build the project

From the project root (the directory containing `platformio.ini`):

```/dev/null/commands.sh#L1-4
# Build
pio run

# If you want verbose output (helps debugging missing headers)
pio run -v
```

Successful build output ends with a message like "Building . . . SUCCESS" and a `/.pio/build/<env>` folder with compiled objects.

Common build errors and how to fix them:
- `fatal error: Wire.h: No such file or directory`
  - Ensure `framework = arduino` is present in `platformio.ini`.
  - Ensure the proper board is set (so framework/toolchain selected is correct).
  - Make sure the Adafruit/Arduino libraries are installed (see `lib_deps` or run `pio lib install`).
  - Run `pio run -v` to see include paths and confirm where the compiler is looking.

- Missing `Adafruit_GFX.h` or `Keypad.h`:
  - Verify `lib_deps` contains those libraries OR install them manually via `pio lib install`.
  - After installing, run `pio run` again.

If things look stale, try cleaning and rebuilding:

```/dev/null/commands.sh#L1-6
pio run --target clean
rm -rf .pio/build .pio/libdeps .pio/.cache  # careful: removes PlatformIO caches
pio run
```

## 4) (Optional, but recommended) Generate `compile_commands.json` for editor intellisense

Many language servers (clangd, ccls) and editors accept a `compile_commands.json` database that contains the compile flags and include paths for each translation unit. PlatformIO can generate it for you.

```/dev/null/commands.sh#L1-6
# From project root
pio run --target compiledb
# This produces a compile_commands.json (or a json in .pio) in the project root.
```

What this does:
- Produces `compile_commands.json` that tools like clangd use to get the exact include paths and flags used by PlatformIO.
- This resolves editor diagnostics such as "Wire.h not found" and other header resolution issues.

Editor-specific notes:
- VS Code with the official PlatformIO extension: compiledb is typically not required because the extension already configures IntelliSense. Still, generating `compile_commands.json` is harmless and can help external tools.
- Zed or other editors using `clangd`: after generating `compile_commands.json`, restart your editor or the language server so it picks the file up and re-indexes the project.

## 5) Editor integration / clangd troubleshooting (Zed, others)

If your editor still shows `Wire.h` or other "file not found" diagnostics after generating the compiledb:

1. Confirm `compile_commands.json` exists in the project root.
2. Restart the editor / language server (e.g., reload window).
3. Confirm the language server is configured to look for `compile_commands.json` in the project root. clangd normally auto-detects it.
4. If your editor has a caching layer, clear or re-index the project.

## 6) Extra PlatformIO commands that help diagnose issues

```/dev/null/commands.sh#L1-12
# Show project environments and targets
pio project inspect

# Show installed libs
pio lib list

# Force update of platform and libraries
pio update

# Remove and reinstall libs
pio lib uninstall <LIBNAME>
pio lib install <LIBNAME>
```

## 7) If the problem is code-level (not environment)

- The compiler will show errors and line numbers. Fix the code errors shown by the compiler — example common fixes:
  - Forward-declare functions if they are used before their definitions.
  - Convert `String` to `const char*` for `display.println()` if your language server complains: `display.println(line.c_str());`
  - Make sure constants/macros (e.g., `NUM_LEDS`) are correctly defined and in scope.

## 8) Example quick checklist to get to a successful `pio run`

1. Ensure Python is installed and `pio` works:
   ```/dev/null/commands.sh#L1-4
   python3 -m pip install -U platformio
   pio --version
   ```
2. Confirm `platformio.ini` includes `framework = arduino` and (optionally) `lib_deps`.
3. Install libraries if needed:
   ```/dev/null/commands.sh#L1-3
   pio lib install "Adafruit GFX Library"
   pio lib install "Adafruit SSD1306"
   pio lib install "Keypad"
   ```
4. Build:
   ```/dev/null/commands.sh#L1-3
   pio run
   ```
5. If editor diagnostics show missing headers, generate compiledb:
   ```/dev/null/commands.sh#L1-2
   pio run --target compiledb
   # Restart editor / language server
   ```

---

## Wokwi Simulation (wokwi/diagram.json)

This project contains a Wokwi diagram file at `wokwi/diagram.json` plus a detailed guide at `wokwi/README.md`. Important: the `diagram.json` in this repository describes the simulated hardware wiring and components only — it does NOT include the firmware/sketch by default.

How to use the diagram and run code:
1. Open https://wokwi.com in your browser and open the Wokwi editor.
2. Import the diagram (drag-and-drop `wokwi/diagram.json`, use the editor's Import menu, or paste the JSON).
3. After the diagram loads, you must add the firmware/sketch in Wokwi's Code or Files panel (paste your `.ino` / `.cpp` source or create a new sketch file). See `wokwi/README.md` for step-by-step instructions.
4. Click Run to compile and start the simulation. Open the Serial Monitor to view serial output and use the on-screen controls to interact with simulated inputs (keypad, switches, etc.).

Notes:
- Because the repository `diagram.json` does not contain the sketch, you need to paste or create the sketch inside Wokwi before running the simulation.
- The Wokwi simulation is useful for quick prototyping and visualization but may differ from real hardware. Always verify behavior on physical hardware.
- If Wokwi reports missing libraries or unsupported components, consult `wokwi/README.md` for troubleshooting and possible substitutions.

If you get a specific `pio run` error message, paste the exact output here (or at least the last ~40 lines) and I will help you resolve it step-by-step.