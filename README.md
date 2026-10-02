# Reloj Arduino

[![Compilar firmware](https://github.com/Varucard/Reloj-Arduino/actions/workflows/compile.yml/badge.svg)](https://github.com/Varucard/Reloj-Arduino/actions/workflows/compile.yml)

Reloj despertador casero hecho con Arduino. Muestra la hora, la fecha, la temperatura y la humedad ambiente, y tiene una alarma que se apaga **sacudiendo el reloj**.

## Funcionalidades

- Hora y fecha desde un módulo RTC DS1302, que mantiene la hora aunque se corte la alimentación.
- Temperatura y humedad con un sensor DHT11, actualizadas cada 6 segundos (muestra `--` si el sensor falla).
- Alarma con melodía, configurable para **todos los días, lunes a viernes o sábado y domingo**.
- Hora, días y estado de la alarma guardados en EEPROM, así que se conservan aunque se corte la luz.
- **Posponer (snooze)** 5 minutos con el botón ALARM; la alarma se apaga con 6 sacudidas (sensor SW-520D) o sola a los 5 minutos.
- La luz de fondo se prende sola cuando suena la alarma.
- Programa sin bloqueos: el reloj sigue andando mientras se usan los botones, y mantener UP o DOWN apretado avanza rápido.
- Íconos propios en la pantalla (termómetro, gota y campana de alarma armada).

## Hardware

| Componente | Detalle |
|---|---|
| Placa | Arduino Uno o Arduino Pro Mini (ATmega328P) |
| Pantalla | LCD 16x2 con adaptador I2C (PCF8574, dirección `0x27`) |
| Reloj | Módulo RTC DS1302 |
| Sensor ambiente | DHT11 |
| Sensor de movimiento | SW-520D (inclinación) |
| Sonido | Buzzer pasivo |
| Entradas | 5 pulsadores |

### Conexiones

| Pin Arduino | Conectado a |
|---|---|
| A4 / A5 | LCD I2C (SDA / SCL) |
| 4 | Botón LOOK |
| 5 | DHT11 (datos) |
| 6 / 7 / 8 | DS1302 (CLK / DAT / RST) |
| 9 | Botón SET |
| 10 | Botón UP |
| 11 | Botón DOWN |
| 12 | Botón ALARM |
| 13 | Buzzer |
| A3 | SW-520D |

Los botones van entre el pin y GND; se usan las resistencias pull-up internas. Los esquemas completos están en [`hardware/esquemas`](hardware/esquemas) (PNG y archivos editables de [Fritzing](https://fritzing.org/)).

## Uso

| Botón | Funcionamiento normal | En modo ajuste |
|---|---|---|
| **SET** | Entra al modo ajuste | Pasa al siguiente campo; en el último, guarda |
| **UP** | — | Aumenta el valor (mantener = avance rápido) |
| **DOWN** | Prende o apaga la luz de fondo | Disminuye el valor (mantener = avance rápido) |
| **ALARM** | Arma o desarma la alarma | Solo pospone o cancela si está sonando |
| **LOOK** | Clic: muestra 3 s la pantalla de info · Mantener 1 s: ajuste directo de la alarma | — |

Mientras suena la alarma:

| Acción | Resultado |
|---|---|
| Botón **ALARM** | Pospone 5 minutos (en pantalla aparece `z`) |
| Botón **ALARM** con la alarma pospuesta | Cancela la alarma hasta el día siguiente |
| **Sacudir** el reloj 6 veces | Apaga la alarma |
| No hacer nada | Se apaga sola a los 5 minutos |

El modo ajuste recorre, en orden: hora → minutos → día → mes → año → hora de alarma → minutos de alarma → días de alarma. Mientras estás en el ajuste, el reloj y la alarma siguen funcionando. Al guardar, el RTC solo se reescribe con lo que cambiaste: si cambiaste la hora, los segundos se ponen en 00; si solo cambiaste la fecha, se conserva la hora actual. Si no se toca ningún botón durante 30 segundos, el ajuste se cancela sin guardar.

```
Pantalla principal   Info (LOOK)          Ajuste fecha/hora    Ajuste alarma        Días de alarma
+----------------+   +----------------+   +----------------+   +----------------+   +----------------+
|HH:MM DD/MM/YYYY|   |Vie HH:MM:SS    |   |    >HH : MM    |   |AJUSTE ALARMA   |   |DIAS DE ALARMA  |
|T 27°C  D 74%  A|   |A HH:MM ON  L-V |   | DD / MM / YYYY |   |    >HH : MM    |   |  >Lun a Vie    |
+----------------+   +----------------+   +----------------+   +----------------+   +----------------+
```

En la pantalla, `T`, `D` y `A` son íconos (termómetro, gota y campana). La campana aparece solo si la alarma está armada.

## Compilación

### Librerías

| Librería | Uso | Instalación |
|---|---|---|
| [DHTlib](https://github.com/RobTillaart/DHTlib) | `dht.h` – sensor DHT11 | Gestor de librerías del IDE |
| [New LiquidCrystal](https://github.com/fmalpartida/New-LiquidCrystal) | `LCD.h`, `LiquidCrystal_I2C.h` – pantalla | Agregar el `.zip` desde GitHub |
| [virtuabotixRTC](https://github.com/chrisfryer78/ArduinoRTClibrary) | `virtuabotixRTC.h` – DS1302 | [`libraries/virtuabotixRTC.zip`](libraries) |

> **Importante:** New LiquidCrystal reemplaza a la librería `LiquidCrystal_I2C` de Frank de Brabander. Si tenés esa otra instalada, desinstalala o renombrala, porque si no el sketch no compila.

### Arduino IDE

1. Instalar las librerías (*Programa → Incluir librería → Añadir biblioteca .ZIP*).
2. Abrir `firmware/RelojArduino/RelojArduino.ino`.
3. Elegir la placa (Uno o Pro Mini) y subir.

### arduino-cli

```bash
arduino-cli core install arduino:avr
arduino-cli lib install DHTlib
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/fmalpartida/New-LiquidCrystal.git
arduino-cli lib install --zip-path libraries/virtuabotixRTC.zip

arduino-cli compile firmware/RelojArduino                               # Uno (por defecto)
arduino-cli compile -b arduino:avr:pro firmware/RelojArduino            # Pro Mini
arduino-cli upload -p /dev/ttyUSB0 firmware/RelojArduino
```

Cada push compila el firmware automáticamente para ambas placas con GitHub Actions.

## Estructura del repositorio

```
firmware/RelojArduino/   Sketch principal
hardware/esquemas/       Diagramas de conexión (PNG y Fritzing .fzz)
libraries/               Copias de las librerías usadas
docs/datasheets/         Hojas de datos de los componentes
docs/referencias/        Proyectos de Instructables en los que se basa este reloj
```

## Créditos

Basado en estos proyectos de Instructables, adaptados y extendidos:

- [Arduino Digital Clock With Alarm Function (custom PCB)](https://www.instructables.com/Arduino-Digital-Clock-With-Alarm-Function-custom-P/)
- [Arduino Digital Clock Thermometer (3D Printer Files)](https://www.instructables.com/Arduino-Digital-Clock-Thermometer-3D-Printer-Files/)
