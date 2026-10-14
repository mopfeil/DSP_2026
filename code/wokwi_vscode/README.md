# Wokwi projects for VS Code

Copies of the Wokwi projects in `code/wokwi/` (student versions) and
`code/solutions/wokwi/` (reference solutions, in `code/solutions/wokwi_vscode/`) that run in
the **Wokwi for VS Code** extension instead of the browser simulator on wokwi.com.

The difference to the browser: Wokwi for VS Code does **not compile** anything.
You build the firmware locally with PlatformIO, and Wokwi simulates the resulting file.
The custom chips (`engine-sim`, `lambda-probe`) are also not compiled by Wokwi:
they come prebuilt as WebAssembly in `chips/`.

```
src/sketch.ino ──(PlatformIO: Build)──▶ .pio/build/<uno|esp32>/firmware.hex/.bin/.elf ─┐
chips/*.chip.wasm + *.chip.json (prebuilt) ─────────────────────────────────────────────┤
diagram.json + wokwi.toml ──────────────────────(Wokwi: Start Simulator)────────────────┴─▶ simulation
```

## Folder layout

```
code/wokwi_vscode/
  chips/                    engine-sim and lambda-probe: .chip.wasm (prebuilt) + .chip.json, Makefile
  sheetNN_<name>/           one PlatformIO project per Wokwi project of the sheets
    src/sketch.ino          copy of code/wokwi/sheetNN_<name>/sketch.ino
    src/*.h                 copies of the extra headers (fft.h, knock_filter.h, resonator.h)
    diagram.json            copy of the circuit (board, engine-sim chip, logic analyzer, ...)
    platformio.ini          build settings (Arduino Uno or ESP32)
    wokwi.toml              firmware path, logic analyzer file, custom chips
  wokwi_vscode.code-workspace   all student projects in one VS Code window
  sync.sh                   regenerates the copies from code/wokwi/ and code/solutions/wokwi/
```

| Board | PlatformIO environment | Firmware for Wokwi |
|-------|------------------------|--------------------|
| Arduino Uno (`sheet00`–`04`, `*_uno`, `sheet08_unofft`, `sheet09`) | `uno`, platform `atmelavr` | `.pio/build/uno/firmware.hex` |
| ESP32 DevKit v1 (`*_esp32`, `sheet08_knockfft`) | `esp32`, platform pioarduino 55.03.312 (Arduino-ESP32 core 3.3) | `.pio/build/esp32/firmware.bin` |

The ESP32 sketches use the core-3 API (`ledcAttach`, ...), which the official PlatformIO
`espressif32` platform does not provide yet; therefore `platformio.ini` uses the community
platform [pioarduino](https://github.com/pioarduino/platform-espressif32).

Nothing in the sketches is changed for VS Code: `diagram.json` and `sketch.ino` are the same as in the browser.
In the exercise sheets, *"open the project in Wokwi"* means: open the folder with the same name here.
`code/wokwi/` stays the master copy (see *For instructors* below).

## One-time setup

1. Install [VS Code](https://code.visualstudio.com/).
2. Install the extensions **PlatformIO IDE** (`platformio.platformio-ide`) and
   **Wokwi Simulator** (`wokwi.wokwi-vscode`).
   On the first build PlatformIO downloads the compiler and the Arduino core:
   a few minutes for the Uno, considerably longer (about 1 GB) for the ESP32.
3. Activate the Wokwi license (free, needs a Wokwi account):
   `F1` → **Wokwi: Request a New License** → confirm in the browser (*Get your license*) →
   confirm again in VS Code. VS Code shows *License activated for …*.
   The free license has to be renewed from time to time; VS Code tells you when.

You do **not** need a C/WebAssembly compiler: the chips are prebuilt.

## Workflow

### 1. Open a project

Either open one project folder directly (*File → Open Folder… → `sheet01_crank`*),
or open all of them at once with *File → Open Workspace from File… → `wokwi_vscode.code-workspace`*.

Wokwi and PlatformIO look for their config files (`wokwi.toml`, `platformio.ini`) in the root of a workspace folder,
so do not open `code/` or the whole repository as a folder. In the multi-folder workspace:

- PlatformIO: choose the project in the status bar (folder icon, *Switch PlatformIO Project Environment*)
  or build it from the PlatformIO sidebar (*Project Tasks → sheetNN_… → uno/esp32 → General → Build*).
- Wokwi: the first `F1` → **Wokwi: Start Simulator** asks which `wokwi.toml` to use.
  To switch to another project later: `F1` → **Wokwi: Select Config File**.

### 2. Build

Click the ✓ (*PlatformIO: Build*) in the status bar, or run `pio run` in the project folder.
Successful output ends with `[SUCCESS]` and creates the firmware in `.pio/build/<env>/`.

The sheets ask you to complete `TODO`s in the sketch: edit `src/sketch.ino` (or the header in `src/`)
of the project folder here.

### 3. Simulate

`F1` → **Wokwi: Start Simulator**. A tab with the circuit opens and the simulation starts.

- Serial output appears in the VS Code terminal panel; you can also type input there
  (e.g. the commands of the knock-control sketches).
- Click parts to interact: potentiometers, push buttons, slide switch.
  Clicking the `engine-sim` chip shows its controls (engine speed, knock probability, …),
  the same sliders as in the browser.
- The simulation runs only while the simulator tab is visible. Keep it open, e.g. in a split editor next to the code.
- Pause/resume with the button in the simulator tab.

### 4. Edit, rebuild, restart

Change `src/sketch.ino`, build again (✓), then restart the simulation
(`F1` → **Wokwi: Start Simulator** again).
Wokwi always simulates the last built firmware: if the code change seems to have no effect, check that the build succeeded.

Chip parameters set in `diagram.json` (`attrs` of the `chip-engine-sim` part, e.g. `rpm`, `fs_khz`, `vmax`)
take effect on the next simulator start; no rebuild needed.

### 5. Logic analyzer and data

The logic analyzer records while the simulation runs. When you **stop** the simulation
(close the simulator tab or `F1` → **Wokwi: Stop Simulator**), Wokwi writes `wokwi.vcd` into the project folder
(path set by `vcdFile` in `wokwi.toml`); in the browser you would download it instead.
Open it in [Surfer](https://app.surfer-project.org/) (drag and drop the file), GTKWave,
or the *Surfer* VS Code extension.
Keep the run short (a few seconds of simulated time): the logic analyzer has a limited sample buffer.

Serial data for the JSLinux exercises: copy the lines from the terminal panel into a file,
the same as from the browser's Serial Monitor.

### 6. Optional: debugging with GDB

Every `wokwi.toml` sets `gdbServerPort = 3333`. To step through the code:

1. Install the **C/C++** extension (`ms-vscode.cpptools`) and a GDB for the target:
   a recent `avr-gdb` for the Uno, `xtensa-esp32-elf-gdb` for the ESP32
   (see <https://docs.wokwi.com/vscode/debugging>).
2. Create `.vscode/launch.json` in the project folder (Uno shown; for the ESP32 use
   `.pio/build/esp32/firmware.elf` and `xtensa-esp32-elf-gdb`):

   ```json
   {
     "version": "0.2.0",
     "configurations": [
       {
         "name": "Wokwi GDB",
         "type": "cppdbg",
         "request": "launch",
         "program": "${workspaceFolder}/.pio/build/uno/firmware.elf",
         "cwd": "${workspaceFolder}",
         "MIMode": "gdb",
         "miDebuggerPath": "avr-gdb",
         "miDebuggerServerAddress": "localhost:3333"
       }
     ]
   }
   ```

3. `F1` → **Wokwi: Start Simulator and Wait for Debugger**, then `F5`. Always start Wokwi first.

A breakpoint stops the simulated CPU but not the engine chip, so samples and teeth are missed while halted:
use breakpoints for logic errors, not for timing measurements.

## Command line (without VS Code)

```
cd code/wokwi_vscode/sheet01_crank
pio run                     # build
wokwi-cli --timeout 5000 .  # simulate 5 s (needs WOKWI_CLI_TOKEN from https://wokwi.com/dashboard/ci)
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| *Firmware not found* when starting Wokwi | Build first; check the path in `wokwi.toml` |
| *Chip binary not found* / `chip-engine-sim` unknown | The project folder was moved: `binary` in `wokwi.toml` is a path relative to the project folder (`../chips/…`) |
| Wokwi asks for a license | `F1` → **Wokwi: Request a New License** |
| Simulation stalls | Simulator tab hidden: bring it to the front |
| No `wokwi.vcd` | The file is written only when the simulation is stopped |
| ESP32: `'ledcAttach' was not declared` | Old Arduino-ESP32 core 2.x: the `platform` line in `platformio.ini` must point to pioarduino |
| Wrong project simulated in the multi-folder workspace | `F1` → **Wokwi: Select Config File** |
| `fft.h: No such file` | Extra headers belong in `src/` next to `sketch.ino` |

## For instructors

The projects here are generated; edit the master copies in `code/wokwi/` and
`code/solutions/wokwi/`, then:

```
code/wokwi_vscode/sync.sh           # copy sketch.ino, *.h, diagram.json; regenerate platformio.ini and wokwi.toml
code/wokwi_vscode/sync.sh --check   # only list differences (exit code 1 if any)
```

A new project in `code/wokwi/` gets its VS Code copy automatically on the next `sync.sh`
(board and chips are read from its `diagram.json`); add it to `wokwi_vscode.code-workspace`.

After changing `code/common/engine_signals.h` or a chip source:

```
tools/make_chips.sh                                   # regenerate code/wokwi/chips/*.chip.c
cd code/wokwi_vscode/chips && make WASI_SDK=/path/to/wasi-sdk   # rebuild the .wasm
```

The Makefile needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases) (tested with wasi-sdk 34)
and downloads `wokwi-api.h` from wokwi.com. `sync.sh` reports an outdated `.wasm`
(it compares the MD5 of the chip source with `*.chip.src.md5`).

Solutions: `code/solutions/wokwi_vscode/` has the same structure and uses the chips from
`code/wokwi_vscode/chips/`. Do not hand it out.

Status: all projects build with `pio run` (PlatformIO Core 6.2). Not yet run in the
Wokwi for VS Code extension itself; see the checklist in the top-level `README.md`.
