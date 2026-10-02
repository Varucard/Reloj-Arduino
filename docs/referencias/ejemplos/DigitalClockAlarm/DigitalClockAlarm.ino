/*  Digital Clock with Alarm Function
 *  More info: http://www.ardumotive.com/
 *  Dev: Michalis Vasilakis Data: 19/11/2016 Ver: 1.0
 *  
 *  Display 16x2:         Setup:            Setup Alarm
 *  +----------------+  +----------------+ +----------------+ 
 *  |HH:MM:SS|* HH:MM|  |    >HH :>MM    | |   Set Alarm    |
 *  |DD/MM/YY|  ALARM|  |>DD />MM />YYYY | |   >HH :>MM     |
 *  +----------------+  +----------------+ +----------------+
 */

//Libraries
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>

//Connections and constants 
LiquidCrystal lcd(8,7,6,5,4,3); //LCD
RTC_DS1307 rtc; //DS1307 i2c
char daysOfTheWeek[7][12] = {"Sunday","Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int btSet = A0;
const int btAdj = A1;
const int btAlarm = A2;
const int buzzer = 11;
const int shakeSensor = A3;
long interval = 300;  
int melody[] = { 600, 800, 1000,1200 };

//Variables
int DD,MM,YY,H,M,S,set_state, adjust_state, alarm_state,AH,AM, shake_state;
int shakeTimes=0;
int i =0;
int btnCount = 0;
String sDD;
String sMM;
String sYY;
String sH;
String sM;
String sS;
String aH="12";
String aM="00";
String alarm = "     ";
long previousMillis = 0;    

//Boolean flags
boolean setupScreen = false;
boolean alarmON=false;
boolean turnItOn = false;
   
void setup() {
  //Init RTC and LCD library items
  rtc.begin();
  lcd.begin(16,2);
  //Set outputs/inputs
  pinMode(btSet,INPUT_PULLUP);
  pinMode(btAdj,INPUT_PULLUP);
  pinMode(btAlarm, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  //Check if RTC has a valid time/date, if not set it to 00:00:00 01/01/2018.
  //This will run only at first time or if the coin battery is low.
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // This line sets the RTC with an explicit date & time, for example to set
    // January 1, 2018 at 00:00am you would call:
    rtc.adjust(DateTime(2018, 01, 01, 00, 00, 0)); 
  }
  delay(100);
  //Read alarm time from EEPROM memmory
  AH=EEPROM.read(0);
  AM=EEPROM.read(1);
  //Check if the numbers that you read are valid. (Hours:0-23 and Minutes: 0-59)
  if (AH>23){
    AH=0;
  }
  if (AM>59){
    AM=0;
  }
}

void loop() {
  readBtns();       //Read buttons 
  getTimeDate();    //Read time and date from RTC
  if (!setupScreen){
    lcdPrint();     //Normanlly print the current time/date/alarm to the LCD
    if (alarmON){
     callAlarm();   // and check the alarm if set on
    }
  }
  else{
    timeSetup();    //If button set is pressed then call the time setup function
  }
  
}

/*************** Functions ****************/
//Read buttons state
void readBtns(){
  set_state = digitalRead(btSet);
  adjust_state = digitalRead(btAdj);
  alarm_state = digitalRead(btAlarm);
  if(!setupScreen){
    if (alarm_state==LOW){
      if (alarmON){
        alarm="     ";
        alarmON=false;
      }
      else{
        alarm="ALARM";
        alarmON=true;
      }
      delay(500);
    }
  }
  if (set_state==LOW){
    if(btnCount<7){
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
      rtc.adjust(DateTime(YY, MM, DD, H, M, 0)); //Save time and date to RTC IC
      EEPROM.write(0, AH);  //Save the alarm hours to EEPROM 0
      EEPROM.write(1, AM);  //Save the alarm minuted to EEPROM 1
      lcd.print("Saving....");
      delay(2000);
      lcd.clear();
      setupScreen = false;
      btnCount=0;
    }
    delay(500);
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
  sYY=YY-2000;
  if (H<10){ sH = '0' + String(H); } else { sH = H; }
  if (M<10){ sM = '0' + String(M); } else { sM = M; }
  if (S<10){ sS = '0' + String(S); } else { sS = S; }
  if (AH<10){ aH = '0' + String(AH); } else { aH = AH; }
  if (AM<10){ aM = '0' + String(AM); }  else { aM = AM; }
}
//Print values to the display
void lcdPrint(){
  String line1 = sH+":"+sM+":"+sS+" | "+aH+":"+aM;
  String line2 = sDD+"/"+sMM+"/"+sYY +" | "+alarm;
  lcd.setCursor(0,0); //First row
  lcd.print(line1);
  lcd.setCursor(0,1); //Second row
  lcd.print(line2);  
}

//Setup screen
void timeSetup(){
  int up_state = adjust_state;
  int down_state = alarm_state;
  if(btnCount<=5){
    if (btnCount==1){         //Set Hour
      lcd.setCursor(4,0);
      lcd.print(">"); 
      if (up_state == LOW){   //Up button +
        if (H<23){
          H++;
        }
        else {
          H=0;
        }
        delay(350);
      }
      if (down_state == LOW){ //Down button -
        if (H>0){
          H--;
        }
        else {
          H=23;
        }
        delay(350);
      }
    }
    else if (btnCount==2){      //Set  Minutes
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
        delay(350);
      }
      if (down_state == LOW){
        if (M>0){
          M--;
        }
        else {
          M=59;
        }
        delay(350);
      }
    }
    else if (btnCount==3){      //Set Day
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
        delay(350);
      }
      if (down_state == LOW){
        if (DD>1){
          DD--;
        }
        else {
          DD=31;
        }
        delay(350);
      }
    }
    else if (btnCount==4){      //Set Month
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
        delay(350);
      }
      if (down_state == LOW){
        if (MM>1){
          MM--;
        }
        else {
          MM=12;
        }
        delay(350);
      }
    }
    else if (btnCount==5){      //Set Year
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
        delay(350);
      }
      if (down_state == LOW){
        if (YY>2018){
          YY--;
        }
        else {
          YY=2999;
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
  }
  else{
    setAlarmTime();
  }
}

//Set alarm time
void setAlarmTime(){
  int up_state = adjust_state;
  int down_state = alarm_state;
  String line2;
  lcd.setCursor(0,0);
  lcd.print("SET  ALARM TIME");
  if (btnCount==6){             //Set alarm Hour
    if (up_state == LOW){
      if (AH<23){
        AH++;
      }
      else {
        AH=0;
      }
      delay(350);
    }
    if (down_state == LOW){
      if (AH>0){
        AH--;
      }
      else {
        AH=23;
      }
      delay(350);
    }
    line2 = "    >"+aH+" : "+aM+"    ";
  }
  else if (btnCount==7){        //Set alarm Minutes
    if (up_state == LOW){
      if (AM<59){
        AM++;
      }
      else {
        AM=0;
      }
      delay(350);
    }
    if (down_state == LOW){
      if (AM>0){
        AM--;
      }
      else {
        AM=59;
      }
      delay(350);
    }
    line2 = "     "+aH+" :>"+aM+"    ";    
  }
  lcd.setCursor(0,1);
  lcd.print(line2);
}

void callAlarm(){
  if (aM==sM && aH==sH && S>=0 && S<=2){
    turnItOn = true;
  }
  if(alarm_state==LOW || shakeTimes>=6 || (M==(AM+5))){
    turnItOn = false;
    alarmON=true;
    delay(500);
  } 
  if(analogRead(shakeSensor)>200){
    shakeTimes++;
    Serial.print(shakeTimes);
    delay(50);
  }
  if (turnItOn){
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;   
      tone(buzzer,melody[i],100);
      i++; 
      if(i>3){i=0; };
    }
  }
  else{
    noTone(buzzer);
    shakeTimes=0;
  }
}
