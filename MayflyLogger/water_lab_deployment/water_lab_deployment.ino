#include <Arduino.h>
#include <ModularSensors.h>

#define XbeeSerial Serial1
#define XBEE_PWR 18

#include "Sodaq_DS3231.h"
#include <HydroServerMQTTClient.h>
#include <modems/ExpressifESP32.h>

const char *sketchName = "mqtt_publish_dts12_cs451.ino";
const char *loggerID = "mayflylogger";
const int8_t loggingInterval = 5;

const int32_t serialBaud = 115200;
const int8_t greenLED = 8;
const int8_t redLED = 9;
const int8_t buttonPin = 21;
const int8_t wakePin = 31;
const int8_t sdCardPwrPin = -1;
const int8_t sdCardSSPin = 12;
const int8_t sensorPowerPin = 22;

const char *wifiId = "USU-guest";
const char *wifiPwd = 0;
const char *MQTT_BROKER = "144.39.174.90";
const int32_t modemBaud = 57600;

ExpressifESP32 esp32(XbeeSerial, XBEE_PWR);
ExpressifESP32 &modem = esp32;
HydroServerMQTTClient mqttClient(*modem.createClient(), MQTT_BROKER);

#include <sensors/ProcessorStats.h>
const char *mcuBoardVersion = "v1.1";
ProcessorStats mcuBoard(mcuBoardVersion);

#include <sensors/MaximDS3231.h>
MaximDS3231 ds3231(1);

const int8_t SDI12DataPin = 7;
const int8_t SDI12Power = sensorPowerPin;

#include <sensors/DTS12.h>
const char DTS12SDI12address = '1';
const uint8_t DTS12NumberReadings = 1;
DTS12 dts12(DTS12SDI12address, SDI12Power, SDI12DataPin, DTS12NumberReadings);

#include <sensors/CS451.h>
const char CS451SDI12address = '2';
const uint8_t CS451NumberReadings = 1;
CS451 cs451(CS451SDI12address, SDI12Power, SDI12DataPin, CS451NumberReadings);

// ---------- Variables ----------
Variable *dtsMedianTurb  = new DTS12_Median_Turbidity(&dts12);
Variable *dtsAverageTurb = new DTS12_Mean_Turbidity(&dts12);
Variable *dtsBESTurb     = new DTS12_BES_Turbidity(&dts12);
Variable *dtsMaxTurb     = new DTS12_Max_Turbidity(&dts12);
Variable *dtsMinTurb     = new DTS12_Min_Turbidity(&dts12);
Variable *dtsVariance    = new DTS12_Variance(&dts12);
Variable *dtsTemp        = new DTS12_Temp(&dts12);
Variable *csGageHeight   = new CS451_GageHeight(&cs451);
Variable *csTemp         = new CS451_Temp(&cs451);

Variable *variableList[] = {new ProcessorStats_SampleNumber(&mcuBoard),
                            new ProcessorStats_Battery(&mcuBoard),
                            new MaximDS3231_Temp(&ds3231),
                            dtsMedianTurb,
                            dtsAverageTurb,
                            dtsBESTurb,
                            dtsMaxTurb,
                            dtsMinTurb,
                            dtsVariance,
                            dtsTemp,
                            csGageHeight,
                            csTemp,
                            };

int variableCount = sizeof(variableList) / sizeof(variableList[0]);

VariableArray varArray;
Logger dataLogger;

Observation medianTurbidity   = { "medianturbidity",   "01a136fa-33c0-7cde-a1fb-4213a9451acf", "dts12" };
Observation averageTurbidity  = { "averageturbidity",  "01a136fc-9f31-7f1d-93c5-3ea786c3dc9e", "dts12" };
Observation besTurbidity      = { "besturbidity",      "01a136fe-3b40-778f-b8b0-c96cf50c5c95", "dts122" };
Observation maxTurbidity      = { "maxturbidity",      "01a13706-6291-73cf-9d31-f99b79de8747", "dts12" };
Observation minTurbidity      = { "minturbidity",      "01a13706-ba9d-703d-bf2c-ef168763b106", "dts12" };
Observation varianceTurbidity = { "turbidityvariance", "01a13704-a23e-7dd0-9ac5-ab5ff13bd6d1", "dts12" };
Observation gageHeight        = { "gageHeight",        "01a136f6-3387-7eca-b22f-69ce4557dff1", "cs451" };
Observation waterTemperature  = { "watertemp",         "01a13708-db93-70d5-ab36-e4c0ace6925e", "cs451" };

Observation *observations[] = { &medianTurbidity, &averageTurbidity, &besTurbidity,
                                &maxTurbidity, &minTurbidity, &varianceTurbidity,
                                &gageHeight, &waterTemperature };
uint8_t observationCount = sizeof(observations) / sizeof(observations[0]);

void publishCycle() {
  String ts = Logger::formatDateTime_ISO8601(Logger::markedLocalUnixTime);

  medianTurbidity.value   = dtsMedianTurb->getValue();
  averageTurbidity.value  = dtsAverageTurb->getValue();
  besTurbidity.value      = dtsBESTurb->getValue();
  maxTurbidity.value      = dtsMaxTurb->getValue();
  minTurbidity.value      = dtsMinTurb->getValue();
  varianceTurbidity.value = dtsVariance->getValue();
  gageHeight.value        = csGageHeight->getValue();
  waterTemperature.value  = csTemp->getValue();

  uint8_t ok = mqttClient.publishAll(observations, observationCount, ts.c_str());
  if (ok == observationCount) {
    Serial.println(F("Data is published to the broker"));
  } else {
    Serial.print(F("Publishing incomplete: "));
    Serial.print(ok);
    Serial.print(F(" of "));
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

  if (!modem.connectToInternet(wifiId, wifiPwd)) {
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
  mqttClient.setSiteCode("uwrl");
  mqttClient.setClientID(loggerID);
  mqttClient.setLastWill("Mayfly Logger shutting down");
  mqttClient.setKeepAliveInterval(660);

  if (!mqttClient.connectToBroker()) {
    Serial.println(F("Cannot connect to Broker. Connection Error is "));
    Serial.println(mqttClient.getConnectionError());
  } else {
    Serial.println(F("Connected to MQTT broker"));
  }

  Logger::setLoggerTimeZone(0);
  loggerClock::setRTCOffset(0);

  dataLogger.setLoggerPins(wakePin, sdCardSSPin, sdCardPwrPin, buttonPin,
                           greenLED);

  varArray.begin(variableCount, variableList);
  // delete this line before deployment
  dataLogger.setStartupMeasurements(5);
  dataLogger.begin(loggerID, loggingInterval, &varArray);

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
    dts12.update();
    cs451.powerUp();
    cs451.update();
    dts12.powerDown();
    cs451.powerDown();

    mcuBoard.update();
    ds3231.update();

    if (!mqttClient.isConnected()) {
      Serial.println(F("MQTT disconnected, reconnecting..."));
      if (!modem.isInternetAvailable()) {
        Serial.println(modem.connectToInternet(wifiId, wifiPwd) ? "WiFi OK" : "WiFi FAILED");
      }
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