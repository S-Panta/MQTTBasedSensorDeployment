/** =========================================================================
 * This is code for Mayfly logger that is deployed at the Utah Water Research Lab that sends observation through MQTT protocol
 * Author: Sabin Panta <sabin.panta@usu.edu>
 * Build Environment: Arduino IDE Version 2.3.6
 * Hardware Platform: EnviroDIY Mayfly Arduino Datalogger V1.1
 * ======================================================================= */
#include <Arduino.h>


#define XbeeSerial Serial1
#define XBEE_PWR 18

#include "Sodaq_DS3231.h"
#include "arduino_secrets.h"
#include <HydroServerMQTTClient.h>
#include <modems/ESP32Bee.h>
#include <ModularSensors.h>


#include <sensors/ProcessorStats.h>
#include <sensors/MaximDS3231.h>
#include <sensors/DTS12.h>
#include <sensors/CS451.h>

const char *loggerID = "mayflylogger";
const int8_t loggingInterval = 15;

const int32_t serialBaud = 115200;
const int8_t greenLED = 8;
const int8_t redLED = 9;
const int8_t buttonPin = 21;
const int8_t wakePin = 31;
const int8_t sdCardPwrPin = -1;
const int8_t sdCardSSPin = 12;
const int8_t sensorPowerPin = 22;

const char *ssid = SSID;
const char *pass = PASSWORD;
const char *MQTT_BROKER = BROKERHOST;

ESP32Bee esp32(XbeeSerial, XBEE_PWR);
ESP32Bee &modem = esp32;
const int32_t modemBaud = 57600;
HydroServerMQTTClient mqttClient(*modem.createClient(), MQTT_BROKER);


const char *mcuBoardVersion = "v1.1";
ProcessorStats mcuBoard(mcuBoardVersion);
MaximDS3231 ds3231(1);

const int8_t SDI12DataPin = 7;
const int8_t SDI12Power = sensorPowerPin;

const char DTS12SDI12address = '1';
const uint8_t DTS12NumberReadings = 1;
DTS12 dts12(DTS12SDI12address, SDI12Power, SDI12DataPin, DTS12NumberReadings);

const char CS451SDI12address = '2';
const uint8_t CS451NumberReadings = 1;
CS451 cs451(CS451SDI12address, SDI12Power, SDI12DataPin, CS451NumberReadings);

Variable *medianTurb  = new DTS12_Median_Turbidity(&dts12);
Variable *averageTurb = new DTS12_Mean_Turbidity(&dts12);
Variable *besTurb     = new DTS12_BES_Turbidity(&dts12);
Variable *maxTurb     = new DTS12_Max_Turbidity(&dts12);
Variable *minTurb     = new DTS12_Min_Turbidity(&dts12);
Variable *turbVariance    = new DTS12_Variance(&dts12);
Variable *dts12WaterTemp        = new DTS12_Temp(&dts12);
Variable *cs451GageHeight   = new CS451_GageHeight(&cs451);
Variable *cs451WaterTemp         = new CS451_Temp(&cs451);

Variable *variableList[] = {new ProcessorStats_SampleNumber(&mcuBoard),
                            new ProcessorStats_Battery(&mcuBoard),
                            new MaximDS3231_Temp(&ds3231),
                            medianTurb,
                            averageTurb,
                            besTurb,
                            maxTurb,
                            minTurb,
                            turbVariance,
                            dts12WaterTemp,
                            cs451GageHeight,
                            cs451WaterTemp,
                            };

int variableCount = sizeof(variableList) / sizeof(variableList[0]);

VariableArray varArray;
Logger dataLogger;

Observation medianTurbidity   = { "medianturbidity",   "01a136fa-33c0-7cde-a1fb-4213a9451acf", "dts12" };
Observation averageTurbidity  = { "averageturbidity",  "01a136fc-9f31-7f1d-93c5-3ea786c3dc9e", "dts12" };
Observation besTurbidity      = { "besturbidity",      "01a136fe-3b40-778f-b8b0-c96cf50c5c95", "dts12" };
Observation maxTurbidity      = { "maxturbidity",      "01a13706-6291-73cf-9d31-f99b79de8747", "dts12" };
Observation minTurbidity      = { "minturbidity",      "01a13706-ba9d-703d-bf2c-ef168763b106", "dts12" };
Observation varianceTurbidity = { "turbidityvariance", "01a13704-a23e-7dd0-9ac5-ab5ff13bd6d1", "dts12" };
Observation dts12waterTemperature = {"watertemp","01a159e9-9f30-7021-a0cf-7c691e3fc2ab","dts12"};
Observation gageHeight        = { "gageheight",        "01a136f6-3387-7eca-b22f-69ce4557dff1", "cs451" };
Observation waterTemperature  = { "watertemp",         "01a13708-db93-70d5-ab36-e4c0ace6925e", "cs451" };

Observation *observations[] = { &medianTurbidity, &averageTurbidity, &besTurbidity,
                                &maxTurbidity, &minTurbidity, &varianceTurbidity, &dts12waterTemperature,
                                &gageHeight, &waterTemperature };
uint8_t observationCount = sizeof(observations) / sizeof(observations[0]);

void publishCycle() {
  String ts = Logger::formatDateTime_ISO8601(Logger::markedLocalUnixTime);

  medianTurbidity.value   = medianTurb->getValue();
  averageTurbidity.value  = averageTurb->getValue();
  besTurbidity.value      = besTurb->getValue();
  maxTurbidity.value      = maxTurb->getValue();
  minTurbidity.value      = minTurb->getValue();
  varianceTurbidity.value = turbVariance->getValue();
  dts12waterTemperature.value = dts12WaterTemp->getValue();
  gageHeight.value        = cs451GageHeight->getValue();
  waterTemperature.value  = cs451WaterTemp->getValue();

  uint8_t publishItems = mqttClient.publishAll(observations, observationCount, ts.c_str());
  if (publishItems == observationCount) {
    Serial.println(F("Data is published to the broker"));
  } else {
    Serial.print(F("Only published"));
    Serial.print(publishItems);
    Serial.print(F(" out of "));
    Serial.println(observationCount);
  }
}

void setup() {
  Serial.begin(serialBaud);
  delay(1000);

  Serial.print(F("Now running "));
  Serial.println(__builtin_strrchr(__FILE__, '/') + 1);
  Serial.print(F(" on Logger "));
  Serial.println(loggerID);

  pinMode(greenLED, OUTPUT);
  digitalWrite(greenLED, LOW);
  pinMode(redLED, OUTPUT);
  digitalWrite(redLED, LOW);

  XbeeSerial.begin(modemBaud);
  modem.powerUp();
  delay(1000);
  Serial.println(F("Powered up Esp32 Bee Wifi modem"));

  if (!modem.connectToInternet(ssid, pass)) {
    Serial.println(F("WiFi is not connected"));
  } else {
    Serial.println(F("WiFi is connected"));
    delay(2000);
    uint32_t unixTime = modem.getNISTTime();
    rtc.begin();
    rtc.setDateTime(unixTime);
    Serial.println(F("RTC synced from NIST time server"));
  }
  modem.extraSetupForMQTT();

  Logger::setLoggerTimeZone(0);
  loggerClock::setRTCOffset(0);

  mqttClient.setSiteCode("uwrl");
  mqttClient.setClientID(loggerID);
  mqttClient.setLastWill("Mayfly Logger shutting down");
  mqttClient.setKeepAliveInterval(900);
  if (!mqttClient.connectToBroker()) {
    Serial.println(F("Cannot connect to Broker. Connection Error is "));
    Serial.println(mqttClient.getConnectionError());
  } else {
    Serial.println(F("Connected to MQTT broker"));
  }

  dataLogger.setLoggerPins(wakePin, sdCardSSPin, sdCardPwrPin, buttonPin,
                           greenLED);

  varArray.begin(variableCount, variableList);
  // keep the startup measurement value to 0 on deployment
  dataLogger.setStartupMeasurements(0);
  dataLogger.begin(loggerID, loggingInterval, &varArray);
  dataLogger.setFileName("waterlab_deployment.csv");
  dataLogger.createLogFile(true);
  Serial.println(F("Setting up sensors..."));
  varArray.setupSensors();

  dataLogger.systemSleep();
  Serial.println(F("The logger is ready to publish data via MQTT"));
}

void loop() {
  if (dataLogger.checkInterval()) {
    Serial.println(Logger::formatDateTime_ISO8601(rtc.now().getEpoch()));
    Serial.println(F("Taking measurements from DTS-12 and CS451 and sending to broker"));
    // Instead of varArray.completeUpdate(), we want to have control over which sensor runs first.
    // DTS 12 startup time is more because at every start the wiper has to run 
    dts12.powerUp();
    cs451.powerUp();
    dts12.update();
    cs451.update();
    dts12.powerDown();
    cs451.powerDown();

    mcuBoard.update();
    ds3231.update();
    dataLogger.logToSD();
    if (!modem.isInternetAvailable()) {
        Serial.println(modem.connectToInternet(ssid, pass) ? "WiFi OK" : "WiFi FAILED");
    }
    if (!mqttClient.isConnected()) {
      if (!mqttClient.connectToBroker()) {
        Serial.print(F("Broker connect failed, error: "));
        Serial.println(mqttClient.getConnectionError());
      }
    }

    publishCycle();
  }
  mqttClient.poll();
  dataLogger.systemSleep();
}