/*
 * Reloj Arduino
 * Reloj con termómetro, higrómetro y alarma que se apaga por movimiento.
 *
 * Pantalla principal   Info alarma (LOOK)   Entrada a ajustes    Ajuste fecha/hora    Ajuste alarma
 * +----------------+   +----------------+   +----------------+   +----------------+   +----------------+
 * |HH:MM DD/MM/YYYY|   |HH:MM:SS| HH:MM |   |------SET------ |   |    >HH :>MM    |   |SET  ALARM TIME |
 * |Temp:27c Hum:74%|   |DD/MM/YYYY| ON  |   |-TIME and DATE- |   |>DD />MM />YYYY |   |    >HH :>MM    |
 * +----------------+   +----------------+   +----------------+   +----------------+   +----------------+
 *
 * Botones (a GND, con pull-up interno):
 *   SET   -> recorre los pasos de ajuste: hora, minutos, día, mes, año,
 *            hora de alarma, minutos de alarma; la última pulsación guarda.
 *   UP    -> incrementa el valor en ajuste.
 *   DOWN  -> decrementa el valor en ajuste / prende-apaga la luz de fondo.
 *   ALARM -> arma o desarma la alarma; si está sonando, la silencia.
 *   LOOK  -> muestra un segundo la pantalla de información de la alarma.
 */

#include <Wire.h>
#include <LCD.h>                // New LiquidCrystal (fmalpartida)
#include <LiquidCrystal_I2C.h>  // New LiquidCrystal (fmalpartida)
#include <dht.h>                // DHTlib (Rob Tillaart)
#include <virtuabotixRTC.h>     // Módulo RTC DS1302
#include <EEPROM.h>

// --- Pines ---
const uint8_t PIN_BTN_LOOK  = 4;
const uint8_t PIN_DHT11     = 5;
const uint8_t PIN_RTC_SCLK  = 6;
const uint8_t PIN_RTC_IO    = 7;
const uint8_t PIN_RTC_CE    = 8;
const uint8_t PIN_BTN_SET   = 9;
const uint8_t PIN_BTN_UP    = 10;
const uint8_t PIN_BTN_DOWN  = 11;
const uint8_t PIN_BTN_ALARM = 12;
const uint8_t PIN_BUZZER    = 13;
const uint8_t PIN_SHAKE     = A3;  // Sensor de inclinación SW-520D

// --- Configuración ---
const uint8_t       LCD_I2C_ADDR          = 0x27;
const unsigned long DHT_READ_INTERVAL_MS  = 6000;             // El DHT11 necesita >= 1 s entre lecturas
const unsigned long MELODY_STEP_MS        = 300;              // Tiempo entre notas de la alarma
const unsigned long ALARM_MAX_RING_MS     = 5UL * 60 * 1000;  // La alarma se apaga sola a los 5 minutos
const int           SHAKE_THRESHOLD       = 200;              // Lectura analógica que cuenta como movimiento
const uint8_t       SHAKES_TO_STOP        = 6;                // Movimientos necesarios para apagar la alarma
const int           MIN_YEAR              = 2000;             // El DS1302 solo guarda años 2000-2099
const int           MAX_YEAR              = 2099;
const int           MELODY[]              = {600, 800, 1000, 1200};
const uint8_t       MELODY_LEN            = sizeof(MELODY) / sizeof(MELODY[0]);

// Direcciones de EEPROM donde se guarda la hora de la alarma
const int EEPROM_ALARM_HOUR   = 0;
const int EEPROM_ALARM_MINUTE = 1;

// Pasos del modo ajuste (0 = funcionamiento normal)
enum SetupStep {
  STEP_NONE = 0,
  STEP_HOUR,
  STEP_MINUTE,
  STEP_DAY,
  STEP_MONTH,
  STEP_YEAR,
  STEP_ALARM_HOUR,
  STEP_ALARM_MINUTE
};

// LCD por I2C: dirección, En, Rw, Rs, D4, D5, D6, D7, luz de fondo, polaridad
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
dht DHT;
virtuabotixRTC rtcModule(PIN_RTC_SCLK, PIN_RTC_IO, PIN_RTC_CE);  // La librería ya define un global "rtc"

// --- Estado ---
int day, month, year, hour, minute, second;
int temperature = 0, humidity = 0;
int alarmHour, alarmMinute;

uint8_t setupStep = STEP_NONE;
bool timeEdited   = false;  // Solo se escribe el RTC si el usuario cambió fecha u hora

bool backlightOn   = true;
bool alarmArmed    = false;
bool alarmRinging  = false;
bool alarmFired    = false;  // Evita que la alarma vuelva a sonar dentro del mismo minuto
unsigned long alarmStartMs  = 0;
unsigned long lastNoteMs    = 0;
uint8_t       melodyIndex   = 0;
uint8_t       shakeCount    = 0;
bool          lastShakeHigh = false;

unsigned long lastDhtReadMs = 0;

// Lectura de los botones en la vuelta actual del loop (LOW = presionado)
bool btnSet, btnUp, btnDown, btnAlarm, btnLook;

void setup() {
  // Para poner en hora un RTC nuevo se puede usar el menú SET, o descomentar
  // esta línea una sola vez: (segundos, minutos, hora, díaSemana 1-7, día, mes, año)
  // rtcModule.setDS1302Time(0, 5, 22, 3, 8, 3, 2022);

  pinMode(PIN_BTN_SET, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_LOOK, INPUT_PULLUP);
  pinMode(PIN_BTN_ALARM, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  // Una EEPROM sin usar devuelve 255, por eso se valida
  alarmHour   = EEPROM.read(EEPROM_ALARM_HOUR);
  alarmMinute = EEPROM.read(EEPROM_ALARM_MINUTE);
  if (alarmHour > 23)   alarmHour = 0;
  if (alarmMinute > 59) alarmMinute = 0;

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();

  readTempHum();
}

void loop() {
  readButtons();

  if (millis() - lastDhtReadMs >= DHT_READ_INTERVAL_MS) {
    readTempHum();
  }

  if (setupStep == STEP_NONE) {
    readRtc();
    printMainScreen();
    updateAlarm();
  } else {
    handleSetup();
  }
}

// ---------------------------------------------------------------------------
// Botones
// ---------------------------------------------------------------------------

void readButtons() {
  btnSet   = digitalRead(PIN_BTN_SET) == LOW;
  btnUp    = digitalRead(PIN_BTN_UP) == LOW;
  btnDown  = digitalRead(PIN_BTN_DOWN) == LOW;
  btnAlarm = digitalRead(PIN_BTN_ALARM) == LOW;
  btnLook  = digitalRead(PIN_BTN_LOOK) == LOW;

  if (btnAlarm) {
    if (alarmRinging) {
      stopRinging();  // Silencia sin desarmar: mañana vuelve a sonar
    } else {
      alarmArmed = !alarmArmed;
    }
    delay(500);
  }

  if (btnLook) {
    lcd.clear();
    printAlarmInfoScreen();
    delay(1000);
    lcd.clear();
  }

  if (btnDown && setupStep == STEP_NONE) {
    backlightOn = !backlightOn;
    if (backlightOn) lcd.backlight(); else lcd.noBacklight();
    delay(500);
  }

  if (btnSet) {
    if (setupStep < STEP_ALARM_MINUTE) {
      if (setupStep == STEP_NONE) {
        stopRinging();
        timeEdited = false;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("------SET------");
        lcd.setCursor(0, 1);
        lcd.print("-TIME and DATE-");
        delay(2000);
        lcd.clear();
      }
      setupStep++;
    } else {
      saveSettings();
      setupStep = STEP_NONE;
    }
    delay(500);
  }
}

// ---------------------------------------------------------------------------
// Sensores
// ---------------------------------------------------------------------------

void readTempHum() {
  lastDhtReadMs = millis();
  // Si la lectura falla se conservan los últimos valores válidos
  if (DHT.read11(PIN_DHT11) == DHTLIB_OK) {
    humidity    = DHT.humidity;
    temperature = DHT.temperature;
  }
}

void readRtc() {
  rtcModule.updateTime();
  day    = rtcModule.dayofmonth;
  month  = rtcModule.month;
  year   = rtcModule.year;
  hour   = rtcModule.hours;
  minute = rtcModule.minutes;
  second = rtcModule.seconds;
}

// ---------------------------------------------------------------------------
// Pantallas
// ---------------------------------------------------------------------------

void printLine(uint8_t row, const char *text) {
  lcd.setCursor(0, row);
  lcd.print(text);
}

void printMainScreen() {
  char line[17];
  snprintf(line, sizeof(line), "%02d:%02d %02d/%02d/%04d", hour, minute, day, month, year);
  printLine(0, line);
  snprintf(line, sizeof(line), "Temp:%2dc Hum:%2d%%", temperature, humidity);
  printLine(1, line);
}

void printAlarmInfoScreen() {
  char line[17];
  snprintf(line, sizeof(line), "%02d:%02d:%02d| %02d:%02d", hour, minute, second, alarmHour, alarmMinute);
  printLine(0, line);
  snprintf(line, sizeof(line), "%02d/%02d/%04d| %-3s", day, month, year, alarmArmed ? "ON" : "OFF");
  printLine(1, line);
}

// Línea "    >HH :>MM    " con el cursor '>' delante del campo que se está editando
void printHourMinuteLine(uint8_t row, int h, int m, bool markHour, bool markMinute) {
  char line[17];
  snprintf(line, sizeof(line), "    %c%02d :%c%02d    ",
           markHour ? '>' : ' ', h, markMinute ? '>' : ' ', m);
  printLine(row, line);
}

void printSetupScreen() {
  if (setupStep <= STEP_YEAR) {
    char line[17];
    printHourMinuteLine(0, hour, minute, setupStep == STEP_HOUR, setupStep == STEP_MINUTE);
    snprintf(line, sizeof(line), "%c%02d /%c%02d /%c%04d ",
             setupStep == STEP_DAY ? '>' : ' ', day,
             setupStep == STEP_MONTH ? '>' : ' ', month,
             setupStep == STEP_YEAR ? '>' : ' ', year);
    printLine(1, line);
  } else {
    printLine(0, "SET  ALARM TIME ");
    printHourMinuteLine(1, alarmHour, alarmMinute,
                        setupStep == STEP_ALARM_HOUR, setupStep == STEP_ALARM_MINUTE);
  }
}

// ---------------------------------------------------------------------------
// Modo ajuste
// ---------------------------------------------------------------------------

// Suma delta a value dando la vuelta dentro de [minValue, maxValue]
int wrap(int value, int delta, int minValue, int maxValue) {
  value += delta;
  if (value > maxValue) return minValue;
  if (value < minValue) return maxValue;
  return value;
}

bool isLeapYear(int y) {
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int daysInMonth(int m, int y) {
  static const uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeapYear(y)) ? 29 : DAYS[m - 1];
}

// Día de la semana 1-7 (1 = domingo), algoritmo de Sakamoto
int dayOfWeek(int d, int m, int y) {
  static const uint8_t T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7 + 1;
}

void handleSetup() {
  int delta = (btnUp ? 1 : 0) - (btnDown ? 1 : 0);

  if (delta != 0) {
    switch (setupStep) {
      case STEP_HOUR:         hour        = wrap(hour, delta, 0, 23); break;
      case STEP_MINUTE:       minute      = wrap(minute, delta, 0, 59); break;
      case STEP_DAY:          day         = wrap(day, delta, 1, daysInMonth(month, year)); break;
      case STEP_MONTH:        month       = wrap(month, delta, 1, 12); break;
      case STEP_YEAR:         year        = wrap(year, delta, MIN_YEAR, MAX_YEAR); break;
      case STEP_ALARM_HOUR:   alarmHour   = wrap(alarmHour, delta, 0, 23); break;
      case STEP_ALARM_MINUTE: alarmMinute = wrap(alarmMinute, delta, 0, 59); break;
    }
    if (setupStep <= STEP_YEAR) {
      timeEdited = true;
      // Corrige días inválidos al cambiar mes o año (ej.: 31/04 -> 30/04)
      day = min(day, daysInMonth(month, year));
    }
    delay(350);
  }

  printSetupScreen();
}

void saveSettings() {
  lcd.clear();
  lcd.print("Saving....");

  // Si solo se ajustó la alarma no se toca el RTC: reescribirlo con la hora
  // leída al entrar al menú haría que el reloj se atrase.
  if (timeEdited) {
    rtcModule.setDS1302Time(0, minute, hour, dayOfWeek(day, month, year), day, month, year);
  }
  EEPROM.update(EEPROM_ALARM_HOUR, alarmHour);
  EEPROM.update(EEPROM_ALARM_MINUTE, alarmMinute);

  delay(2000);
  lcd.clear();
}

// ---------------------------------------------------------------------------
// Alarma
// ---------------------------------------------------------------------------

void startRinging() {
  alarmRinging  = true;
  alarmStartMs  = millis();
  lastNoteMs    = 0;
  melodyIndex   = 0;
  shakeCount    = 0;
  lastShakeHigh = false;
}

void stopRinging() {
  alarmRinging = false;
  noTone(PIN_BUZZER);
}

void updateAlarm() {
  bool isAlarmMinute = hour == alarmHour && minute == alarmMinute;
  if (!isAlarmMinute) {
    alarmFired = false;
  } else if (alarmArmed && !alarmFired) {
    alarmFired = true;
    startRinging();
  }

  if (!alarmArmed && alarmRinging) {
    stopRinging();
  }
  if (!alarmRinging) {
    return;
  }

  // Se cuenta cada vez que el sensor pasa de reposo a movimiento
  bool shakeHigh = analogRead(PIN_SHAKE) > SHAKE_THRESHOLD;
  if (shakeHigh && !lastShakeHigh) {
    shakeCount++;
  }
  lastShakeHigh = shakeHigh;

  unsigned long now = millis();
  if (shakeCount >= SHAKES_TO_STOP || now - alarmStartMs >= ALARM_MAX_RING_MS) {
    stopRinging();
    return;
  }

  if (now - lastNoteMs >= MELODY_STEP_MS) {
    lastNoteMs = now;
    tone(PIN_BUZZER, MELODY[melodyIndex], 100);
    melodyIndex = (melodyIndex + 1) % MELODY_LEN;
  }
}
