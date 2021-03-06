/*  Clock - Thermometer with Arduino
 *  More info: http://www.ardumotive.com/
 *  Dev: Michalis Vasilakis Data: 19/11/2016 Ver: 1.0
 *  
 *  Display 16x2:         Setup:
 *  +----------------+  +----------------+
 *  |HH:MM DD/MM/YYYY|  |    >HH :>MM    |
 *  |Temp:27c Hum:74%|  |>DD />MM />YYYY |
 *  +----------------+  +----------------+
 */

//Libraries
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <dht.h>

//Init libraries objects
RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address, if it's not working try 0x27.
dht DHT;

//Constants
char daysOfTheWeek[7][12] = {"Sunday","Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
#define DHT21_PIN 5     // DHT 22  (AM2302) - what pin we're connected to
const long interval = 6000;  // Read data from DHT every 6 sec
const int btSet = 8;
const int btUp = 9;
const int btDown = 10;

//Variables
int DD,MM,YY,H,M,S,temp,hum, set_state, up_state, down_state;
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


void setup () {
  pinMode(btSet, INPUT_PULLUP);
  pinMode(btUp, INPUT_PULLUP);
  pinMode(btDown, INPUT_PULLUP);
  lcd.begin(16,2);   // iInit the LCD for 16 chars 2 lines
  lcd.backlight();   // Turn on the backligt (try lcd.noBacklight() to turn it off)
  lcd.clear();
}

void loop () {
  currentMillis = millis();
  readBtns();
  getTempHum();    
  getTimeDate();
  if (!setupScreen){
    lcdPrint();
  }
  else{
    lcdSetup();
  }
}
//Read buttons
void readBtns(){
  set_state = digitalRead(btSet);
  up_state = digitalRead(btUp);
  down_state = digitalRead(btDown);
  //Turn backlight on/off by pressing the down button
  if (down_state==LOW && btnCount==0){
    if (backlightON){
      lcd.noBacklight();
      backlightON = false;
    }
    else{
      lcd.backlight();
      backlightON = true;
    }
    delay(500);
  }
  if (set_state==LOW){
    if(btnCount<5){
      btnCount++;
      setupScreen = true;
        if(btnCount==1){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("------SET------");
          lcd.setCursor(0,1);
          lcd.print("-TIME and DATE-");
          delay(2000);
          lcd.clear();
        }
    }
    else{
      lcd.clear();
      rtc.adjust(DateTime(YY, MM, DD, H, M, 0));
      lcd.print("Saving....");
      delay(2000);
      lcd.clear();
      setupScreen = false;
      btnCount=0;
    }
    delay(500);
  }
}
//Read temperature and humidity every 6 seconds from DHT sensor
void getTempHum(){
  if (currentMillis - previousMillis >= interval) {
    int chk = DHT.read21(DHT21_PIN);
    previousMillis = currentMillis;    
    hum = DHT.humidity;
    temp= DHT.temperature;
  }
}
//Read time and date from rtc ic
void getTimeDate(){
  if (!setupScreen){
    DateTime now = rtc.now();
    DD = now.day();
    MM = now.month();
    YY = now.year();
    H = now.hour();
    M = now.minute();
    S = now.second();
  }
  //Make some fixes...
  if (DD<10){ sDD = '0' + String(DD); } else { sDD = DD; }
  if (MM<10){ sMM = '0' + String(MM); } else { sMM = MM; }
  sYY=YY;
  if (H<10){ sH = '0' + String(H); } else { sH = H; }
  if (M<10){ sM = '0' + String(M); } else { sM = M; }
  if (S<10){ sS = '0' + String(S); } else { sS = S; }
}
//Print values to the display
void lcdPrint(){
  lcd.setCursor(0,0); //First row
  lcd.print(sH);
  lcd.print(":");
  lcd.print(sM);
  lcd.print(" ");
  lcd.print(sDD);
  lcd.print("/");
  lcd.print(sMM);
  lcd.print("/");
  lcd.print(sYY);
  lcd.setCursor(0,1); //Second row
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print("c");
  lcd.setCursor(9,1); //10 cell of second row
  lcd.print("Hum:");
  lcd.print(hum);
  lcd.print("%");
}
//Setup screen
void lcdSetup(){
  if (btnCount==1){
    lcd.setCursor(4,0);
    lcd.print(">"); 
    if (up_state == LOW){
      if (H<23){
        H++;
      }
      else {
        H=0;
      }
      delay(500);
    }
    if (down_state == LOW){
      if (H>0){
        H--;
      }
      else {
        H=23;
      }
      delay(500);
    }
  }
  else if (btnCount==2){
    lcd.setCursor(4,0);
    lcd.print(" ");
    lcd.setCursor(9,0);
    lcd.print(">");
    if (up_state == LOW){
      if (M<59){
        M++;
      }
      else {
        M=0;
      }
      delay(500);
    }
    if (down_state == LOW){
      if (M>0){
        M--;
      }
      else {
        M=59;
      }
      delay(500);
    }
  }
  else if (btnCount==3){
    lcd.setCursor(9,0);
    lcd.print(" ");
    lcd.setCursor(0,1);
    lcd.print(">");
    if (up_state == LOW){
      if (DD<31){
        DD++;
      }
      else {
        DD=1;
      }
      delay(500);
    }
    if (down_state == LOW){
      if (DD>1){
        DD--;
      }
      else {
        DD=31;
      }
      delay(500);
    }
  }
  else if (btnCount==4){
    lcd.setCursor(0,1);
    lcd.print(" ");
    lcd.setCursor(5,1);
    lcd.print(">");
    if (up_state == LOW){
      if (MM<12){
        MM++;
      }
      else {
        MM=1;
      }
      delay(500);
    }
    if (down_state == LOW){
      if (MM>1){
        MM--;
      }
      else {
        MM=12;
      }
      delay(500);
    }
  }
  else if (btnCount==5){
    lcd.setCursor(5,1);
    lcd.print(" ");
    lcd.setCursor(10,1);
    lcd.print(">");
    if (up_state == LOW){
      if (YY<2999){
        YY++;
      }
      else {
        YY=2000;
      }
      delay(500);
    }
    if (down_state == LOW){
      if (YY>2000){
        YY--;
      }
      else {
        YY=2999;
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
