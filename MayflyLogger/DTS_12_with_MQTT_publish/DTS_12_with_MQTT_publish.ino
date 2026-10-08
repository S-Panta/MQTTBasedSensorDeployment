#include <Arduino.h>
#include <ModularSensors.h>

#define XbeeSerial Serial1
#define XBEE_PWR 18

#include "Sodaq_DS3231.h"
#include <HydroServerMQTTClient.h>
#include <modems/ExpressifESP32.h>

const char *sketchName = "mqtt_publish_dts_12.ino";
const char *loggerID = "mayflylogger";
const int8_t loggingInterval = 10;

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
const char *MQTT_BROKER = "144.39.51.28";
const int32_t modemBaud = 57600;

ExpressifESP32 esp32(XbeeSerial, XBEE_PWR);
ExpressifESP32 &modem = esp32;
HydroServerMQTTClient mqttClient(*modem.createClient(), MQTT_BROKER);

#include <sensors/ProcessorStats.h>
const char *mcuBoardVersion = "v1.1";
ProcessorStats mcuBoard(mcuBoardVersion);

#include <sensors/MaximDS3231.h>
MaximDS3231 ds3231(1);

#include <sensors/DTS12.h>

const char DTS12SDI12address = '1';
const int8_t DTS12Power = sensorPowerPin;
const int8_t SDI12DataPin = 7;
const uint8_t DTS12NumberReadings = 1;

DTS12 dts12(DTS12SDI12address, DTS12Power, SDI12DataPin, DTS12NumberReadings);

Variable *variableList[] = {new ProcessorStats_SampleNumber(&mcuBoard),
                            new ProcessorStats_Battery(&mcuBoard),
                            new MaximDS3231_Temp(&ds3231),
                            new DTS12_Variance(&dts12),
                            new DTS12_Median_Turbidity(&dts12),
                            new DTS12_Temp(&dts12),
                            };

int variableCount = sizeof(variableList) / sizeof(variableList[0]);

VariableArray varArray;
Logger dataLogger;

Observation medianTurbidity   = { "medianturbidity","01a0f8c1-035d-7eb6-8125-35db98effc88", "DTS-12" };
Observation waterTemperature  = { "watertemp","01a0f8c3-a327-7f68-8143-b19ba3e327fd", "DTS-12"};
Observation varianceTurbidity = { "turbidityvariance","01a0f8c2-137a-7d61-86e1-fa278bda2be7", "DTS-12" };
Observation *observations[] = { &medianTurbidity, &waterTemperature, &varianceTurbidity };
uint8_t observationCount = sizeof(observations) / sizeof(observations[0]);

void publishCycle() {
  String ts = Logger::formatDateTime_ISO8601(Logger::markedLocalUnixTime);

  medianTurbidity.value   = variableList[4]->getValue();
  waterTemperature.value  = variableList[5]->getValue();
  varianceTurbidity.value = variableList[3]->getValue();

  uint8_t ok = mqttClient.publishAll(observations, observationCount, ts.c_str());
  if (ok = observationCount) {
    Serial.print(F("Data is published to the broker "));
  }
}

void setup() {
  Serial.begin(serialBaud);
  delay(1000);

  Serial.print(F("Now running "));
  // __FILE__ prints full path so need to extract filename from that path
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
    Serial.println("Cannot connect to Broker. Connection Error is ");
    Serial.println(mqttClient.getConnectionError());
  } else {
    Serial.println("Connected to MQTT broker");
  };

  Logger::setLoggerTimeZone(0);
  loggerClock::setRTCOffset(0);

  dataLogger.setLoggerPins(wakePin, sdCardSSPin, sdCardPwrPin, buttonPin,
                           greenLED);

  varArray.begin(variableCount, variableList);
  dataLogger.setStartupMeasurements(5);
  dataLogger.begin(loggerID, loggingInterval, &varArray);

  Serial.println(F("Setting up sensors..."));
  varArray.setupSensors();
  // dataLogger.setFileName("testdata.csv");
  // dataLogger.createLogFile(true);

  dataLogger.systemSleep();
  Serial.println("The logger is Ready to publish data via MQTT");
}

void loop() {
  if (dataLogger.checkInterval()) {
    Serial.println(Logger::formatDateTime_ISO8601(rtc.now().getEpoch()));
    Serial.println("Taking measurement and sending to broker");
    varArray.completeUpdate();
    // dataLogger.logToSD();
    
    if (!mqttClient.isConnected()) {
      Serial.println("MQTT disconnected, reconnecting...");
      if (!modem.isInternetAvailable()) {
        Serial.println(modem.connectToInternet(wifiId, wifiPwd) ? "WiFi OK" : "WiFi FAILED");
      }
    if (!mqttClient.connectToBroker()) {
      Serial.print("Broker connect failed, error: ");
      Serial.println(mqttClient.getConnectionError());
      }
    }

    publishCycle();
  }
  mqttClient.poll();
  dataLogger.systemSleep();
}