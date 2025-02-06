#include <LSM6DS3.h>
#include <PDM.h>
#include <Arduino.h>
#include <U8x8lib.h>
#include <Wire.h>
#include <ArduinoBLE.h>
#include <PCF8563.h>
#include <SPI.h>
#include <SD.h>

// Constants
const float LOW_BATTERY_THRESHOLD = 3.2;
const unsigned long LOG_INTERVAL = 1000;
const int VBAT_PIN = A0;
const int SD_CHIP_SELECT = 2;

// Voltage divider resistors
const float R1 = 330.0;
const float R2 = 1000.0;
const float ADC_REF = 3.3;

// Hardware Objects
LSM6DS3 myIMU(I2C_MODE, 0x6A);
PCF8563 rtc;
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(PIN_WIRE_SCL, PIN_WIRE_SDA);
BLEService dataService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEByteCharacteristic systemControl("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

// Global Variables
volatile int samplesRead;
short sampleBuffer[512];
bool systemActive = false;
bool lowBattery = false;
unsigned long lastLogTime = 0;
File logFile;

// Function declarations
void initializeSensors();
void initializeRTC();
void initializeOLED();
void initializeBLE();
void initializeSDCard();
void updateSystemState();
void updateBatteryStatus(float voltage);
bool shouldLogData();
void logSensorData();
bool openLogFile();
float readBatteryVoltage();
void updateDisplay(float batteryVoltage);
void onPDMdata();

void setup() {
  Serial.begin(9600);
  while (!Serial);

  initializeSensors();
  initializeRTC();
  initializeOLED();
  initializeBLE();
  initializeSDCard();
}

void loop() {
  BLE.poll();
  updateSystemState();
  
  if (millis() - lastLogTime >= LOG_INTERVAL) {
    float batteryVoltage = readBatteryVoltage();
    updateBatteryStatus(batteryVoltage);
    updateDisplay(batteryVoltage);
    
    if (shouldLogData()) {
      logSensorData();
    }
    
    lastLogTime = millis();
  }
}

void initializeSensors() {
  if (myIMU.begin() != 0) {
    Serial.println("IMU Error!");
    while(1);
  }

  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("MIC Error!");
    while(1);
  }
}

void initializeRTC() {
  rtc.init();
  rtc.stopClock();
  rtc.setYear(25);
  rtc.setMonth(2);
  rtc.setDay(5);
  rtc.setHour(2);
  rtc.setMinut(0);
  rtc.setSecond(0);
  rtc.startClock();
}

void initializeOLED() {
  oled.begin();
  oled.setFlipMode(1);
  oled.setFont(u8x8_font_chroma48medium8_r);
}

void initializeBLE() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (!BLE.begin()) {
    Serial.println("BLE Error!");
    while(1);
  }

  BLE.setLocalName("Deepak & Sindhu");
  BLE.setAdvertisedService(dataService);
  dataService.addCharacteristic(systemControl);
  BLE.addService(dataService);
  systemControl.writeValue(0);
  BLE.advertise();
}

void initializeSDCard() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD Card Error!");
    while(1);
  }
}

void updateSystemState() {
  if (systemControl.written()) {
    if (systemControl.value() && !lowBattery) {
      systemActive = true;
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      systemActive = false;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

void updateBatteryStatus(float voltage) {
  lowBattery = (voltage < LOW_BATTERY_THRESHOLD);
  if (lowBattery) systemActive = false;
}

bool shouldLogData() {
  return systemActive && !lowBattery && (logFile || openLogFile());
}

void logSensorData() {
  char logBuffer[256];
  Time now = rtc.getTime();
  
  snprintf(logBuffer, sizeof(logBuffer),
    "%04d-%02d-%02d %02d:%02d:%02d,%.2f,%d,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
    now.year + 2000, now.month, now.day,
    now.hour, now.minute, now.second,
    readBatteryVoltage(),
    samplesRead ? sampleBuffer[0] : 0,
    myIMU.readTempC(),
    myIMU.readFloatAccelX(),
    myIMU.readFloatAccelY(),
    myIMU.readFloatAccelZ(),
    myIMU.readFloatGyroX(),
    myIMU.readFloatGyroY(),
    myIMU.readFloatGyroZ()
  );

  logFile.println(logBuffer);
  logFile.flush();
  samplesRead = 0;
}

bool openLogFile() {
  logFile = SD.open("datalog.csv", FILE_WRITE);
  if (logFile) {
    if (logFile.size() == 0) {
      logFile.println("DateTime,BatteryV,Microphone,TempC,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ");
    }
    return true;
  }
  return false;
}

float readBatteryVoltage() {
  int raw = analogRead(VBAT_PIN);
  float voltage = raw * (ADC_REF / 1023.0) * ((R1 + R2) / R2);
  return voltage;
}

void updateDisplay(float batteryVoltage) {
  oled.clear();
  
  oled.setCursor(0, 0);
  oled.print("Bat: ");
  oled.print(batteryVoltage, 1);
  oled.print("V");

  oled.setCursor(0, 1);
  if (lowBattery) {
    oled.print("LOW BATTERY!");
  } else {
    oled.print("System: ");
    oled.print(systemActive ? "ON " : "OFF");
  }
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}
