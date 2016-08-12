#include <ESP8266WiFi.h>
#include <wifihelper.h>
#include <dht11sensor.h>
#include <ESP8266HTTPClient.h>

extern "C" {
  #include "user_interface.h"
}

#define _pin  D1
#define sleepTimeS 60 * 15
#define maxWifiRetryCount 10

ADC_MODE(ADC_VCC);

struct {
  unsigned int sleepCount;
  unsigned int sensorError;
} rtcData;

String apiKey = "";
const char* ssid = "";
const char* password = "";
const char* server = "api.thingspeak.com";

DHT11 dht11(_pin);

void configureWifi()
{
  // Faster connection with static IP
  IPAddress ip(192, 168, 203, 250);
  IPAddress gateway(192, 168, 203, 254);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  checkWifiConnection(maxWifiRetryCount);
}

void sendToThingSpeak(DHT11Result dhtResult)
{
  HTTPClient httpClient;
  httpClient.begin("http://api.thingspeak.com/update");
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  httpClient.addHeader("X-THINGSPEAKAPIKEY", apiKey);

  String postStr = apiKey;
  postStr +="&field1=";
  postStr += String(dhtResult.temperatureC);
  postStr +="&field2=";
  postStr += String(dhtResult.humidity);
  postStr +="&field3=";
  postStr += String(ESP.getVcc());
  postStr +="&field4=";
  postStr += String(rtcData.sensorError);
  postStr +="&status=";
  postStr += "Running for " + String(rtcData.sleepCount) + " cycles";

  int httpResult = httpClient.POST(postStr);
  String resultString = httpClient.getString();
  httpClient.end();
  if (httpResult == HTTP_CODE_OK)
  {
    Serial.println("Data sent to Thingspeak: " + resultString);
  }
  else {
    Serial.printf("Cannot connect to Thingspeak server: %s - %s\n", HTTPClient::errorToString(httpResult).c_str(), resultString.c_str());
  }
}

int getResetReason() {
  rst_info* ri = system_get_rst_info();
  if (ri == NULL)
    return -1;

  return ri->reason;
}

void writeRtcMemory()
{
  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData));
  Serial.println("Globalsession object is written to Rtc mem");
}

void readRtcMemory()
{
  if (getResetReason() == REASON_DEEP_SLEEP_AWAKE) {
    ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData));
    Serial.println("Globalsession object is read from Rtc mem");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(1);
  Serial.println();
  Serial.println();
  Serial.println();
  dht11.initialize();
  readRtcMemory();
  configureWifi();
  Serial.println("Initialized..");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED)
  {
    delay(2000);
    DHT11Result result = dht11.read();
    if (result.hasError == true)
    {
      rtcData.sensorError++;
      Serial.println("Sensor error: " + result.errorMessage);
    }
    else
    {
      Serial.printf("Temperature: %d C - Humidity: %d%%\n", result.temperatureC, result.humidity);
      sendToThingSpeak(result);
    }
  }

  rtcData.sleepCount++;

  writeRtcMemory();

  Serial.println("Sleeping for " + String(sleepTimeS) + " seconds..");

  ESP.deepSleep(sleepTimeS * 1000000);
}
