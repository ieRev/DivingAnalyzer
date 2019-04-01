#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>

#include <uRTCLib.h>
#include <Battery.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <EEPROM.h>
#include <RunningAverage.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>
#include "battery.h"
#define RA_SIZE 20
#define MINVOLTAGE 3000
#define MAXVOLTAGE 4200

int pinDHT22 = 2;
int batterySensePin = A0;
int chargeStatusPin = 5;
int usbStatusPin = 6;
double calibrationV = 0;
unsigned long previousMillis = 0;
const long interval = 5000;
bool flash = true;
const int ledPin = LED_BUILTIN;
int batteryLevel = 0;
float temperature = 0;
float humidity = 0;

Adafruit_ADS1115 ads1115(0x48);
U8G2_SSD1327_MIDAS_128X128_1_4W_SW_SPI u8g2(U8G2_R0, 13, 11, 10, 9, 8); //U8G2(Rotation, Clock, Data, CS, DC, Reset)
Battery battery(MINVOLTAGE, MAXVOLTAGE, batterySensePin);
uRTCLib rtc(0x68);
DHT_Unified dht(pinDHT22, DHT22);

RunningAverage RA(RA_SIZE);
RunningAverage batVoltage(RA_SIZE);


void EEPROMWriteInt(int p_address, int p_value) {
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);

  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

unsigned int EEPROMReadInt(int p_address) {
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);

  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

void read_O2sensor(int adc=0) {  
  int16_t millivolts = 0;
  millivolts = ads1115.readADC_Differential_0_1();
  RA.addValue(millivolts);

}

int calibrate(int x) {

  Serial.print("Starting Calibration....\n");
  
  double result;  
  
  for(int cx=0; cx<= RA_SIZE; cx++) {
    delay(50);
    read_O2sensor(0);
  }
  result = RA.getAverage();
  result = abs(result);

  EEPROMWriteInt(x, result);
  
  Serial.print("Calibration Value = ");
  Serial.println(result);
}

void updateDisplay(float percentageO2, int battery) {

  u8g2.firstPage();
  do {
//    switch (battery) {
//      case 0:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, battery0);
//        break;
//      case 1:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, battery0);
//        break;
//      case 2:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, battery0);
//        break;
//      case 3:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, battery0);
//        break;      
//      case 4:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, battery0);
//        break;
//      case 5:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, pluggedInCharging);
//        break;
//      case 6:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, pluggedInCharged);
//        break;
//      default:
//        u8g2.drawXBMP(105, 0, battery_width, battery_height, batteryBroken);
//        break;
//    }

    u8g2.setFont(u8g2_font_profont10_mr);

    u8g2.setFontMode(0);
    u8g2.setDrawColor(1);
    u8g2.setCursor(0,12);
    u8g2.print("Temp: ");
    u8g2.print(temperature, 1);
    u8g2.print("C");
    u8g2.print("  Hum: ");
    u8g2.print(humidity, 1);
    u8g2.print("%");
    
    if (battery != -1) {
      u8g2.setDrawColor(0);
      u8g2.setCursor(108,12);
      u8g2.print(batteryLevel);
      u8g2.print("%");
    }
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenR24_tr);
    u8g2.setCursor(0, 50);
    u8g2.print("O2 %:");
    u8g2.setCursor(0,90);
    u8g2.print(percentageO2 , 1);

    u8g2.setFontMode(0);
    u8g2.setDrawColor(1);
    u8g2.setCursor(0,128);
    
//    u8g2.setFont(u8g2_font_chikita_tr);
    u8g2.setFont(u8g2_font_profont10_mr);
    if (rtc.hour() < 10) {
      u8g2.print("0");
    }
    u8g2.print(rtc.hour());
    u8g2.print(":");
    if (rtc.minute() < 10) {
      u8g2.print("0");
    }
    u8g2.print(rtc.minute());
    u8g2.print(":");
    if (rtc.second() < 10) {
      u8g2.print("0");
    }
    u8g2.print(rtc.second());    

    u8g2.setCursor(64,128);
    u8g2.print(rtc.day());
    u8g2.print("/");
    u8g2.print(rtc.month());
    u8g2.print("/");
    u8g2.print(rtc.year());
    
  } while ( u8g2.nextPage() );

  
}

int getChargeStatus(void) {

  int batteryVoltage = 0;
  int charging = 0;
  int usbPower = 0;
  batteryLevel = battery.level();
  batteryVoltage = battery.voltage();

  Serial.print("Battery Level: ");
  Serial.print(battery.level());
  Serial.print(" Voltage: ");
  Serial.println(battery.voltage());
  
  charging = digitalRead(chargeStatusPin);
  usbPower = digitalRead(usbStatusPin);

  if ( batteryVoltage < MINVOLTAGE || batteryVoltage > MAXVOLTAGE ) {
     return -1;
  }
  
  if ( usbPower && charging ) {
    return 5;
  }
  else if ( usbPower && !charging) {
    return 6;
  }
  else {
    if ( batteryLevel >= 0 && batteryLevel < 10 )
    {
      return 0;
    }
    else if (batteryLevel >= 10 && batteryLevel < 25)
    {
      return 1;
    }
    else if (batteryLevel >= 25 && batteryLevel < 50)
    {
      return 2;
    }
    else if (batteryLevel >=50 && batteryLevel < 75)
    {
      return 3;
    }
    else 
    { 
      return 4;
    }

    
  }
  
}
void setup(void)
{
  Serial.begin(115200);
  
  ads1115.setGain(GAIN_SIXTEEN);
  ads1115.begin();
  u8g2.begin();
  battery.begin(5000, 1.0, &sigmoidal);
  RA.clear();
  Wire.begin();
  dht.begin();
  
  pinMode(batterySensePin, INPUT);
  pinMode(chargeStatusPin, INPUT);
  pinMode(usbStatusPin, INPUT);
  pinMode(ledPin, OUTPUT);
  calibrate(0);
  calibrationV = EEPROMReadInt(0); 

    sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));


  
}
void getWeather(void){
  
  if (rtc.second() % 5 == 0){
    
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    }
    else {
      temperature = event.temperature;
      Serial.print(F("Temperature: "));
      Serial.print(temperature);
      Serial.println(F("°C"));
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    }
    else {
      humidity = event.relative_humidity;
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.println(F("%"));
    }
  }
}

void loop (void) {

  int currentmV = 0;
  double percentageO2 = 0;
  int batteryState = 0;

  rtc.refresh();
  batteryState = getChargeStatus();
  getWeather();
  read_O2sensor(0);
  currentmV = ads1115.readADC_Differential_0_1();
  currentmV - abs(currentmV);
  
  percentageO2 = (currentmV / calibrationV) * 20.9;

  updateDisplay(percentageO2, batteryState);
  
}
