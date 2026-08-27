/*
  SEDS BPHC Avionics Round 1 Induction - "Athena's Intern"
  Task 2: Keeping Watch Over Odysseus

  Name: Ojasvi Cheruku
  ID:   2025AAPS0226H

  Components (wire these up in Tinkercad):
    - Arduino Uno
    - HC-SR04 ultrasonic distance sensor  -> TRIG_PIN, ECHO_PIN
    - Photoresistor (light sensor) in a voltage divider -> LIGHT_PIN (analog)
    - 16x2 LCD, I2C backpack version (PCF8574) -> A4 (SDA), A5 (SCL);
      default address 0x27 (see LCD_I2C_ADDR)
    - Push button (with pull-down resistor, or use INPUT_PULLUP as below) -> BUTTON_PIN
    - LED (with current-limiting resistor) -> LED_PIN
    - Buzzer -> BUZZER_PIN

  States: OPEN_SEA, ANCHOR_DROPPED, STORM, CHARYBDIS, WRECKED
    - Default/start state: OPEN_SEA
    - Button press toggles the anchor (drops/raises). While anchor is
      dropped, the ship is immune to STORM/CHARYBDIS/WRECKED.
    - STORM triggers when light reading < half of max range: LED blinks.
    - CHARYBDIS triggers when distance < 100 cm: buzzer sounds (pulsed,
      siren-style, synced to the flashing ALARM text on the LCD).
    - If Storm and Charybdis conditions are true simultaneously, whichever
      one is already active keeps running (first-entered wins); if neither
      is active yet and both trip together, STORM is given priority as a
      tie-break (documented choice - see README).
    - Staying in STORM or CHARYBDIS for >= 5 continuous seconds (without the
      anchor being dropped) wrecks the ship permanently, until reset.

  BONUS - "little pictures" on the status screen:
    Tinkercad's component library doesn't include a graphical OLED/TFT, only
    the character 16x2 LCD. So instead of pixel-graphics on a second screen,
    this sketch uses the HD44780's custom-character feature (lcd.createChar)
    to draw small 5x8 pixel-art glyphs directly on the LCD itself: a boat,
    an anchor, a storm cloud + lightning bolt, a spinning whirlpool (two
    alternating frames), and a skull for WRECKED. Row 0 shows the state
    name (flashing during STORM/CHARYBDIS); row 1 shows the animated icon(s).
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- LCD (I2C backpack) ----------
// Most PCF8574 backpacks default to 0x27; some clones use 0x3F. If the LCD
// shows nothing/garbled characters in Tinkercad, try 0x3F here.
const uint8_t LCD_I2C_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

const int TRIG_PIN = 9;
const int ECHO_PIN = 10;
const int LIGHT_PIN = A0;
const int BUTTON_PIN = 7;
const int LED_PIN = 8;
const int BUZZER_PIN = 6;

// ---------- Thresholds ----------
const int LIGHT_MAX = 1023;                 // analogRead range
const int LIGHT_THRESHOLD = LIGHT_MAX / 2;   // "below half" triggers storm
const int DISTANCE_THRESHOLD_CM = 100;       // charybdis trigger distance

// ---------- State machine ----------
enum ShipState { OPEN_SEA, ANCHOR_DROPPED, STORM, CHARYBDIS, WRECKED };
ShipState state = OPEN_SEA;
ShipState preAnchorState = OPEN_SEA;  // state to resume once anchor is raised

bool anchorDown = false;
unsigned long dangerStartTime = 0;    // when current STORM/CHARYBDIS episode began
const unsigned long WRECK_TIME_MS = 5000;

// LED blink (storm) bookkeeping - also drives the LCD's "STORM" text flash
// and the cloud/bolt icon flicker.
unsigned long lastBlinkTime = 0;
bool ledOn = false;
const unsigned long BLINK_INTERVAL_MS = 300;

// Charybdis alarm bookkeeping - drives the pulsed buzzer + LCD ALARM flash.
unsigned long lastAlarmToggle = 0;
bool alarmOn = false;
const unsigned long ALARM_INTERVAL_MS = 220;

// Button debounce bookkeeping
int lastButtonReading = HIGH;   // INPUT_PULLUP -> idle HIGH
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;

// ---------- Custom LCD icon glyphs (5x8 pixels each, 8 slots max: 0-7) ----------
byte ICON_BOAT   = 0;
byte ICON_WAVE   = 1;
byte ICON_ANCHOR = 2;
byte ICON_CLOUD  = 3;
byte ICON_BOLT   = 4;
byte ICON_WHIRL_A = 5;
byte ICON_WHIRL_B = 6;
byte ICON_SKULL  = 7;

byte glyphBoat[8] = {
  B00100,
  B00100,
  B00100,
  B01110,
  B01110,
  B11111,
  B11111,
  B00000
};
byte glyphWave[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B01010,
  B10101,
  B00000
};
byte glyphAnchor[8] = {
  B00100,
  B01110,
  B00100,
  B00100,
  B10101,
  B10101,
  B01110,
  B00100
};
byte glyphCloud[8] = {
  B00000,
  B01110,
  B11111,
  B11111,
  B01110,
  B00000,
  B00000,
  B00000
};
byte glyphBolt[8] = {
  B00011,
  B00110,
  B01100,
  B11111,
  B00110,
  B01100,
  B11000,
  B00000
};
byte glyphWhirlA[8] = {
  B00000,
  B00100,
  B01110,
  B11011,
  B11011,
  B01110,
  B00100,
  B00000
};
byte glyphWhirlB[8] = {
  B00000,
  B01000,
  B00110,
  B01101,
  B10110,
  B01100,
  B00010,
  B00000
};
byte glyphSkull[8] = {
  B01110,
  B10101,
  B10101,
  B11111,
  B01110,
  B01110,
  B10101,
  B00000
};

// Icon-row animation bookkeeping (separate cadence from LED/alarm blinking)
unsigned long lastIconUpdate = 0;
const unsigned long ICON_INTERVAL_MS = 300;
int boatCol = 6;       // current column of the boat icon on row 1 (bobs left/right)
int boatDir = 1;
bool whirlFrameA = true;

// Only rewrite the LCD's text row when the displayed state actually changes
ShipState lastDisplayedState = WRECKED;  // force a first draw
bool anchorDisplayedLast = false;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();

  lcd.createChar(ICON_BOAT, glyphBoat);
  lcd.createChar(ICON_WAVE, glyphWave);
  lcd.createChar(ICON_ANCHOR, glyphAnchor);
  lcd.createChar(ICON_CLOUD, glyphCloud);
  lcd.createChar(ICON_BOLT, glyphBolt);
  lcd.createChar(ICON_WHIRL_A, glyphWhirlA);
  lcd.createChar(ICON_WHIRL_B, glyphWhirlB);
  lcd.createChar(ICON_SKULL, glyphSkull);

  updateStatusRow();
  updateIconRow();

  Serial.begin(9600);
}

void loop() {
  handleButton();

  long distance = readDistanceCM();
  int light = analogRead(LIGHT_PIN);

  bool stormCondition = light < LIGHT_THRESHOLD;
  bool charybdisCondition = distance > 0 && distance < DISTANCE_THRESHOLD_CM;

  updateStateMachine(stormCondition, charybdisCondition);
  updateOutputs();
  updateStatusRow();
  updateIconRow();

  delay(50);  // small loop delay - keeps button/timers responsive
}

// --------------------------------------------------------------------- //
// Reads the anchor push-button with simple debouncing and toggles anchor
// state on a clean press (HIGH -> LOW edge, since INPUT_PULLUP is active-low).
// --------------------------------------------------------------------- //
void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    static int stableState = HIGH;
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {  // button just pressed
        toggleAnchor();
      }
    }
  }

  lastButtonReading = reading;
}

void toggleAnchor() {
  if (state == WRECKED) return;  // wrecked is permanent until reset

  if (!anchorDown) {
    // Dropping anchor: remember what we were doing, go safe.
    anchorDown = true;
    preAnchorState = (state == ANCHOR_DROPPED) ? OPEN_SEA : state;
    state = ANCHOR_DROPPED;
    dangerStartTime = 0;  // any in-progress danger timer resets
  } else {
    // Raising anchor: resume whatever danger (or calm) was happening.
    anchorDown = false;
    state = preAnchorState;
    if (state == STORM || state == CHARYBDIS) {
      dangerStartTime = millis();  // restart the wreck timer fresh
    }
  }
}

// --------------------------------------------------------------------- //
// Core transition logic, called every loop with the latest sensor booleans.
// --------------------------------------------------------------------- //
void updateStateMachine(bool stormCondition, bool charybdisCondition) {
  if (state == WRECKED || state == ANCHOR_DROPPED) {
    // Anchor overrides all danger; wrecked is a terminal state.
    return;
  }

  bool inStorm = (state == STORM);
  bool inCharybdis = (state == CHARYBDIS);

  if (inStorm) {
    if (!stormCondition) {
      // Storm cleared before 5s -> back to open sea
      state = OPEN_SEA;
      dangerStartTime = 0;
    } else if (millis() - dangerStartTime >= WRECK_TIME_MS) {
      state = WRECKED;
    }
    // else: still in storm, timer keeps running
  } else if (inCharybdis) {
    if (!charybdisCondition) {
      state = OPEN_SEA;
      dangerStartTime = 0;
    } else if (millis() - dangerStartTime >= WRECK_TIME_MS) {
      state = WRECKED;
    }
  } else {
    // Currently OPEN_SEA: check for new danger.
    // If both trip on the same loop iteration, Storm wins the tie-break.
    if (stormCondition) {
      state = STORM;
      dangerStartTime = millis();
    } else if (charybdisCondition) {
      state = CHARYBDIS;
      dangerStartTime = millis();
    }
  }
}

// --------------------------------------------------------------------- //
// Drives LED (blinking in storm) and buzzer (pulsed siren in charybdis).
// The same alarmOn flag also drives the LCD's flashing ALARM text, so the
// buzzer beeps and the on-screen warning flash stay in sync.
// --------------------------------------------------------------------- //
void updateOutputs() {
  if (state == STORM) {
    if (millis() - lastBlinkTime >= BLINK_INTERVAL_MS) {
      lastBlinkTime = millis();
      ledOn = !ledOn;
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    ledOn = false;
  }

  if (state == CHARYBDIS) {
    if (millis() - lastAlarmToggle >= ALARM_INTERVAL_MS) {
      lastAlarmToggle = millis();
      alarmOn = !alarmOn;
    }
    digitalWrite(BUZZER_PIN, alarmOn ? HIGH : LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    alarmOn = false;
  }
}

// --------------------------------------------------------------------- //
// Row 0: state name text. Only rewritten when the state (or anchor flag)
// actually changes, or every loop while flashing text needs to alternate
// (STORM / CHARYBDIS), to avoid unnecessary flicker the rest of the time.
// --------------------------------------------------------------------- //
void updateStatusRow() {
  bool flashingState = (state == STORM || state == CHARYBDIS);
  if (state == lastDisplayedState && anchorDown == anchorDisplayedLast && !flashingState) {
    return;
  }
  lastDisplayedState = state;
  anchorDisplayedLast = anchorDown;

  lcd.setCursor(0, 0);
  lcd.print("                ");  // clear row 0 (16 spaces)
  lcd.setCursor(0, 0);

  switch (state) {
    case OPEN_SEA:
      lcd.print("OPEN SEA");
      break;
    case ANCHOR_DROPPED:
      lcd.print("ANCHOR DROPPED");
      break;
    case STORM:
      lcd.print(ledOn ? "STORM!" : "STORM");
      break;
    case CHARYBDIS:
      lcd.print(alarmOn ? "CHARYBDIS ALARM" : "CHARYBDIS");
      break;
    case WRECKED:
      lcd.print("WRECKED");
      break;
  }
}

// --------------------------------------------------------------------- //
// Row 1: animated pixel-icon scene for the current state, throttled to
// ICON_INTERVAL_MS so the animation is visible but not flickery.
// --------------------------------------------------------------------- //
void updateIconRow() {
  if (millis() - lastIconUpdate < ICON_INTERVAL_MS) return;
  lastIconUpdate = millis();

  lcd.setCursor(0, 1);
  lcd.print("                ");  // clear row 1

  switch (state) {
    case OPEN_SEA: {
      // boat drifts left/right across a row of waves
      boatCol += boatDir;
      if (boatCol >= 14 || boatCol <= 1) boatDir = -boatDir;
      for (int c = 0; c < 16; c++) {
        lcd.setCursor(c, 1);
        lcd.write(c == boatCol ? ICON_BOAT : ICON_WAVE);
      }
      break;
    }
    case ANCHOR_DROPPED: {
      lcd.setCursor(6, 1);
      lcd.write(ICON_BOAT);
      lcd.setCursor(9, 1);
      lcd.write(ICON_ANCHOR);
      break;
    }
    case STORM: {
      // cloud + bolt flicker in sync with the physical LED
      if (ledOn) {
        lcd.setCursor(6, 1);
        lcd.write(ICON_CLOUD);
        lcd.setCursor(7, 1);
        lcd.write(ICON_BOLT);
      }
      break;
    }
    case CHARYBDIS: {
      // whirlpool "spins" by alternating two icon frames across the row
      whirlFrameA = !whirlFrameA;
      for (int c = 0; c < 16; c++) {
        lcd.setCursor(c, 1);
        bool useA = ((c + (whirlFrameA ? 0 : 1)) % 2 == 0);
        lcd.write(useA ? ICON_WHIRL_A : ICON_WHIRL_B);
      }
      break;
    }
    case WRECKED: {
      lcd.setCursor(7, 1);
      lcd.write(ICON_SKULL);
      break;
    }
  }
}

// --------------------------------------------------------------------- //
// Standard HC-SR04 pulse-timing distance read, returns centimeters.
// Returns -1 if no echo received (out of range / sensor glitch).
// --------------------------------------------------------------------- //
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 30ms timeout ~ 5m range
  if (duration == 0) return -1;

  long distanceCm = duration * 0.0343 / 2;
  return distanceCm;
}