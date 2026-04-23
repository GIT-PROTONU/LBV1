#include <HX711_ADC.h>
#include <EEPROM.h>

#define VERSION "1.3"

// Pin definitions
static const uint8_t RATE_PIN     = 2;
static const uint8_t LED_PINS[3]  = {3, 5, 6};
static const uint8_t DOUT_PINS[4] = {A2, 4, A5, 10};
static const uint8_t SCK_PINS[4]  = {A3, 7, A4,  9};

// EEPROM: one float (4 bytes) per channel, starting at offset 0
static const int   EEPROM_BASE  = 0;
static const float DEFAULT_CAL  = 14000.0f;
static const float CAL_STEP     = 10.0f;

HX711_ADC lc[4] = {
  HX711_ADC(DOUT_PINS[0], SCK_PINS[0]),
  HX711_ADC(DOUT_PINS[1], SCK_PINS[1]),
  HX711_ADC(DOUT_PINS[2], SCK_PINS[2]),
  HX711_ADC(DOUT_PINS[3], SCK_PINS[3]),
};

float calFactor[4];
bool diagMode = false;

static void loadCal() {
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.get(EEPROM_BASE + i * 4, calFactor[i]);
    if (isnan(calFactor[i]) || calFactor[i] == 0.0f ||
        calFactor[i] > 1e6f  || calFactor[i] < -1e6f) {
      calFactor[i] = DEFAULT_CAL;
    }
  }
}

static void saveCal(uint8_t ch) {
  EEPROM.put(EEPROM_BASE + ch * 4, calFactor[ch]);
}

static void printCal() {
  // Send cal factors as integers (×10 for 1 decimal place precision)
  char buf[48];
  int len = snprintf(buf, sizeof(buf), "CAL:%ld|%ld|%ld|%ld$\n",
    (long)(calFactor[0] * 10),
    (long)(calFactor[1] * 10),
    (long)(calFactor[2] * 10),
    (long)(calFactor[3] * 10));
  Serial.write((uint8_t*)buf, len);
}

static void adjustCal(uint8_t ch, float delta) {
  calFactor[ch] += delta;
  lc[ch].setCalFactor(calFactor[ch]);
  saveCal(ch);
  printCal();
}

static void printDiag() {
  char buf[48];
  int len = snprintf(buf, sizeof(buf), "RAW:%ld|%ld|%ld|%ld$\n",
    (long)(lc[0].getData() * calFactor[0]),
    (long)(lc[1].getData() * calFactor[1]),
    (long)(lc[2].getData() * calFactor[2]),
    (long)(lc[3].getData() * calFactor[3]));
  Serial.write((uint8_t*)buf, len);
}

void setup() {
  pinMode(RATE_PIN, OUTPUT);
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], HIGH);
  }
  delay(1000);
  for (uint8_t i = 0; i < 3; i++) digitalWrite(LED_PINS[i], LOW);
  digitalWrite(RATE_PIN, HIGH);  // 80 SPS

  Serial.begin(115200);
  Serial.println("LBV1 v" VERSION);

  loadCal();

  for (uint8_t i = 0; i < 4; i++) lc[i].begin();

  byte rdy[4] = {};
  uint8_t readyCount = 0;
  while (readyCount < 4) {
    readyCount = 0;
    for (uint8_t i = 0; i < 4; i++) {
      if (!rdy[i]) rdy[i] = lc[i].startMultiple(2000, true);
      readyCount += rdy[i];
    }
  }

  for (uint8_t i = 0; i < 4; i++) lc[i].setCalFactor(calFactor[i]);

  printCal();
}

void loop() {
  // Per-channel ready flags — only output when all 4 have fresh data
  static bool ch_rdy[4] = {};

  for (uint8_t i = 0; i < 4; i++) {
    if (lc[i].update()) ch_rdy[i] = true;
  }

  if (ch_rdy[0] & ch_rdy[1] & ch_rdy[2] & ch_rdy[3]) {
    if (diagMode) {
      printDiag();
    } else {
      // Integer output ×100 (2 dp precision, no float-to-string overhead)
      char buf[48];
      int len = snprintf(buf, sizeof(buf), "%ld|%ld|%ld|%ld$\n",
        -(long)(lc[0].getData() * 100.0f),
        -(long)(lc[1].getData() * 100.0f),
        -(long)(lc[2].getData() * 100.0f),
        -(long)(lc[3].getData() * 100.0f));
      Serial.write((uint8_t*)buf, len);
    }
    ch_rdy[0] = ch_rdy[1] = ch_rdy[2] = ch_rdy[3] = false;
  }

  if (Serial.available() > 0) {
    switch (Serial.read()) {
      case 't':
        for (uint8_t i = 0; i < 4; i++) lc[i].tareNoDelay();
        break;
      case 'p':
        diagMode = !diagMode;
        Serial.print("mode="); Serial.println(diagMode ? "DIAG" : "NORMAL");
        break;
      case 'c': printCal(); break;
      case 'q': adjustCal(0, +CAL_STEP); break;
      case 'a': adjustCal(0, -CAL_STEP); break;
      case 'w': adjustCal(1, +CAL_STEP); break;
      case 's': adjustCal(1, -CAL_STEP); break;
      case 'e': adjustCal(2, +CAL_STEP); break;
      case 'd': adjustCal(2, -CAL_STEP); break;
      case 'r': adjustCal(3, +CAL_STEP); break;
      case 'f': adjustCal(3, -CAL_STEP); break;
    }
  }
}
