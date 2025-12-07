# Wokwi Simulation: `diagram.json`

This folder contains a Wokwi project description file: `diagram.json`.  
`diagram.json` is the Wokwi diagram / project file that describes the virtual breadboard, components, wiring, and the MCU used in the simulation. Use it to open and inspect the circuit in the Wokwi web simulator.

This README explains:
- what `diagram.json` is,
- how to open it in Wokwi,
- how to run the simulation and inspect common outputs (serial, pins),
- troubleshooting tips.

---

## What is `diagram.json`?

- `diagram.json` is a Wokwi project file (JSON format). It encodes:
  - the microcontroller/avatar (e.g., an ESP32, AVR, or other supported board),
  - attached parts (shift registers, OLED, keypad, LEDs, resistors, etc.),
  - wiring between parts,
  - optional references to source files.
- When loaded into the Wokwi editor, it reconstructs the schematic/diagram so you can simulate behavior — but note the important caveat below.

---

## Important: this diagram does NOT include firmware/program

- The `diagram.json` in this repository describes only the circuit and wiring. It does NOT contain the compiled firmware nor an embedded sketch by default.
- To run code in Wokwi you must add or paste the Arduino sketch/source code in the Wokwi editor after loading the diagram (instructions below).

---

## How to open `diagram.json` on Wokwi

There are several ways to open the diagram in Wokwi. Choose whichever is most convenient:

Option A — Drag & Drop (quick)
1. Open your browser and go to: https://wokwi.com
2. Click the “Open Editor” / “Start simulation” area (or the Projects link).
3. Drag `diagram.json` from your file system into the Wokwi editor window.
4. Wokwi will parse the JSON and load the project.

Option B — Import / Upload
1. Go to https://wokwi.com and open the online editor.
2. In the editor, find the menu (often top-right) with “Import project” / “Open” / “Upload”.
3. Choose to upload a file and select `diagram.json`.
4. The project will load.

Option C — Paste JSON (if file > copy)
1. Open the Wokwi editor.
2. If there is an “Import JSON” or “Open from JSON” option, open it and paste the contents of `diagram.json`.
3. Load the project.

Option D — Host the JSON and open by URL (advanced)
- If you host the `diagram.json` at a raw URL (GitHub gist/raw file, raw file serving), Wokwi may allow importing by URL. Use the editor's import-from-URL option if available.

---

## Adding code / Running the simulation

1. After loading the diagram, open the **Code** or **Files** panel in the Wokwi editor.
2. Create a new Arduino sketch file (for example `main.cpp`) or paste your existing sketch code into the editor.
3. Ensure your sketch includes `Serial.begin(...)` if you expect serial output, and verify that pin numbers/constants in the code match the wiring in the diagram.
4. Click the Run / Play button to compile and start the simulation. Use the Serial Monitor panel to view serial output.

Notes on importing repository code:
- To test the code from this repository, copy the relevant source files into Wokwi's editor or export a zip of the repository and import it if Wokwi supports zips for your project.
- Alternatively, paste the essential parts of the sketch (setup/loop and pin definitions) into the Wokwi code editor.

---

## Running the simulation (behavior & interaction)

1. Press the Run / Play button in the Wokwi editor.
2. To view serial output (Serial Monitor), open the console/serial panel (usually bottom-right or via a "Serial" tab).
3. Interact with simulated inputs (e.g., press virtual keypad buttons, toggle switches) with the mouse.
4. The display (OLED) and other components will update in real time per the simulated firmware behavior.

---

## Tips & Troubleshooting

- If the project fails to load:
  - Ensure the JSON is valid (no truncated content).
  - Try opening the file with a text editor and verify it begins with `{` and contains Wokwi components.
- If components don't behave as expected:
  - Confirm that the simulated MCU type matches your intended board (ESP32 vs. AVR).
  - Check wiring/pin numbers — simulated pins must match code.
- Serial output not visible:
  - Ensure your sketch calls `Serial.begin(...)` and prints to the correct port.
  - Open the Wokwi Serial Monitor panel after starting the simulation.
- If Wokwi cannot run the project due to unsupported parts or features:
  - Replace unsupported components with functional equivalents available in the Wokwi parts library.
- If you want to iterate quickly:
  - Edit code in Wokwi and use the Run button to test changes without leaving the browser.
  - For larger codebases, continue editing locally and upload or paste the updated sketch into Wokwi.

---

## Exporting and sharing

- After loading or editing a project in Wokwi you can:
  - Export the project (zip) to use with PlatformIO locally.
  - Share a link (if using Wokwi’s publish/share features) so others can open the same simulation.

---

## Where to go next

- Open `diagram.json` in Wokwi and add/paste the sketch code into the Code panel, then run the simulation to observe:
  - how the keypad input changes displayed messages,
  - how shift-register outputs (relays/LEDs) are simulated,
  - serial logs from the firmware.
- If you'd like, I can help prepare a minimal, Wokwi-ready example sketch you can paste into the editor — tell me which MCU and which pins you want the sketch to use.

--- 

If anything in the editor looks off after loading the JSON, paste the exact Wokwi error or describe what does not match the expected hardware behavior and I’ll help you resolve it.