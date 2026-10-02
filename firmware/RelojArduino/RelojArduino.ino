/*  Clock - Thermometer with Arduino
 *   
 *  Display 16x2:       Display 16x2 (2):   Display 16x2 (3)    Setup:                Display 16x2 (4)  Setup Alarm
 *  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+  
 *  |HH:MM DD/MM/YYYY|  |HH:MM:SS|* HH:MM|  |------SET-------|  |    >HH :>MM    |  |------SET-------|  |   Set Alarm    |  
 *  |Temp:27c Hum:74%|  |DD/MM/YY|  ALARM|  |-TIME and DATE--|  |>DD />MM />YYYY |  |-- TIME ALARM --|  |   >HH :>MM     |
 *  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+  +----------------+ 
 */

//Librerias
#include <dht.h>                  //Libreria para trabajar con el sensor DHT11
#include <Wire.h>                 //Libreria de comunicacion por I2C
#include <LCD.h>                  //Libreria para funciones de LCD
#include <LiquidCrystal_I2C.h>    //Libreria para LCD por I2C
#include <virtuabotixRTC.h>       //Libreria para Modulo Reloj
#include <EEPROM.h>               //Libreria para Manejo de EEPROM de Arduino

//Objetos de las Librerias
LiquidCrystal_I2C lcd (0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // DIR, E, RW, RS, D4, D5, D6, D7 - Objeto modulo pantalla
dht DHT;                         //Objeto Sensor DHT11
virtuabotixRTC myRTC(6, 7, 8);  //Objeto Modulo Reloj

//Constantes
char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
#define DHT11_PIN 5           //DHT 11 Pin donde se encuentra conectado el Sensor
const long interval = 6000;   //Leer los datos de DHT11 cada 6 Segundos
const int btLook = 4;         //Pines de los Botones
const int btSet = 9;
const int btUp = 10;
const int btDown = 11;
const int btAlarm = 12;
const int buzzer = 13;        //Buzzer
const int shakeSensor = A3;   //Sensor de Movimiento
long intercal = 300;
int melody[] = {600, 800, 1000, 1200}; //Melodia del Buzzer

//Variables
int DD,MM,YY,H,M,S,temp,hum, set_state, up_state, down_state, alarm_state, adjust_state, AH, AM, shake_state, look;
int btnCount = 0;
unsigned long previousMillis = 0;
unsigned long currentMillis; 
int i = 0;
String sDD;
String sMM;
String sYY;
String sH;
String sM;
String sS;
String aH = "12";
String aM = "00";
String alarm = "OFF";
boolean backlightON = true;
boolean setupScreen = false;
boolean alarmON = false;
boolean turnItOn = false;
int shakeTimes = 0;
boolean testigo = false;

void setup() {
  
  //(segundos(00), minutos(00), hora(00), díasemana(0), díadelmes(00), mes(00), año(00))
  //myRTC.setDS1302Time(00, 05, 22, 2, 8, 03, 2022); //Descomentar solo la primera vez que se use para setear Hora y Fecha
  
  pinMode(btSet, INPUT_PULLUP); //Inicializar Botonera
  pinMode(btUp, INPUT_PULLUP);
  pinMode(btDown, INPUT_PULLUP);
  pinMode(btLook, INPUT_PULLUP); 
  pinMode(btAlarm, INPUT_PULLUP);
  
  pinMode(buzzer, OUTPUT);      //Inicializar el Buzzer

  AH=EEPROM.read(0);            //Leer la hora de la alarma de EEPROM     
  AM=EEPROM.read(1);
  
  //Verificacion de que la hora de la alarma sea valida
  if (AH > 23) {
    AH =0 ;
  }

  if (AM > 59) {
    AM = 0;
  }
  
  lcd.begin(16,2);              //Inicializar pantalla de 16 caracateres y 2 lineas
  lcd.backlight();              //Inicializar Retroiluminacion pantalla
  lcd.clear();                  //Limpiar la pantalla

}

void loop() {

  currentMillis = millis();
  readBtns();
  getTempHum(); 
  getTimeDate();
  
  if (!setupScreen) {

    lcdPrint();

    if (alarmON) {
      callAlarm();
    }

  } else {
    lcdSetup();
  }

}

//Funciones

//Leer Botonera
void readBtns() {

  set_state = digitalRead(btSet);       //BTN1
  up_state = digitalRead(btUp);         //BTN2
  down_state = digitalRead(btDown);     //BTN3
  alarm_state = digitalRead(btAlarm);   //BTN4
  look = digitalRead(btLook);           //BTN5

  //Activa o Desactiva la Alarma
  if (alarm_state == LOW) {

    if (alarmON) {

      alarm = "OFF";
      alarmON = false;

    } else {

      alarm = "ON";
      alarmON = true;

    }

    delay(500);

  }

  //Pintar Display 16x2 (4)
  if (look == LOW) {

    lcd.clear();
    lcdPrintAlarm();
    delay(1000);
    lcd.clear();

  }
  
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

    if(btnCount < 7) {

      btnCount++;
      setupScreen = true;

        if (btnCount == 1) {

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("------SET------");
          lcd.setCursor(0, 1);
          lcd.print("-TIME and DATE-");
          delay(2000);
          lcd.clear();

        }

    } else { /*Actualizar Fecha y Hora*/

      lcd.clear();
      //rtc.adjust(DateTime(YY, MM, DD, H, M, 0)); //Codigo viejo (Otro modulo Reloj)
      myRTC.setDS1302Time(S, M, H, 0, DD, MM, YY);
      EEPROM.write(0, AH);  //Guardar la Hora de la Alarma en la EEPROM
      EEPROM.write(1, AM);  //Guardar el Minuto de la Alarma en la EEPROM
      lcd.print("Saving....");
      delay(2000);
      lcd.clear();
      setupScreen = false;
      btnCount = 0;

    }

    delay(500);  

  }

}

//Obtener Temperatura y Humedad
void getTempHum() {

  if (currentMillis - previousMillis >= interval) {

    int chk = DHT.read11(DHT11_PIN);
    previousMillis = currentMillis;    
    hum = DHT.humidity;
    temp = DHT.temperature;

  }

}

//Obtener Fecha y Hora del modulo
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
  if (DD < 10){ sDD = '0' + String(DD); } else { sDD = DD; }  //Hora y Fecha
  if (MM < 10){ sMM = '0' + String(MM); } else { sMM = MM; }
  sYY = YY;
  if (H < 10){ sH = '0' + String(H); } else { sH = H; }
  if (M < 10){ sM = '0' + String(M); } else { sM = M; }
  if (S < 10){ sS = '0' + String(S); } else { sS = S; }

  if (AH < 10){ aH = '0' + String(AH); } else { aH = AH; }    //Hora y Minuto de la Alarma
  if (AM < 10){ aM = '0' + String(AM); }  else { aM = AM; }

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

//Pintar Display 16x2 (2)
void lcdPrintAlarm() {

  String line1 = sH+":"+sM+":"+sS+"| "+aH+":"+aM;
  String line2 = sDD+"/"+sMM+"/"+sYY +"| "+alarm;
  lcd.setCursor(0,0); //Primera Fila
  lcd.print(line1);
  lcd.setCursor(0,1); //Segunda Fila
  lcd.print(line2);  

}

//Pintar Setup - Toda la logica en el Mecanismo de Actualizacion Fecha y Hora
void lcdSetup() {

  if (btnCount <= 5) {

  //Setear Hora
  if (btnCount == 1) {

    lcd.setCursor(4,0);
    lcd.print(">"); 

  //Up boton +
    if (up_state == LOW) {

      if (H < 23) {
        H++;
      } else {
        H = 0;
      }

      delay(350);

    }

    //Down boton -
    if (down_state == LOW) {

      if (H > 0) {
        H--;
      } else {
        H = 23;
      }

      delay(350);

    }

  } else if (btnCount == 2) { /*Setear Minutos*/

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

      delay(350);

    }

    if (down_state == LOW) {

      if (M > 0) {
        M--;
      } else {
        M = 59;
      }

      delay(350);

    }

  } else if (btnCount == 3) { /*Setear Dia*/

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

      delay(350);

    }

    if (down_state == LOW) {

      if (DD > 1) {
        DD--;
      } else {
        DD = 31;
      }

      delay(350);

    }

  } else if (btnCount == 4) { /*Setear Mes*/

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

      delay(350);

    }

    if (down_state == LOW) {

      if (MM > 1) {
        MM--;
      } else {
        MM = 12;
      }

      delay(350);

    }

  } else if (btnCount == 5) { /*Setear Anio*/

    lcd.setCursor(5,1);
    lcd.print(" ");
    lcd.setCursor(10,1);
    lcd.print(">");

    if (up_state == LOW) {

      if (YY < 2999) {
        YY++;
      } else {
        YY = 2000;
      }

      delay(350);

    }

    if (down_state == LOW) {

      if (YY > 2000) {
        YY--;
      } else {
        YY = 2999;
      }

      delay(350);

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

  } else {
    setAlarmTime();
  }

}

//Pintar Setup Alarm - Toda la logica en el Mecanismo de Actualizacion Fecha y Hora de alarma
void setAlarmTime() {

  //int up_state = adjust_state;    //Codigo viejo
  //int down_state = alarm_state;
  String line2;
  
  lcd.setCursor(0,0);
  lcd.print("SET  ALARM TIME");

  //Setear la Hora de la Alarma
  if (btnCount == 6) {

    if (up_state == LOW) {

      if (AH < 23) {
        AH++;
      } else {
        AH = 0;
      }

      delay(350);

    }

    if (down_state == LOW) {

      if (AH > 0) {
        AH--;
      } else {
        AH = 23;
      }

      delay(350);

    }

    line2 = "    >"+aH+" : "+aM+"    ";

  } else if (btnCount == 7) { /*Setear los Minutos de la alarma*/

    if (up_state == LOW) {

      if (AM < 59) {
        AM++;
      } else {
        AM = 0;
      }

      delay(350);

    }

    if (down_state == LOW) {

      if (AM > 0) {
        AM--;
      } else {
        AM = 59;
      }

      delay(350);

    }

    line2 = "     "+aH+" :>"+aM+"    ";

  }

  lcd.setCursor(0,1);
  lcd.print(line2);

}

//Funcion que llama al funcionamiento de la alarma
void callAlarm() {

  if (aM == sM && aH == sH && S >= 0 && S <=2 ) {
    turnItOn = true;
  }

  if (alarm_state == LOW || shakeTimes >= 6 || (M == (AM + 5))) {

    turnItOn = false;
    alarmON = true;
    delay(500);

  } 

  if (analogRead(shakeSensor)>200) {

    shakeTimes++;
    Serial.print(shakeTimes);
    delay(50);

  }

  if (turnItOn) {

    unsigned long currentMillis = 10000; // millis(); Codigo viejo

    if (currentMillis - previousMillis > interval) {

      previousMillis = currentMillis;   
      tone(buzzer, melody[i], 100);
      i++;
      
      if (i > 3){ i = 0; };

    }

  } else {

    noTone(buzzer);
    shakeTimes = 0;

  }
  
}
