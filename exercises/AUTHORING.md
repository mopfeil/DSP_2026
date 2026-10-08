# Authoring guide for the DSP exercise sheets

Master-level lecture "Digital Signal Processing -- using automotive engine
signals as a guiding example". Script: `/home/shared/dsp/DSP_Lecture.pdf`
(only chapters 1-2 exist yet; later chapters follow the standard DSP canon).
Outline of all sheets: `/home/shared/dsp/outline/Exercise_Outline.tex` --
follow it, but you may refine exercises if this improves them.
Reference sheet (style, depth, tone): `exercises/sheets/sheet00.tex`.

## Repository layout

```
exercises/dspex.sty            macros (read it!)  -- do NOT modify
exercises/sheets/sheetNN.tex   one file per sheet (NN = 00..12)
exercises/Makefile             `make sheetNN` builds exercise + solution PDF
code/common/                   engine_signals.h, asciiplot.h, csvio.h -- do NOT modify
code/jslinux/sheetNN/          student C programs (templates with TODOs)
code/bonus/jslinux/sheetNN/    optional JSLinux bonus exercises (sheets 00-01;
                               solutions in code/solutions/bonus/jslinux/)
code/wokwi/sheetNN_<name>/     student Wokwi projects: sketch.ino, diagram.json,
                               (+ copies of needed headers, see below)
code/wokwi/chips/              engine-sim + lambda-probe custom chips (generated)
code/solutions/jslinux/sheetNN/        complete reference solutions
code/solutions/wokwi/sheetNN_<name>/   complete reference solutions
tools/wokwi_mock/              PC mock of the Wokwi chip API (for chip testing)
```

If you think the common code or the chips need a change, do NOT edit them;
describe the needed change in your final report.

## Sheet structure (LaTeX)

```latex
\sheet{N}{Title}{Chapter~N (...)}
\begin{goals} ... \end{goals}
(short intro / background, formulas, figure if helpful -- tikz/pgfplots ok)
\begin{exercise}{Title}{\Paper}       % or \JSLinux, \Wokwi, \Challenge
  text
  \begin{tasks} \item ... \end{tasks}
\end{exercise}
\begin{solution} ... \end{solution}  % directly after each exercise
...
\begin{deliverables} ... \end{deliverables}
```
* Per sheet: 1-2 paper exercises, 2-3 lab exercises (JSLinux and/or Wokwi as
  in the outline), optionally one challenge. Workload: 90 min tutorial +
  ~3 h homework. Master level: derivations, quantitative answers, design
  trade-offs -- not just "explain X".
* Solutions must be complete and *correct*: derivations, numerical results
  (obtained by actually running your solution code!), expected plots
  described, discussion. For code tasks show the key part of the solution
  code with `\inputcode[firstline=..,lastline=..]{solutions/...}` or a
  short `lstlisting`.
* Language: English. Use `\SI{}{}` for units, `\jj` for imaginary unit.
* Refer to files as `\codefile{jslinux/sheet05/alias.c}`.
* The sheet must build without errors: `cd exercises && make sheetNN`
  (check build/SheetNN*.log for errors; overfull boxes > 20pt should be fixed).
* Do not use \section numbering; use `\section*{}` for intro parts only if needed.

## Engine model conventions (code/common/engine_signals.h)

Read the header. Key facts: 4 cyl, crank angle 0..720 deg, TDC at
120/300/480/660 deg (cyl 1,3,4,2), 60-2 wheel (tooth k HIGH for
[6k,6k+3) deg, teeth 58,59 missing, reference = rising edge of tooth 0 at
0/360 deg), cam HIGH for 0..180 deg, knock modes 6.5 kHz and 10.5 kHz,
tau 0.8 ms, onset 5..25 deg ATDC, valve bursts 8 kHz at TDC+90 deg,
speed ripple 1 % at 2nd order. API: `es_init`, `es_advance(&e, dt)`,
`es_crank`, `es_cam`, `es_knock`, `es_ion`, `es_lambda_voltage(lambda)`,
`es_omega`, ground truth `e.knock_flag[c]`, `e.knock_t0`, `e.last_cyl`.

## JSLinux code rules

* Plain C99 + libm; must compile with **both** `gcc -std=c99 -Wall` and
  with tcc-style simplicity (no VLAs in structs, no GNU extensions, no
  complex.h -- write your own small complex struct if needed).
  Students compile with `gcc -O2 -o prog prog.c -lm` with all headers in
  the same directory -- so use `#include "engine_signals.h"` (test with
  `-I../../common`, resp. `-I../../../common` for solutions).
* No external tools (no Python, gnuplot) -- plots via `asciiplot.h`,
  data via `csvio.h` (CSV files can be downloaded from JSLinux).
* Keep memory moderate (JSLinux VM has ~ 192 MB): arrays up to ~1e6 doubles ok.
* Runtime of each solution < ~5 s natively (JSLinux is ~50x slower).
* Templates: same program as the solution but with the essential parts
  replaced by `/* TODO (a): ... */` stubs; templates must compile and run
  (producing trivial output). Header comment: sheet, exercise, build line.
* Test: compile & run every solution and template; paste real numbers into
  the LaTeX solutions.

## Wokwi code rules

* Boards: Arduino Uno (`wokwi-arduino-uno`, 10-bit ADC 0..5 V, no FPU,
  2 KB RAM!) and ESP32 (`wokwi-esp32-devkit-v1`, 12-bit ADC 0..3.3 V,
  use ADC1 pins e.g. `esp:D34`/`esp:D35` (devkit-v1 pin names carry a D prefix; serial: `esp:TX0`/`esp:RX0`), Arduino core 3.x).
* Project folder contains `sketch.ino`, `diagram.json` and, if used,
  copies of `engine-sim.chip.c` / `engine-sim.chip.json` (copy from
  `code/wokwi/chips/`, do not edit) and any header the sketch includes
  (e.g. a copy of `engine_signals.h` or your own `filters.h`).
* Custom chip in diagram.json:
  `{ "type": "chip-engine-sim", "id": "eng", "top": .., "left": ..,
     "attrs": { "vmax": "5", "offset": "2.5" } }` (Uno) or vmax 3.3 /
  offset 1.65 (ESP32, default). Pins: CRANK CAM TDC KNK GND VCC ION KNOCK.
  KNOCK analog = offset + 0.6 * knock signal; ION analog = 0.6*ion.
  KNK = HIGH while a knock burst is active (ground truth), TDC = 50 us
  pulse at each combustion TDC. Sliders: rpm, knock_prob, knock_amp, noise.
  Lambda chip `chip-lambda-probe`: pins INJ (PWM fuel command input,
  50 % duty = stoichiometric), O2 (sensor voltage), LAM (true lambda/2 V);
  sliders air (value-10 = % unmetered air), delay (ms).
* Logic analyzer: `wokwi-logic-analyzer`, pins D0..D7, GND.
* Sampling: deadline loop with `micros()` (see sheet00 sketch) or, on the
  Uno, Timer1 CTC interrupt via registers; on ESP32 avoid the hw-timer API
  (changed between core 2/3) -- use deadline loop or `esp_timer`.
* Serial output: CSV with header line, 115200 baud (ESP32: 115200 too).
  Remember the UART bottleneck (11.5 chars/ms): buffer a block of samples
  in RAM, then print it.
* Compile-check every sketch with arduino-cli (see below). For ESP32 use
  FQBN `esp32:esp32:esp32`, Uno `arduino:avr:uno`.
* Wokwi is not cycle-accurate: no execution-time claims from Wokwi; use
  operation counts or `micros()` only as indicative.

### arduino-cli (already installed)

```
S=/tmp/claude-1000/-home-shared-dsp/113e4460-b4f4-441e-ae80-7be25e0c18c3/scratchpad/ard
export ARDUINO_DIRECTORIES_DATA=$S/data ARDUINO_DIRECTORIES_USER=$S/user ARDUINO_DIRECTORIES_DOWNLOADS=$S/dl
$S/arduino-cli compile --fqbn arduino:avr:uno <sketchdir>   # dir name must equal .ino name!
```
arduino-cli requires the .ino name to equal the folder name: copy the
project to a temp dir `/tmp/claude-1000/<agent>/<x>/<x>.ino` (plus headers)
to compile. Chip `.chip.c` files are not compiled by arduino-cli -- leave
them out of the temp copy. Use a private build path:
`--build-path /tmp/claude-1000/<agent>/build_<x>`.

## Final report (your last message)

List files created, test status (compiled / ran / build ok), real numerical
results you relied on, and anything uncertain (e.g. Wokwi behaviour that
could not be tested) -- keep it short.
