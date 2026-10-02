/*
 * Reloj Arduino
 * Reloj con termómetro, higrómetro y alarma que se apaga por movimiento.
 *
 * Pantalla principal   Info (LOOK)          Ajuste fecha/hora    Ajuste alarma        Días de alarma
 * +----------------+   +----------------+   +----------------+   +----------------+   +----------------+
 * |HH:MM DD/MM/YYYY|   |Vie HH:MM:SS    |   |    >HH : MM    |   |AJUSTE ALARMA   |   |DIAS DE ALARMA  |
 * |T 27°C  D 74%  A|   |A HH:MM ON  L-V |   | DD / MM / YYYY |   |    >HH : MM    |   |  >Lun a Vie    |
 * +----------------+   +----------------+   +----------------+   +----------------+   +----------------+
 * (T, D y A son íconos propios: termómetro, gota y campana)
 *
 * Botones (a GND, con pull-up interno):
 *   SET        -> entra al ajuste y pasa al siguiente campo; en el último, guarda.
 *   UP / DOWN  -> cambian el valor en ajuste (mantener apretado = avance rápido).
 *   DOWN       -> fuera del ajuste, prende o apaga la luz de fondo.
 *   ALARM      -> arma o desarma la alarma (fuera del ajuste); sonando la pospone;
 *                 pospuesta la cancela.
 *   LOOK       -> clic: muestra la pantalla de info; mantener: ajuste directo de la alarma.
 *
 * Sacudir el reloj mientras suena apaga la alarma. Sin tocar botones durante
 * 30 s, el ajuste se cancela sin guardar. El reloj y la alarma siguen andando
 * mientras se está en el ajuste: los valores se editan sobre una copia.
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
const uint8_t       LCD_I2C_ADDR         = 0x27;
const unsigned long DHT_READ_INTERVAL_MS = 6000;             // El DHT11 necesita >= 1 s entre lecturas
const uint8_t       DHT_MAX_FAILURES     = 3;                // Lecturas fallidas seguidas antes de mostrar "--"
const unsigned long DEBOUNCE_MS          = 30;
const unsigned long LONG_PRESS_MS        = 1000;
const unsigned long REPEAT_DELAY_MS      = 500;              // Mantener UP/DOWN: espera antes de repetir...
const unsigned long REPEAT_INTERVAL_MS   = 120;              // ...y cada cuánto repite
const unsigned long INFO_SCREEN_MS       = 3000;
const unsigned long SETUP_TIMEOUT_MS     = 30000;            // Sin actividad, el ajuste se cancela
const unsigned long MELODY_STEP_MS       = 300;              // Tiempo entre notas de la alarma
const unsigned long ALARM_MAX_RING_MS    = 5UL * 60 * 1000;  // La alarma se apaga sola a los 5 minutos
const unsigned long SNOOZE_MS            = 5UL * 60 * 1000;  // Tiempo que se pospone con el botón ALARM
const int           SHAKE_THRESHOLD      = 200;              // Lectura analógica que cuenta como movimiento
const unsigned long SHAKE_DEBOUNCE_MS    = 150;              // El sensor rebota: ignora flancos muy seguidos
const uint8_t       SHAKES_TO_STOP       = 6;                // Movimientos necesarios para apagar la alarma
const int           MIN_YEAR             = 2000;             // El DS1302 solo guarda años 2000-2099
const int           MAX_YEAR             = 2099;
const int           MELODY[]             = {600, 800, 1000, 1200};
const uint8_t       MELODY_LEN           = sizeof(MELODY) / sizeof(MELODY[0]);

// Direcciones de EEPROM
const int EEPROM_ALARM_HOUR   = 0;
const int EEPROM_ALARM_MINUTE = 1;
const int EEPROM_ALARM_ARMED  = 2;
const int EEPROM_ALARM_DAYS   = 3;

// Pasos del modo ajuste (0 = funcionamiento normal)
enum SetupStep {
  STEP_NONE = 0,
  STEP_HOUR,
  STEP_MINUTE,
  STEP_DAY,
  STEP_MONTH,
  STEP_YEAR,
  STEP_ALARM_HOUR,
  STEP_ALARM_MINUTE,
  STEP_ALARM_DAYS
};

enum AlarmDays { ALARM_EVERY_DAY = 0, ALARM_WEEKDAYS, ALARM_WEEKENDS, ALARM_DAYS_COUNT };
const char *const ALARM_DAYS_LONG[]  = {"Todos los dias", "Lun a Vie", "Sab y Dom"};
const char *const ALARM_DAYS_SHORT[] = {"L-D", "L-V", "S-D"};
const char *const WEEKDAY_NAMES[]    = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};

enum AlarmState { ALARM_IDLE, ALARM_RINGING, ALARM_SNOOZED };

// --- Íconos propios del LCD (el código 0 no se usa porque termina los strings) ---
const char ICON_THERMOMETER = 1;
const char ICON_DROP        = 2;
const char ICON_BELL        = 3;
const char DEGREE_SYMBOL    = (char)0xDF;  // "°" en la ROM del HD44780

uint8_t thermometerGlyph[8] = {0b00100, 0b01010, 0b01010, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110};
uint8_t dropGlyph[8]        = {0b00100, 0b00100, 0b01010, 0b01010, 0b10001, 0b10001, 0b01110, 0b00000};
uint8_t bellGlyph[8]        = {0b00100, 0b01110, 0b01110, 0b01110, 0b11111, 0b00000, 0b00100, 0b00000};

// --- Botones ---
// Eventos que devuelve updateButton()
const uint8_t EV_PRESS  = 1;  // Se acaba de presionar
const uint8_t EV_CLICK  = 2;  // Se soltó antes de llegar a pulsación larga
const uint8_t EV_LONG   = 4;  // Lleva LONG_PRESS_MS presionado (una sola vez)
const uint8_t EV_REPEAT = 8;  // Al presionar y luego periódicamente mientras se mantiene

struct Button {
  uint8_t pin;
  bool pressed;
  bool lastReading;
  bool longFired;
  unsigned long lastChangeMs;
  unsigned long pressStartMs;
  unsigned long nextRepeatMs;
  bool ignore;  // Presionado durante un mensaje: no genera eventos hasta soltarlo
};

Button btnSet   = {PIN_BTN_SET, false, false, false, 0, 0, 0, false};
Button btnUp    = {PIN_BTN_UP, false, false, false, 0, 0, 0, false};
Button btnDown  = {PIN_BTN_DOWN, false, false, false, 0, 0, 0, false};
Button btnAlarm = {PIN_BTN_ALARM, false, false, false, 0, 0, 0, false};
Button btnLook  = {PIN_BTN_LOOK, false, false, false, 0, 0, 0, false};

// LCD por I2C: dirección, En, Rw, Rs, D4, D5, D6, D7, luz de fondo, polaridad
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
dht DHT;
virtuabotixRTC rtcModule(PIN_RTC_SCLK, PIN_RTC_IO, PIN_RTC_CE);  // La librería ya define un global "rtc"

// --- Estado ---
int day, month, year, hour, minute, second;

int temperature = 0, humidity = 0;
bool dhtHasReading = false;
uint8_t dhtFailures = 0;
unsigned long lastDhtReadMs = 0;

int alarmHour, alarmMinute;
uint8_t alarmDays;
bool alarmArmed;
AlarmState alarmState = ALARM_IDLE;
bool alarmFired = false;  // Evita que la alarma vuelva a sonar dentro del mismo minuto
unsigned long alarmStartMs = 0;
unsigned long snoozeStartMs = 0;
unsigned long lastNoteMs = 0;
uint8_t melodyIndex = 0;
uint8_t shakeCount = 0;
bool lastShakeHigh = false;
unsigned long lastShakeMs = 0;

uint8_t setupStep = STEP_NONE;
unsigned long lastActivityMs = 0;
// Valores que se editan en el ajuste y foto de cómo estaban al entrar. Se edita
// sobre una copia para que el reloj y la alarma sigan andando mientras tanto.
int editHour, editMinute, editDay, editMonth, editYear;
int entryHour, entryMinute, entryDay, entryMonth, entryYear;
bool entryRtcValid;
int editAlarmHour, editAlarmMinute;
uint8_t editAlarmDays;

bool backlightOn = true;  // Preferencia del usuario; sonando la alarma se prende igual

bool infoActive = false;
unsigned long infoStartMs = 0;

bool messageActive = false;
unsigned long messageStartMs = 0;
unsigned long messageDurationMs = 0;
char messageLines[2][17];

char shownLines[2][17];  // Lo que hay en el LCD, para no reescribir lo que no cambió

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

  loadAlarmSettings();

  lcd.begin(16, 2);
  lcd.createChar(ICON_THERMOMETER, thermometerGlyph);
  lcd.createChar(ICON_DROP, dropGlyph);
  lcd.createChar(ICON_BELL, bellGlyph);
  lcd.clear();
  memset(shownLines, ' ', sizeof(shownLines));
  shownLines[0][16] = shownLines[1][16] = '\0';
  applyBacklight();

  readRtc();
  // El DHT11 necesita ~1 s tras encenderse: la primera lectura se hace a los 1,5 s
  lastDhtReadMs = millis() - DHT_READ_INTERVAL_MS + 1500;
}

void loop() {
  unsigned long now = millis();

  handleButtons(now);

  if (now - lastDhtReadMs >= DHT_READ_INTERVAL_MS) {
    readTempHum(now);
  }

  readRtc();
  updateAlarm(now);

  if (setupStep != STEP_NONE && now - lastActivityMs >= SETUP_TIMEOUT_MS) {
    cancelSetup(now);
  }

  render(now);
}

// ---------------------------------------------------------------------------
// Botones
// ---------------------------------------------------------------------------

uint8_t updateButton(Button &b, unsigned long now) {
  uint8_t events = 0;
  bool reading = digitalRead(b.pin) == LOW;

  if (reading != b.lastReading) {
    b.lastReading = reading;
    b.lastChangeMs = now;
  }

  if (reading != b.pressed && now - b.lastChangeMs >= DEBOUNCE_MS) {
    b.pressed = reading;
    if (!b.pressed && b.ignore) {
      b.ignore = false;
      return 0;
    }
    if (b.pressed) {
      b.pressStartMs = now;
      b.nextRepeatMs = now + REPEAT_DELAY_MS;
      b.longFired = false;
      events |= EV_PRESS | EV_REPEAT;
    } else if (!b.longFired) {
      events |= EV_CLICK;
    }
  }

  if (b.ignore) {
    return 0;
  }

  if (b.pressed) {
    if (!b.longFired && now - b.pressStartMs >= LONG_PRESS_MS) {
      b.longFired = true;
      events |= EV_LONG;
    }
    if ((long)(now - b.nextRepeatMs) >= 0) {
      b.nextRepeatMs = now + REPEAT_INTERVAL_MS;
      events |= EV_REPEAT;
    }
  }

  return events;
}

void handleButtons(unsigned long now) {
  uint8_t evSet   = updateButton(btnSet, now);
  uint8_t evUp    = updateButton(btnUp, now);
  uint8_t evDown  = updateButton(btnDown, now);
  uint8_t evAlarm = updateButton(btnAlarm, now);
  uint8_t evLook  = updateButton(btnLook, now);

  // Mientras se muestra un mensaje se ignoran los botones (el reloj sigue andando).
  // Los que estén apretados quedan ignorados hasta soltarlos, para que no
  // disparen un clic, una pulsación larga o una repetición al terminar el mensaje.
  if (messageVisible(now)) {
    Button *buttons[] = {&btnSet, &btnUp, &btnDown, &btnAlarm, &btnLook};
    for (Button *b : buttons) {
      if (b->pressed) b->ignore = true;
    }
    return;
  }

  if ((evSet | evUp | evDown | evAlarm | evLook) & EV_REPEAT) {
    lastActivityMs = now;
  }

  if (evAlarm & EV_PRESS) {
    onAlarmButton(now);
  }

  if (setupStep == STEP_NONE) {
    if (evLook & EV_CLICK) {
      infoActive = true;
      infoStartMs = now;
    }
    if (evLook & EV_LONG) {
      enterSetup(STEP_ALARM_HOUR, now);
    }
    if (evDown & EV_PRESS) {
      backlightOn = !backlightOn;
      applyBacklight();
    }
    if (evSet & EV_PRESS) {
      enterSetup(STEP_HOUR, now);
      showMessage("---- AJUSTE ----", "- FECHA Y HORA -", 1500, now);
    }
  } else {
    int delta = ((evUp & EV_REPEAT) ? 1 : 0) - ((evDown & EV_REPEAT) ? 1 : 0);
    if (delta != 0) {
      adjustSetupValue(delta);
    }
    if (evSet & EV_PRESS) {
      if (setupStep == STEP_ALARM_DAYS) {
        saveSettings(now);
      } else {
        setupStep++;
      }
    }
  }
}

void onAlarmButton(unsigned long now) {
  switch (alarmState) {
    case ALARM_RINGING:
      // Posponer
      alarmState = ALARM_SNOOZED;
      snoozeStartMs = now;
      noTone(PIN_BUZZER);
      applyBacklight();
      break;
    case ALARM_SNOOZED:
      stopAlarm();
      break;
    case ALARM_IDLE:
      // En el ajuste no se arma/desarma: el cambio no se vería en pantalla
      if (setupStep == STEP_NONE) {
        alarmArmed = !alarmArmed;
        EEPROM.update(EEPROM_ALARM_ARMED, alarmArmed);
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// Sensores
// ---------------------------------------------------------------------------

void readTempHum(unsigned long now) {
  lastDhtReadMs = now;
  if (DHT.read11(PIN_DHT11) == DHTLIB_OK) {
    humidity = DHT.humidity;
    temperature = DHT.temperature;
    dhtHasReading = true;
    dhtFailures = 0;
  } else if (dhtFailures < DHT_MAX_FAILURES) {
    dhtFailures++;
  }
}

bool dhtValid() {
  return dhtHasReading && dhtFailures < DHT_MAX_FAILURES;
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
// Pantalla
// ---------------------------------------------------------------------------

void applyBacklight() {
  if (backlightOn || alarmState == ALARM_RINGING) {
    lcd.backlight();
  } else {
    lcd.noBacklight();
  }
}

// Escribe una línea completa (rellenada a 16 caracteres) solo si cambió
void printLine(uint8_t row, const char *text) {
  char line[17];
  snprintf(line, sizeof(line), "%-16s", text);
  if (strcmp(line, shownLines[row]) != 0) {
    lcd.setCursor(0, row);
    lcd.print(line);
    strcpy(shownLines[row], line);
  }
}

void showMessage(const char *line1, const char *line2, unsigned long durationMs, unsigned long now) {
  strncpy(messageLines[0], line1, 16);
  strncpy(messageLines[1], line2, 16);
  messageLines[0][16] = messageLines[1][16] = '\0';
  messageActive = true;
  messageStartMs = now;
  messageDurationMs = durationMs;
}

bool messageVisible(unsigned long now) {
  if (messageActive && now - messageStartMs >= messageDurationMs) {
    messageActive = false;
  }
  return messageActive;
}

void render(unsigned long now) {
  char line0[17], line1[17];

  if (messageVisible(now)) {
    printLine(0, messageLines[0]);
    printLine(1, messageLines[1]);
    return;
  }

  if (setupStep != STEP_NONE) {
    buildSetupScreen(line0, line1);
  } else if (infoActive && now - infoStartMs < INFO_SCREEN_MS) {
    buildInfoScreen(line0, line1);
  } else {
    infoActive = false;
    buildMainScreen(line0, line1);
  }

  printLine(0, line0);
  printLine(1, line1);
}

char alarmIndicator() {
  if (alarmState == ALARM_SNOOZED) return 'z';
  return alarmArmed ? ICON_BELL : ' ';
}

void buildMainScreen(char *line0, char *line1) {
  char temp[3] = "--", hum[3] = "--";
  if (dhtValid()) {
    // Solo hay lugar para 2 caracteres (el DHT11 mide 0-50 °C y 20-90 %)
    snprintf(temp, sizeof(temp), "%2d", constrain(temperature, -9, 99));
    snprintf(hum, sizeof(hum), "%2d", constrain(humidity, 0, 99));
  }
  snprintf(line0, 17, "%02d:%02d %02d/%02d/%04d", hour, minute, day, month, year);
  snprintf(line1, 17, "%c %s%cC  %c %s%%  %c",
           ICON_THERMOMETER, temp, DEGREE_SYMBOL, ICON_DROP, hum, alarmIndicator());
}

void buildInfoScreen(char *line0, char *line1) {
  const char *status = alarmState == ALARM_SNOOZED ? "ZZZ" : (alarmArmed ? "ON" : "OFF");
  snprintf(line0, 17, "%s %02d:%02d:%02d",
           WEEKDAY_NAMES[dayOfWeek(day, month, year) - 1], hour, minute, second);
  snprintf(line1, 17, "%c %02d:%02d %-3s %s",
           ICON_BELL, alarmHour, alarmMinute, status, ALARM_DAYS_SHORT[alarmDays]);
}

// Línea "    >HH :>MM    " con el cursor '>' delante del campo que se está editando
void buildHourMinuteLine(char *line, int h, int m, bool markHour, bool markMinute) {
  snprintf(line, 17, "    %c%02d :%c%02d    ", markHour ? '>' : ' ', h, markMinute ? '>' : ' ', m);
}

void buildSetupScreen(char *line0, char *line1) {
  if (setupStep <= STEP_YEAR) {
    buildHourMinuteLine(line0, editHour, editMinute, setupStep == STEP_HOUR, setupStep == STEP_MINUTE);
    snprintf(line1, 17, "%c%02d /%c%02d /%c%04d",
             setupStep == STEP_DAY ? '>' : ' ', editDay,
             setupStep == STEP_MONTH ? '>' : ' ', editMonth,
             setupStep == STEP_YEAR ? '>' : ' ', editYear);
  } else if (setupStep <= STEP_ALARM_MINUTE) {
    strcpy(line0, "AJUSTE ALARMA");
    buildHourMinuteLine(line1, editAlarmHour, editAlarmMinute,
                        setupStep == STEP_ALARM_HOUR, setupStep == STEP_ALARM_MINUTE);
  } else {
    strcpy(line0, "DIAS DE ALARMA");
    snprintf(line1, 17, " >%s", ALARM_DAYS_LONG[editAlarmDays]);
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

// Los índices se blindan porque un RTC detenido o desconectado devuelve valores basura
int daysInMonth(int m, int y) {
  static const uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) m = 1;
  return (m == 2 && isLeapYear(y)) ? 29 : DAYS[m - 1];
}

// Día de la semana 1-7 (1 = domingo), algoritmo de Sakamoto
int dayOfWeek(int d, int m, int y) {
  static const uint8_t T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 1 || m > 12) m = 1;
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7 + 1;
}

void loadAlarmSettings() {
  // Una EEPROM sin usar devuelve 255, por eso se valida
  alarmHour   = EEPROM.read(EEPROM_ALARM_HOUR);
  alarmMinute = EEPROM.read(EEPROM_ALARM_MINUTE);
  alarmArmed  = EEPROM.read(EEPROM_ALARM_ARMED) == 1;
  alarmDays   = EEPROM.read(EEPROM_ALARM_DAYS);
  if (alarmHour > 23)                alarmHour = 0;
  if (alarmMinute > 59)              alarmMinute = 0;
  if (alarmDays >= ALARM_DAYS_COUNT) alarmDays = ALARM_EVERY_DAY;
}

bool rtcValuesValid() {
  return hour <= 23 && minute <= 59 && month >= 1 && month <= 12 &&
         year >= MIN_YEAR && year <= MAX_YEAR && day >= 1 && day <= daysInMonth(month, year);
}

void enterSetup(uint8_t step, unsigned long now) {
  // Un RTC nuevo o desconectado trae basura: se parte de valores válidos
  entryRtcValid = rtcValuesValid();
  entryHour   = constrain(hour, 0, 23);
  entryMinute = constrain(minute, 0, 59);
  entryMonth  = constrain(month, 1, 12);
  entryYear   = constrain(year, MIN_YEAR, MAX_YEAR);
  entryDay    = constrain(day, 1, daysInMonth(entryMonth, entryYear));

  editHour  = entryHour;
  editMinute = entryMinute;
  editDay   = entryDay;
  editMonth = entryMonth;
  editYear  = entryYear;
  editAlarmHour   = alarmHour;
  editAlarmMinute = alarmMinute;
  editAlarmDays   = alarmDays;

  infoActive = false;
  setupStep = step;
  lastActivityMs = now;
}

void adjustSetupValue(int delta) {
  switch (setupStep) {
    case STEP_HOUR:         editHour        = wrap(editHour, delta, 0, 23); break;
    case STEP_MINUTE:       editMinute      = wrap(editMinute, delta, 0, 59); break;
    case STEP_DAY:          editDay         = wrap(editDay, delta, 1, daysInMonth(editMonth, editYear)); break;
    case STEP_MONTH:        editMonth       = wrap(editMonth, delta, 1, 12); break;
    case STEP_YEAR:         editYear        = wrap(editYear, delta, MIN_YEAR, MAX_YEAR); break;
    case STEP_ALARM_HOUR:   editAlarmHour   = wrap(editAlarmHour, delta, 0, 23); break;
    case STEP_ALARM_MINUTE: editAlarmMinute = wrap(editAlarmMinute, delta, 0, 59); break;
    case STEP_ALARM_DAYS:   editAlarmDays   = wrap(editAlarmDays, delta, 0, ALARM_DAYS_COUNT - 1); break;
  }
  // Corrige días inválidos al cambiar mes o año (ej.: 31/04 -> 30/04)
  editDay = min(editDay, daysInMonth(editMonth, editYear));
}

void saveSettings(unsigned long now) {
  // Solo se reescribe el RTC con lo que el usuario cambió: escribir la hora que
  // había al entrar al menú haría que el reloj se atrase.
  bool clockChanged = editHour != entryHour || editMinute != entryMinute || !entryRtcValid;
  bool dateChanged  = editDay != entryDay || editMonth != entryMonth || editYear != entryYear;
  if (clockChanged) {
    rtcModule.setDS1302Time(0, editMinute, editHour, dayOfWeek(editDay, editMonth, editYear),
                            editDay, editMonth, editYear);
  } else if (dateChanged) {
    // Solo cambió la fecha: se conserva la hora actual, que siguió corriendo
    rtcModule.updateTime();
    rtcModule.setDS1302Time(rtcModule.seconds, rtcModule.minutes, rtcModule.hours,
                            dayOfWeek(editDay, editMonth, editYear), editDay, editMonth, editYear);
  }

  alarmHour   = editAlarmHour;
  alarmMinute = editAlarmMinute;
  alarmDays   = editAlarmDays;
  EEPROM.update(EEPROM_ALARM_HOUR, alarmHour);
  EEPROM.update(EEPROM_ALARM_MINUTE, alarmMinute);
  EEPROM.update(EEPROM_ALARM_DAYS, alarmDays);

  setupStep = STEP_NONE;
  showMessage("Guardando...", "", 1000, now);
}

void cancelSetup(unsigned long now) {
  setupStep = STEP_NONE;  // Las copias editadas se descartan
  showMessage("Ajuste", "cancelado", 1000, now);
}

// ---------------------------------------------------------------------------
// Alarma
// ---------------------------------------------------------------------------

bool alarmAppliesToday() {
  int dow = dayOfWeek(day, month, year);
  bool weekend = dow == 1 || dow == 7;
  switch (alarmDays) {
    case ALARM_WEEKDAYS: return !weekend;
    case ALARM_WEEKENDS: return weekend;
    default:             return true;
  }
}

void startRinging(unsigned long now) {
  alarmState    = ALARM_RINGING;
  alarmStartMs  = now;
  lastNoteMs    = now - MELODY_STEP_MS;  // La primera nota suena de inmediato
  melodyIndex   = 0;
  shakeCount    = 0;
  lastShakeHigh = analogRead(PIN_SHAKE) > SHAKE_THRESHOLD;  // En reposo puede leer alto
  infoActive    = false;
  applyBacklight();
}

void stopAlarm() {
  alarmState = ALARM_IDLE;
  noTone(PIN_BUZZER);
  applyBacklight();
}

void updateAlarm(unsigned long now) {
  bool isAlarmMinute = hour == alarmHour && minute == alarmMinute;
  if (!isAlarmMinute) {
    alarmFired = false;
  } else if (alarmArmed && !alarmFired && alarmAppliesToday()) {
    alarmFired = true;
    startRinging(now);
  }

  if (!alarmArmed && alarmState != ALARM_IDLE) {
    stopAlarm();
  }
  if (alarmState == ALARM_SNOOZED && now - snoozeStartMs >= SNOOZE_MS) {
    startRinging(now);
  }
  if (alarmState != ALARM_RINGING) {
    return;
  }

  // Se cuenta cada vez que el sensor pasa de reposo a movimiento, filtrando rebotes
  bool shakeHigh = analogRead(PIN_SHAKE) > SHAKE_THRESHOLD;
  if (shakeHigh && !lastShakeHigh && now - lastShakeMs >= SHAKE_DEBOUNCE_MS) {
    shakeCount++;
    lastShakeMs = now;
  }
  lastShakeHigh = shakeHigh;

  if (shakeCount >= SHAKES_TO_STOP || now - alarmStartMs >= ALARM_MAX_RING_MS) {
    stopAlarm();
    return;
  }

  if (now - lastNoteMs >= MELODY_STEP_MS) {
    lastNoteMs = now;
    tone(PIN_BUZZER, MELODY[melodyIndex], 100);
    melodyIndex = (melodyIndex + 1) % MELODY_LEN;
  }
}
