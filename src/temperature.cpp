#include <ESP8266WiFi.h>
#include <wifihelper.h>
#include <dht11sensor.h>

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
WiFiClient client;
DHT11 dht11(_pin);

void sendToThingSpeak(DHT11Result dhtResult)
{
  if (client.connect(server, 80)) { // "184.106.153.149" or api.thingspeak.com
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
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    client.stop();
    Serial.println("Data sent to Thingspeak");
  }
  else {
    Serial.println("Cannot connect to Thingspeak server");
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
  Serial.println();
  Serial.println();
  Serial.println();
  dht11.initialize();
  readRtcMemory();
  WiFi.begin(ssid, password);
  checkWifiConnection(maxWifiRetryCount);
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
