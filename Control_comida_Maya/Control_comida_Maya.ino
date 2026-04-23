/*
 * Control de comidas - Maya
 * Hardware: Arduino Nano, SSD1306 128x64, DS3231, rotary encoder + button, 2 LEDs
 *
 * Encoder pins:  CLK=2  DT=3  SW=4
 * LEDs:          RED=5  BLUE=6
 *
 * Menu access: turn knob one full turn (20 steps CW) — bar fills the screen.
 * Turn CCW to empty it. When full → enter menu.
 * Menu closes after 30 s idle or via Exit.
 */

#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
RTC_DS3231 rtc;

// ── Pins ──────────────────────────────────────────────────────────────
#define PIN_RED   5
#define PIN_BLUE  6
#define PIN_SW    4
#define PIN_CLK   2
#define PIN_DT    3

// ── EEPROM layout ─────────────────────────────────────────────────────
#define EE_FED    0   // uint32 (4 bytes) — unix timestamp last fed
#define EE_SLOT   4   // uint8  — meal slot when last fed
#define EE_DES_H  5
#define EE_DES_M  6
#define EE_CEN_H  7
#define EE_CEN_M  8

// ── Runtime config ────────────────────────────────────────────────────
uint8_t desH, desM;   // desayuno
uint8_t cenH, cenM;   // cena

uint32_t lastFedUnix;
uint8_t  storedSlot, curSlot;

// ── App states ────────────────────────────────────────────────────────
enum State : uint8_t {
  S_NORMAL,
  S_MENU,
  S_SET_RTC_H, S_SET_RTC_M,
  S_SET_DES_H, S_SET_DES_M,
  S_SET_CEN_H, S_SET_CEN_M
};
State appState = S_NORMAL;

enum Feed : uint8_t { F_WAIT, F_DUE, F_FED };
Feed feedState = F_WAIT;

// ── Encoder ───────────────────────────────────────────────────────────
uint8_t lastClk = HIGH;

// ── Button debounce ───────────────────────────────────────────────────
bool btnStable = HIGH, btnLast = HIGH;
unsigned long btnTime = 0;

// ── Menu ──────────────────────────────────────────────────────────────
uint8_t menuSel = 0;
unsigned long menuActivity = 0;
#define MENU_TIMEOUT 30000UL

// ── Menu-access bar ───────────────────────────────────────────────────
int8_t  barVal = 0;
#define BAR_MAX 20
unsigned long lastEncTime = 0;

// ── Blink ─────────────────────────────────────────────────────────────
unsigned long blinkMs = 0;
bool blinkOn = false;

// ── Temp edit values ──────────────────────────────────────────────────
uint8_t editH, editM;

// ═══════════════════════════════════════════════════════════════════════
//  EEPROM helpers
// ═══════════════════════════════════════════════════════════════════════
void writeU32(uint8_t addr, uint32_t v) {
  for (uint8_t i = 0; i < 4; i++)
    EEPROM.update(addr + i, (v >> (8 * i)) & 0xFF);
}
uint32_t readU32(uint8_t addr) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; i++)
    v |= (uint32_t)EEPROM.read(addr + i) << (8 * i);
  return v;
}

// ═══════════════════════════════════════════════════════════════════════
//  Encoder & button
// ═══════════════════════════════════════════════════════════════════════
int8_t pollEnc() {
  uint8_t clk = digitalRead(PIN_CLK);
  int8_t  dir = 0;
  if (lastClk == HIGH && clk == LOW)
    dir = (digitalRead(PIN_DT) == HIGH) ? 1 : -1;
  lastClk = clk;
  return dir;
}

bool pollBtn() {
  bool r = digitalRead(PIN_SW);
  if (r != btnLast) btnTime = millis();
  btnLast = r;
  if (millis() - btnTime > 40 && r != btnStable) {
    btnStable = r;
    return r == LOW;
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════
//  Meal logic
// ═══════════════════════════════════════════════════════════════════════
void p2(uint8_t v) { if (v < 10) display.print('0'); display.print(v); }

uint8_t mealSlot(const DateTime& t) {
  uint16_t d = (uint16_t)(t.year() % 100) * 372 + t.month() * 31 + t.day();
  return ((uint16_t)t.hour() * 60 + t.minute() >= (uint16_t)cenH * 60 + cenM)
         ? (uint8_t)((d * 2) & 0xFF)
         : (uint8_t)(((d * 2) - 1) & 0xFF);
}

bool mealDue(const DateTime& t) {
  return (uint16_t)t.hour() * 60 + t.minute() >= (uint16_t)desH * 60 + desM;
}

// Returns blink period in ms; 0 = solid on (first 30 min)
unsigned long blinkIv(const DateTime& t) {
  uint16_t now  = (uint16_t)t.hour() * 60 + t.minute();
  uint16_t base = (now >= (uint16_t)cenH * 60 + cenM)
                  ? (uint16_t)cenH * 60 + cenM
                  : (uint16_t)desH * 60 + desM;
  uint16_t el   = now - base;
  if (el >= 90) return 300;
  if (el >= 60) return 600;
  if (el >= 30) return 1200;
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  Draw routines
// ═══════════════════════════════════════════════════════════════════════

void drawClock(const DateTime& t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Control Maya"));
  display.setTextSize(2);
  display.setCursor(8, 18);
  p2(t.hour()); display.print(':'); p2(t.minute()); display.print(':'); p2(t.second());
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(F("D:")); p2(desH); display.print(':'); p2(desM);
  display.print(F("  C:")); p2(cenH); display.print(':'); p2(cenM);
  display.display();
}

void drawDue(const DateTime& t) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(8, 14);
  display.print(F("Hora de"));
  display.setCursor(8, 34);
  display.print(F("comer!"));
  display.setTextSize(1);
  display.setCursor(90, 56);
  p2(t.hour()); display.print(':'); p2(t.minute());
  display.display();
}

void drawFed(uint32_t sec, const DateTime& t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Maya comio hace:"));
  display.setTextSize(2);
  display.setCursor(0, 14);
  uint8_t h = (uint8_t)(sec / 3600UL);
  uint8_t m = (uint8_t)((sec % 3600UL) / 60UL);
  uint8_t s = (uint8_t)(sec % 60UL);
  if (h > 0) { display.print(h); display.print('h'); display.print(m); display.print('m'); }
  else        { display.print(m); display.print('m'); display.print(s); display.print('s'); }
  display.setTextSize(1);
  display.setCursor(90, 56);
  p2(t.hour()); display.print(':'); p2(t.minute());
  display.display();
}

void drawBar() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 22);
  display.print(F("Accesando"));
  display.setCursor(40, 34);
  display.print(F("menu..."));
  display.drawRect(4, 48, 120, 12, SSD1306_WHITE);
  uint8_t fill = (uint8_t)((uint16_t)barVal * 116 / BAR_MAX);
  if (fill > 0) display.fillRect(6, 50, fill, 8, SSD1306_WHITE);
  display.display();
}

void drawMenu(const DateTime& t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(88, 0);
  p2(t.hour()); display.print(':'); p2(t.minute());
  const char* items[4] = { "Reloj", "Desayuno", "Cena", "Exit" };
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t y = 10 + i * 13;
    if (i == menuSel) {
      display.fillRect(0, y, 80, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y + 2);
    display.print(items[i]);
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void drawSetTime(const __FlashStringHelper* lbl, uint8_t h, uint8_t m, bool editH) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(lbl);
  display.setTextSize(2);
  // Hours field
  if (editH) {
    display.fillRect(8, 16, 34, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(10, 18); p2(h);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(44, 18); display.print(':');
  // Minutes field
  if (!editH) {
    display.fillRect(60, 16, 34, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(62, 18); p2(m);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(F("Girar=cambiar  Clic=OK"));
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  Setup
// ═══════════════════════════════════════════════════════════════════════
void setup() {
  pinMode(PIN_RED,  OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_SW,   INPUT_PULLUP);
  pinMode(PIN_CLK,  INPUT_PULLUP);
  pinMode(PIN_DT,   INPUT_PULLUP);
  digitalWrite(PIN_RED,  LOW);
  digitalWrite(PIN_BLUE, LOW);

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Iniciando..."));
  display.display();

  if (!rtc.begin()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Sin RTC!"));
    display.display();
    while (1);
  }
  if (rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lastFedUnix = readU32(EE_FED);
  storedSlot  = EEPROM.read(EE_SLOT);
  desH = EEPROM.read(EE_DES_H);
  desM = EEPROM.read(EE_DES_M);
  cenH = EEPROM.read(EE_CEN_H);
  cenM = EEPROM.read(EE_CEN_M);

  // Defaults on fresh/corrupt EEPROM
  if (desH > 23) { desH = 8;  desM = 0; }
  if (desM > 59)   desM = 0;
  if (cenH > 23) { cenH = 19; cenM = 0; }
  if (cenM > 59)   cenM = 0;

  delay(600);
}

// ═══════════════════════════════════════════════════════════════════════
//  Loop
// ═══════════════════════════════════════════════════════════════════════
void loop() {
  DateTime now = rtc.now();
  int8_t   enc = pollEnc();
  bool     btn = pollBtn();

  switch (appState) {

    // ── Normal operation ──────────────────────────────────────────────
    case S_NORMAL: {
      if (enc != 0) {
        barVal += enc;
        if (barVal < 0) barVal = 0;
        lastEncTime = millis();
        if (barVal >= BAR_MAX) {
          barVal = 0;
          appState     = S_MENU;
          menuSel      = 0;
          menuActivity = millis();
        } else {
          drawBar();
        }
        break;
      }
      // Auto-clear bar after 2 s of no encoder input
      if (barVal > 0 && millis() - lastEncTime > 2000) barVal = 0;

      // Feed state machine
      curSlot = mealSlot(now);
      bool due = mealDue(now);
      bool fed = (storedSlot == curSlot);
      feedState = !due ? F_WAIT : (fed ? F_FED : F_DUE);

      if (feedState == F_DUE && btn) {
        lastFedUnix = now.unixtime();
        storedSlot  = curSlot;
        writeU32(EE_FED, lastFedUnix);
        EEPROM.update(EE_SLOT, storedSlot);
        feedState = F_FED;
      }

      switch (feedState) {
        case F_WAIT:
          digitalWrite(PIN_RED,  LOW);
          digitalWrite(PIN_BLUE, LOW);
          drawClock(now);
          break;

        case F_DUE: {
          digitalWrite(PIN_BLUE, LOW);
          unsigned long iv = blinkIv(now);
          if (iv == 0) {
            digitalWrite(PIN_RED, HIGH);
          } else {
            if (millis() - blinkMs >= iv) { blinkMs = millis(); blinkOn = !blinkOn; }
            digitalWrite(PIN_RED, blinkOn ? HIGH : LOW);
          }
          drawDue(now);
          break;
        }

        case F_FED: {
          digitalWrite(PIN_RED,  LOW);
          digitalWrite(PIN_BLUE, HIGH);
          uint32_t u  = now.unixtime();
          uint32_t el = (u >= lastFedUnix) ? u - lastFedUnix : 0;
          drawFed(el, now);
          break;
        }
      }
      break;
    }

    // ── Main menu ─────────────────────────────────────────────────────
    case S_MENU: {
      if (millis() - menuActivity > MENU_TIMEOUT) { appState = S_NORMAL; break; }
      if (enc != 0) { menuSel = (menuSel + enc + 4) % 4; menuActivity = millis(); }
      if (btn) {
        menuActivity = millis();
        switch (menuSel) {
          case 0: { DateTime t = rtc.now(); editH = t.hour(); editM = t.minute();
                    appState = S_SET_RTC_H; break; }
          case 1: editH = desH; editM = desM; appState = S_SET_DES_H; break;
          case 2: editH = cenH; editM = cenM; appState = S_SET_CEN_H; break;
          case 3: appState = S_NORMAL; break;
        }
        break;
      }
      drawMenu(now);
      break;
    }

    // ── Set clock ─────────────────────────────────────────────────────
    case S_SET_RTC_H:
      if (enc) editH = (uint8_t)(editH + enc + 24) % 24;
      if (btn) appState = S_SET_RTC_M;
      drawSetTime(F("Reloj"), editH, editM, true);
      break;

    case S_SET_RTC_M:
      if (enc) editM = (uint8_t)(editM + enc + 60) % 60;
      if (btn) {
        DateTime t = rtc.now();
        rtc.adjust(DateTime(t.year(), t.month(), t.day(), editH, editM, 0));
        appState = S_MENU; menuActivity = millis();
      }
      drawSetTime(F("Reloj"), editH, editM, false);
      break;

    // ── Set desayuno ──────────────────────────────────────────────────
    case S_SET_DES_H:
      if (enc) editH = (uint8_t)(editH + enc + 24) % 24;
      if (btn) appState = S_SET_DES_M;
      drawSetTime(F("Desayuno"), editH, editM, true);
      break;

    case S_SET_DES_M:
      if (enc) editM = (uint8_t)(editM + enc + 60) % 60;
      if (btn) {
        desH = editH; desM = editM;
        EEPROM.update(EE_DES_H, desH); EEPROM.update(EE_DES_M, desM);
        appState = S_MENU; menuActivity = millis();
      }
      drawSetTime(F("Desayuno"), editH, editM, false);
      break;

    // ── Set cena ──────────────────────────────────────────────────────
    case S_SET_CEN_H:
      if (enc) editH = (uint8_t)(editH + enc + 24) % 24;
      if (btn) appState = S_SET_CEN_M;
      drawSetTime(F("Cena"), editH, editM, true);
      break;

    case S_SET_CEN_M:
      if (enc) editM = (uint8_t)(editM + enc + 60) % 60;
      if (btn) {
        cenH = editH; cenM = editM;
        EEPROM.update(EE_CEN_H, cenH); EEPROM.update(EE_CEN_M, cenM);
        appState = S_MENU; menuActivity = millis();
      }
      drawSetTime(F("Cena"), editH, editM, false);
      break;
  }
}
