#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- RTC ----------------
RTC_DS3231 rtc;

// ---------------- Pins ----------------
const int RED_LED_PIN  = 5;
const int BLUE_LED_PIN = 6;
const int ENC_SW_PIN   = 4;

// ---------------- Feeding schedule ----------------
const int MORNING_HOUR = 5;
const int MORNING_MIN  = 0;

const int EVENING_HOUR = 18;
const int EVENING_MIN  = 30;

// AM escalation
const int ESC1_HOUR_AM = MORNING_HOUR + 1;
const int ESC1_MIN_AM  = 30;

const int ESC2_HOUR_AM = MORNING_HOUR + 2;
const int ESC2_MIN_AM  = 0;

const int ESC3_HOUR_AM = MORNING_HOUR + 3;
const int ESC3_MIN_AM  = 0;

// PM escalation
const int ESC1_HOUR_PM = EVENING_HOUR + 1;
const int ESC1_MIN_PM  = 30;

const int ESC2_HOUR_PM = EVENING_HOUR + 2;
const int ESC2_MIN_PM  = 0;

const int ESC3_HOUR_PM = EVENING_HOUR + 3;
const int ESC3_MIN_PM  = 0;

// ---------------- EEPROM ----------------
const int EEPROM_ADDR_UNIX = 0;
const int EEPROM_ADDR_SLOT = 8;

uint8_t currentMealSlot = 0;
uint8_t storedMealSlot = 0;
uint32_t lastFedUnix = 0;

// ---------------- Button debounce ----------------
bool lastStableState = HIGH;
bool lastReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 40;

// ---------------- Blink ----------------
unsigned long lastBlinkMillis = 0;
bool blinkState = false;

// ---------------- State ----------------
enum FeedState {
  STATE_WAITING,
  STATE_DUE,
  STATE_FED
};

FeedState feedState = STATE_WAITING;

// ---------------- EEPROM helpers ----------------
void writeUint32ToEEPROM(int address, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    EEPROM.update(address + i, (value >> (8 * i)) & 0xFF);
  }
}

uint32_t readUint32FromEEPROM(int address) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value |= ((uint32_t)EEPROM.read(address + i) << (8 * i));
  }
  return value;
}

// ---------------- Time helpers ----------------
int minutesSinceMidnight(const DateTime& now) {
  return now.hour() * 60 + now.minute();
}

int hmToMinutes(int h, int m) {
  return h * 60 + m;
}

void print2Digits(int value) {
  if (value < 10) display.print('0');
  display.print(value);
}

uint8_t getMealSlot(const DateTime& now) {
  int mins = minutesSinceMidnight(now);
  int morningStart = hmToMinutes(MORNING_HOUR, MORNING_MIN);
  int eveningStart = hmToMinutes(EVENING_HOUR, EVENING_MIN);

  uint16_t daySeed = (now.year() * 372) + (now.month() * 31) + now.day();

  if (mins >= eveningStart) {
    return (uint8_t)((daySeed * 2) & 0xFF);       // PM
  } else if (mins >= morningStart) {
    return (uint8_t)(((daySeed * 2) - 1) & 0xFF); // AM
  } else {
    return (uint8_t)(((daySeed * 2) - 1) & 0xFF); // before AM, next pending is AM
  }
}

bool isMealDueNow(const DateTime& now) {
  int mins = minutesSinceMidnight(now);
  return mins >= hmToMinutes(MORNING_HOUR, MORNING_MIN);
}

bool isMorningWindow(const DateTime& now) {
  int mins = minutesSinceMidnight(now);
  return mins < hmToMinutes(EVENING_HOUR, EVENING_MIN);
}

unsigned long getBlinkIntervalMs(const DateTime& now) {
  int mins = minutesSinceMidnight(now);

  if (isMorningWindow(now)) {
    if (mins >= hmToMinutes(ESC3_HOUR_AM, ESC3_MIN_AM)) return 500;
    if (mins >= hmToMinutes(ESC2_HOUR_AM, ESC2_MIN_AM)) return 1000;
    if (mins >= hmToMinutes(ESC1_HOUR_AM, ESC1_MIN_AM)) return 2000;
    return 0;
  } else {
    if (mins >= hmToMinutes(ESC3_HOUR_PM, ESC3_MIN_PM)) return 500;
    if (mins >= hmToMinutes(ESC2_HOUR_PM, ESC2_MIN_PM)) return 1000;
    if (mins >= hmToMinutes(ESC1_HOUR_PM, ESC1_MIN_PM)) return 2000;
    return 0;
  }
}

// ---------------- Button ----------------
bool buttonPressedEvent() {
  bool reading = digitalRead(ENC_SW_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != lastStableState) {
      lastStableState = reading;

      if (lastStableState == LOW) {
        lastReading = reading;
        return true;
      }
    }
  }

  lastReading = reading;
  return false;
}

// ---------------- Display screens ----------------
void showWaitingScreen(const DateTime& now) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Control de comida");
  display.println("de Maya");

  display.setCursor(0, 20);
  display.print("Hora: ");
  print2Digits(now.hour());
  display.print(':');
  print2Digits(now.minute());
  display.print(':');
  print2Digits(now.second());

  display.setCursor(0, 40);
  display.println("Siguiente comida:");

  display.setCursor(0, 54);
  display.print("05:30 / 19:30");

  display.display();
}

void showDueScreen(const DateTime& now) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Hora de alimentar");
  display.println("a Maya");

  display.setCursor(0, 24);
  display.println("Presione encoder");
  display.println("cuando ya comio.");

  display.setCursor(0, 54);
  print2Digits(now.hour());
  display.print(':');
  print2Digits(now.minute());

  display.display();
}

void showFedScreen(uint32_t elapsedSec, const DateTime& now) {
  char buffer[20];

  uint32_t h = elapsedSec / 3600UL;
  uint32_t m = (elapsedSec % 3600UL) / 60UL;
  uint32_t s = elapsedSec % 60UL;

  if (h > 0) {
    snprintf(buffer, sizeof(buffer), "%luh %lum", h, m);
  } else {
    snprintf(buffer, sizeof(buffer), "%lum %lus", m, s);
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Maya comio hace");

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(buffer);

  display.setTextSize(1);
  display.setCursor(0, 54);
  print2Digits(now.hour());
  display.print(':');
  print2Digits(now.minute());

  display.display();
}

// ---------------- Feeding logic ----------------
void confirmFeeding(const DateTime& now) {
  lastFedUnix = now.unixtime();
  storedMealSlot = currentMealSlot;

  writeUint32ToEEPROM(EEPROM_ADDR_UNIX, lastFedUnix);
  EEPROM.update(EEPROM_ADDR_SLOT, storedMealSlot);
}

void updateFeedState(const DateTime& now) {
  currentMealSlot = getMealSlot(now);

  bool due = isMealDueNow(now);
  bool fed = (storedMealSlot == currentMealSlot);

  if (!due) feedState = STATE_WAITING;
  else if (fed) feedState = STATE_FED;
  else feedState = STATE_DUE;
}

void updateOutputs(const DateTime& now) {
  switch (feedState) {
    case STATE_WAITING:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, LOW);
      showWaitingScreen(now);
      break;

    case STATE_DUE: {
      digitalWrite(BLUE_LED_PIN, LOW);

      unsigned long interval = getBlinkIntervalMs(now);

      if (interval == 0) {
        digitalWrite(RED_LED_PIN, HIGH);
      } else {
        if (millis() - lastBlinkMillis >= interval) {
          lastBlinkMillis = millis();
          blinkState = !blinkState;
        }
        digitalWrite(RED_LED_PIN, blinkState ? HIGH : LOW);
      }

      showDueScreen(now);
      break;
    }

    case STATE_FED: {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);

      uint32_t elapsed = 0;
      uint32_t nowUnix = now.unixtime();
      if (nowUnix >= lastFedUnix) {
        elapsed = nowUnix - lastFedUnix;
      }

      showFedScreen(elapsed, now);
      break;
    }
  }
}

// ---------------- Setup ----------------
void setup() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1) {
      // OLED init failed
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  if (!rtc.begin()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("RTC no detectado");
    display.display();
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lastFedUnix = readUint32FromEEPROM(EEPROM_ADDR_UNIX);
  storedMealSlot = EEPROM.read(EEPROM_ADDR_SLOT);

  delay(800);
}

// ---------------- Loop ----------------
void loop() {
  DateTime now = rtc.now();

  updateFeedState(now);

  if (feedState == STATE_DUE && buttonPressedEvent()) {
    confirmFeeding(now);
    updateFeedState(now);
  }

  updateOutputs(now);

  delay(50);
}
