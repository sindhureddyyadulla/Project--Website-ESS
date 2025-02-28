#include <LSM6DS3.h>
#include <Wire.h>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <PCF8563.h> // RTC library
#include <SPI.h>
#include <SD.h>

#include "model.h" // Your trained model header file

const int numSamples = 119; // Number of samples for inference
const int chipSelect = 2;   // SD card chip select pin (adjust if needed)
const unsigned long interval = 1000; // 1 second interval in milliseconds

LSM6DS3 myIMU(I2C_MODE, 0x6A); // IMU object
PCF8563 pcf; // RTC object

// TensorFlow Lite global variables
tflite::MicroErrorReporter tflErrorReporter;
tflite::AllOpsResolver tflOpsResolver;
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));

// Define activity labels (5 activities from Phase II)
const char* GESTURES[] = {
  "Sitting",
  "Standing",
  "Walking",
  "Ascending stairs",
  "Descending stairs"
};
#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

unsigned long previousMillis = 0; // To track the last time inference was performed
int samplesRead = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Initialize IMU
  if (myIMU.begin() != 0) {
    Serial.println("IMU Device error");
  } else {
    Serial.println("IMU Device OK!");
  }

  // Initialize RTC
  Wire.begin();
  pcf.init();
  pcf.startClock();
  Serial.println("RTC Initialized");

  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Card failed, or not present");
    while (1); // Halt if SD card fails
  }
  Serial.println("SD Card initialized");

  // Initialize TensorFlow Lite
  tflModel = tflite::GetModel(model);
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);
  tflInterpreter->AllocateTensors();
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);

  // Write header to SD file
  File dataFile = SD.open("activity_log.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.println("Time,Activity,Confidence");
    dataFile.close();
  } else {
    Serial.println("opening SD file for header");
  }
}

void loop() {
  float aX, aY, aZ;
  unsigned long currentMillis = millis();

  // Collect samples continuously
  if (samplesRead < numSamples) {
    aX = myIMU.readFloatAccelX();
    aY = myIMU.readFloatAccelY();
    aZ = myIMU.readFloatAccelZ();

    // Store accelerometer data in input tensor
    tflInputTensor->data.f[samplesRead * 3 + 0] = aX;
    tflInputTensor->data.f[samplesRead * 3 + 1] = aY;
    tflInputTensor->data.f[samplesRead * 3 + 2] = aZ;

    samplesRead++;

    // Small delay to control sampling rate (adjust as needed)
    delay(8); // Approximately 119 samples in ~1 second (1000ms / 119 ≈ 8.4ms)
  }

  // Perform inference and log every second
  if (currentMillis - previousMillis >= interval && samplesRead == numSamples) {
    // Run inference
    TfLiteStatus invokeStatus = tflInterpreter->Invoke();
    if (invokeStatus != kTfLiteOk) {
      Serial.println("Invoke failed!");
      while (1);
      return;
    }

    // Get current time from RTC
    Time nowTime = pcf.getTime();

    // Find the gesture with the highest confidence
    int maxIndex = 0;
    float maxConfidence = tflOutputTensor->data.f[0];
    for (int i = 1; i < NUM_GESTURES; i++) {
      if (tflOutputTensor->data.f[i] > maxConfidence) {
        maxConfidence = tflOutputTensor->data.f[i];
        maxIndex = i;
      }
    }

    // Print to Serial for debugging
    Serial.print(nowTime.hour);
    Serial.print(":");
    Serial.print(nowTime.minute);
    Serial.print(":");
    Serial.print(nowTime.second);
    Serial.print(", ");
    Serial.print(GESTURES[maxIndex]);
    Serial.print(", Confidence: ");
    Serial.println(maxConfidence, 6);
    // Reset sample counter and update timing
    samplesRead = 0;
    previousMillis = currentMillis;
  }
}