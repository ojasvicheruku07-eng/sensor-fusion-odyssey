# Athena's Intern — SEDS BPHC Avionics Round 1 Induction Task

**Name:** Ojasvi Cheruku
**ID:** 2025AAPS0226H

Two tasks, both themed around getting Odysseus and crew back to Ithaca in one piece: a sea-floor depth visualizer (Python) and an onboard hazard-monitoring state machine (Arduino, simulated in Tinkercad).

---

## Task 1: Finding the Sea Floor

**Files:** `depth_monitor.py`, `Depth_Data.csv` (sample input), `depth_monitor_graph.png` (screenshot), `depth_animation.gif` (generated on run)

### Approach
- **Load:** `load_data()` reads the CSV with pandas, coerces the depth column to numeric so malformed entries (e.g. `#VALUE!`) become `NaN` instead of crashing the script, and converts the sample index into seconds (1 sample/sec, per the task's stated sampling rate).
- **Clean:** `clean_data()` flags three kinds of bad readings — non-numeric (`NaN`), physically implausible values (depth ≥ 0, i.e. above the sea surface), and statistical spikes (points that deviate from a centered rolling median by more than `SPIKE_THRESHOLD_M`). All flagged points are linearly interpolated from their neighbors so the series stays continuous, and their original indices are kept so the plot can mark them transparently.
- **Smooth (brownie points):** `smooth_data()` applies a Savitzky-Golay filter, which reduces sample-to-sample noise while preserving the actual shape/slope of the seafloor better than a plain moving average would.
- **Animate & present:** `animate_depth()` builds a live-updating matplotlib figure that reveals one new point per frame (adding the "just recorded" feel the task asks for), overlays the corrected points in green, shows a rolling caution band near the shallowest 5th-percentile depth as an example intervention threshold, and saves the whole thing as a GIF (`depth_animation.gif`) in addition to displaying it.

### Running it
```
pip install pandas numpy matplotlib scipy
python depth_monitor.py
```
Expects `Depth_Data.csv` in the same directory (path configurable via `CSV_PATH` at the top of the script).

---

## Task 2: Keeping Watch Over Odysseus

**Files:** `task_2.ino`, `Task_2.pdf` (schematic), `Task_2.png` (breadboard-view wiring screenshot), `Task_2.csv` (Tinkercad bill of materials)

### Components (from Tinkercad BOM)
Arduino Uno R3 · HC-SR04 ultrasonic distance sensor · photoresistor (light sensor, in a voltage-divider with a 10kΩ resistor) · 16x2 I2C LCD · pushbutton (anchor control) · red LED (220Ω resistor) · piezo buzzer.

### Pin mapping
| Component | Pin |
|---|---|
| HC-SR04 TRIG / ECHO | D9 / D10 |
| Photoresistor (analog) | A0 |
| Pushbutton (anchor) | D7, `INPUT_PULLUP` |
| LED | D8 |
| Buzzer | D6 |
| LCD (I2C) | A4 (SDA), A5 (SCL), address `0x27` |

### State machine
Implements all five required states — `OPEN_SEA` (default), `ANCHOR_DROPPED`, `STORM`, `CHARYBDIS`, `WRECKED` — with the current state name printed on the LCD's top row.

- **Anchor:** the button toggles `anchorDown`. Dropping it saves whatever state the ship was in (`preAnchorState`) and switches to `ANCHOR_DROPPED`, pausing any in-progress danger timer. Raising it resumes that saved state and, if it was a hazard, restarts the 5-second wreck timer fresh — matching the task's "resets the timer" language.
- **Storm / Charybdis:** triggered by `light < 511` (below half of the 0–1023 analog range) and `distance < 100 cm` respectively. Each hazard is timed from the moment it's entered (`dangerStartTime`); staying continuously past `WRECK_TIME_MS` (5000 ms) moves the ship to `WRECKED`, which is terminal until the simulation restarts.
- **Simultaneous trigger tie-break:** the task specifies that whichever hazard is *already active* keeps priority — implemented directly (`updateStateMachine()` only re-checks the currently active hazard's own condition, ignoring the other). For the edge case of both conditions becoming true on the *same* loop iteration from `OPEN_SEA` (not covered explicitly by the task), `STORM` was chosen as the tie-break and documented in the code comments.
- **Outputs:** LED blinks (300ms interval) during `STORM`; buzzer pulses in a siren pattern (220ms interval) during `CHARYBDIS`. Both are driven off `millis()`, not `delay()`, so the state machine and button stay responsive.

### Bonus: animated status icons
Tinkercad's parts library doesn't include a graphical OLED/TFT — only the character-based 16x2 LCD — so instead of true pixel graphics, the sketch uses `lcd.createChar()` to draw small 5x8 custom glyphs (boat, anchor, storm cloud + lightning bolt, a two-frame whirlpool, a skull) directly on the LCD's second row. Each state gets its own little animated scene: a boat drifting back and forth over waves in `OPEN_SEA`, a spinning whirlpool across the whole row in `CHARYBDIS`, a flickering cloud/bolt synced to the physical LED in `STORM`, and so on. The animation runs on its own timer (`ICON_INTERVAL_MS`), independent of the status text, and the status row itself only redraws when the state actually changes (or needs to flash) to avoid unnecessary flicker. (Note- Please make sure the LCD is PCF8574 based before running it)

### Running it
Import `task_2.ino` into a Tinkercad Circuits project wired per `Task_2.pdf` / `Task_2.png`, or match the pin table above manually. Add the `LiquidCrystal_I2C` library if not already present, then start the simulation. Drag the light sensor slider below half brightness to trigger `STORM`; drag the distance sensor below 100cm to trigger `CHARYBDIS`; click the pushbutton to drop/raise anchor. Or, just go to the link- https://www.tinkercad.com/things/faHolO5kGtK-task-2?sharecode=zw_sjEJiAAk6Dtyl_z7H5sIAxcQanbu3RWUCidyOeM4
