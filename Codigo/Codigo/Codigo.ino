/*  Clock - Thermometer with Arduino
 *   
 *  Display 16x2:       Display 16x2 (2):   Display 16x2 (3)    Setup:              Setup Alarm
 *  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+  
 *  |HH:MM DD/MM/YYYY|  |HH:MM:SS|* HH:MM|  |------SET-------|  |    >HH :>MM    |  |   Set Alarm    |
 *  |Temp:27c Hum:74%|  |DD/MM/YY|  ALARM|  |-TIME and DATE--|  |>DD />MM />YYYY |  |   >HH :>MM     |
 *  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+ 
 */

//Librerias
#include <dht.h>                  //Libreria para trabajar con el sensor DHT11
#include <Wire.h>                 //Libreria de comunicacion por I2C
#include <LCD.h>                  //Libreria para funciones de LCD
#include <LiquidCrystal_I2C.h>    //Libreria para LCD por I2C
#include <virtuabotixRTC.h>       //Libreria para Modulo Reloj

//Objetos de las Librerias
LiquidCrystal_I2C lcd (0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // DIR, E, RW, RS, D4, D5, D6, D7 - Objeto modulo pantalla
dht DHT;                         //Objeto Sensor DHT11
virtuabotixRTC myRTC(6, 7, 8);  //Objeto Modulo Reloj

//Constantes
char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
#define DHT11_PIN 5           //DHT 11  (AM2302) - El PIN al que estamos conectados
const long interval = 6000;  //Leer los datos de DHT11 cada 6 Segundos
const int btSet = 9;
const int btUp = 10;
const int btDown = 11;

//Variables
int DD, MM, YY, H, M, S, temp, hum, set_state, up_state, down_state;
int btnCount = 0;
unsigned long previousMillis = 0;
unsigned long currentMillis; 
String sDD;
String sMM;
String sYY;
String sH;
String sM;
String sS;
boolean backlightON = true;
boolean setupScreen = false;

void setup() {

  pinMode(btSet, INPUT_PULLUP); //Inicializar Botonera
  pinMode(btUp, INPUT_PULLUP);
  pinMode(btDown, INPUT_PULLUP);
  
  lcd.begin(16,2);   //Inicializar pantalla de 16 caracateres y 2 lineas
  lcd.backlight();   //Inicializar Retroiluminacion pantalla
  lcd.clear();       //Limpiar la pantalla

}

void loop() {

  currentMillis = millis();

  readBtns();
  getTempHum();    
  getTimeDate();

  if (!setupScreen) {
    lcdPrint();
  } else {
    lcdSetup();
  }

}

/**---------- Funciones ----------**/

/**
* Capta la actividad en la botonera
*/
void readBtns() {

  set_state = digitalRead(btSet);
  up_state = digitalRead(btUp);
  down_state = digitalRead(btDown);
  
  //Apagar o prender la Retroilumnacion de la pantalla
  if (down_state == LOW && btnCount == 0) {

    if (backlightON) {

      lcd.noBacklight();
      backlightON = false;

    } else {

      lcd.backlight();
      backlightON = true;

    }

    delay(500);

  }

  //Pintar Display 16x2 (3)
  if (set_state == LOW) {

    if(btnCount < 5) {

      btnCount++;
      setupScreen = true;

        if (btnCount == 1) {

          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("------SET------");
          lcd.setCursor(0,1);
          lcd.print("-TIME and DATE-");
          delay(2000);
          lcd.clear();

        }

    } else {  /*Actualizar Fecha y Hora */

      lcd.clear();
      //rtc.adjust(DateTime(YY, MM, DD, H, M, 0)); //Codigo viejo (Otro modulo Reloj)
      myRTC.setDS1302Time(S, M, H, 0, DD, MM, YY);
      lcd.print("Saving....");
      delay(2000);
      lcd.clear();
      setupScreen = false;
      btnCount = 0;

    }

    delay(500);

  }

}

/**
* Obtiene la Temperatura y la Humedad
*/
void getTempHum() {

  if (currentMillis - previousMillis >= interval) {

    int chk = DHT.read11(DHT11_PIN);
    previousMillis = currentMillis;    
    hum = DHT.humidity;
    temp= DHT.temperature;

  }

}

/**
* Obtiene la Fecha y Hora del modulo
*/
void getTimeDate() {

  if (!setupScreen) {

    myRTC.updateTime();
    DD = myRTC.dayofmonth;
    MM = myRTC.month;
    YY = myRTC.year;
    H = myRTC.hours;
    M = myRTC.minutes;
    S = myRTC.seconds;
    
    /* Codigo viejo (Otro modulo Reloj)
      DateTime now = rtc.now();
      DD = now.day();
      MM = now.month();
      YY = now.year();
      H = now.hour();
      M = now.minute();
      S = now.second();
    */
    
  }
  
  //No tengo idea bien de que hace este codigo
  if (DD < 10){ sDD = '0' + String(DD); } else { sDD = DD; }
  if (MM < 10){ sMM = '0' + String(MM); } else { sMM = MM; }
  sYY = YY;
  if (H < 10){ sH = '0' + String(H); } else { sH = H; }
  if (M < 10){ sM = '0' + String(M); } else { sM = M; }
  if (S < 10){ sS = '0' + String(S); } else { sS = S; }

}

//Pintar Display 16x2
void lcdPrint() {

  lcd.setCursor(0,0); //Primera Fila
  lcd.print(sH);
  lcd.print(":");
  lcd.print(sM);
  lcd.print(" ");
  lcd.print(sDD);
  lcd.print("/");
  lcd.print(sMM);
  lcd.print("/");
  lcd.print(sYY);
  lcd.setCursor(0,1); //Segunda Fila
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print("c");
  lcd.setCursor(9,1); //10 celdas de segunda fila
  lcd.print("Hum:");
  lcd.print(hum);
  lcd.print("%");

}

//Pintar Setup - Toda la logica en el Mecanismo de Actualizacion Fecha y Hora
void lcdSetup() {

  if (btnCount == 1) {

    lcd.setCursor(4,0);
    lcd.print(">"); 
    if (up_state == LOW) {

      if (H < 23) {
        H++;
      } else {
        H = 0;
      }

      delay(500);
    }

    if (down_state == LOW) {

      if (H > 0){
        H--;
      } else {
        H = 23;
      }

      delay(500);

    }

  } else if (btnCount == 2) {

    lcd.setCursor(4,0);
    lcd.print(" ");
    lcd.setCursor(9,0);
    lcd.print(">");

    if (up_state == LOW) {

      if (M < 59) {
        M++;
      } else {
        M = 0;
      }

      delay(500);

    }

    if (down_state == LOW) {

      if (M > 0) {
        M--;
      } else {
        M = 59;
      }

      delay(500);

    }

  } else if (btnCount == 3) {

    lcd.setCursor(9,0);
    lcd.print(" ");
    lcd.setCursor(0,1);
    lcd.print(">");

    if (up_state == LOW) {

      if (DD < 31) {
        DD++;
      } else {
        DD = 1;
      }

      delay(500);

    }

    if (down_state == LOW) {

      if (DD > 1) {
        DD--;
      } else {
        DD = 31;
      }

      delay(500);

    }

  } else if (btnCount == 4) {

    lcd.setCursor(0,1);
    lcd.print(" ");
    lcd.setCursor(5,1);
    lcd.print(">");

    if (up_state == LOW) {

      if (MM < 12) {
        MM++;
      } else {
        MM = 1;
      }

      delay(500);

    }

    if (down_state == LOW) {

      if (MM > 1) {
        MM--;
      } else {
        MM = 12;
      }

      delay(500);

    }

  } else if (btnCount == 5) {

    lcd.setCursor(5,1);
    lcd.print(" ");
    lcd.setCursor(10,1);
    lcd.print(">");

    if (up_state == LOW) {

      if (YY < 2999) {
        YY++;
      }
      else {
        YY = 2000;
      }

      delay(500);

    }

    if (down_state == LOW) {

      if (YY > 2000) {
        YY--;
      } else {
        YY = 2999;
      }

      delay(500);

    }

  }

  lcd.setCursor(5,0);
  lcd.print(sH);
  lcd.setCursor(8,0);
  lcd.print(":");
  lcd.setCursor(10,0);
  lcd.print(sM);
  lcd.setCursor(1,1);
  lcd.print(sDD);
  lcd.setCursor(4,1);
  lcd.print("/");
  lcd.setCursor(6,1);
  lcd.print(sMM);
  lcd.setCursor(9,1);
  lcd.print("/");
  lcd.setCursor(11,1);
  lcd.print(sYY);

}
