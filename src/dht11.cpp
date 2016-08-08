#include <ESP8266WiFi.h>
extern "C" {
  #include "user_interface.h"
}

ADC_MODE(ADC_VCC);

unsigned int sleepCount = 0;
int sleepCountOffset = 0;

unsigned int sensorWakeUpError = 0;
int sensorWakeUpErrorOffset = sleepCountOffset + sizeof(sleepCount);

unsigned int sensorParityError = 0;
int sensorParityErrorOffset = sensorWakeUpErrorOffset + sizeof(sensorWakeUpError);

String apiKey = "XXXXXX";
const char* ssid = "XXXXXX";
const char* password = "XXXXXX";
const char* server = "api.thingspeak.com";
WiFiClient client;

#define _pin  D1
#define sleepTimeS 20
#define maxWifiRetryCount 10

uint32_t pulse(uint32_t pin, uint32_t state, uint32_t timeout)
{
  uint32_t maxloops = microsecondsToClockCycles(timeout) / 16;
  uint32_t numloops = 0;

  while (digitalRead(pin) == state) {
		if (numloops++ == maxloops)
			return 0;
  }

  return numloops;
}

void initialize(uint32_t pin)
{
  pinMode(pin, INPUT_PULLUP);
}

void read(uint32_t pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(20);

  uint32_t cycles[80];

  noInterrupts();
  {
    pinMode(pin, INPUT_PULLUP);

    if (pulseIn(pin, HIGH) == 0) {
     sensorWakeUpError++;
     Serial.println("Timeout waiting for start signal LOW pulse.");
     return;
   }

   for (int i = 0; i < 80; i+= 2) {
      cycles[i] = pulse(pin, LOW, 150);
      cycles[i + 1] = pulse(pin, HIGH, 150);
    }

    interrupts();
 }

 uint32_t data[5];
 data[0] = data[1] = data[2] = data[3] = data[4] = 0;

 for (int i = 0; i < 40; ++i) {
    uint32_t lowCycles  = cycles[2 * i];
    uint32_t highCycles = cycles[2 * i + 1];

    //Serial.println("Low: " + String(lowCycles) + " High: " + String(highCycles));

    data[i/8] <<= 1;

    if (highCycles > lowCycles) {
      data[i/8] |= 1;
    }
  }

  Serial.println("(High Humidity)    Data[0]: " + String(data[0]));
  Serial.println("(Low Humidity)     Data[1]: " + String(data[1]));
  Serial.println("(High Temperature) Data[2]: " + String(data[2]));
  Serial.println("(Low Temperature)  Data[3]: " + String(data[3]));
  Serial.println("(Parity)           Data[4]: " + String(data[4]));
  uint32_t calculatedParity = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
  Serial.println("Calculated parity: " + String(calculatedParity));
  Serial.println("Voltage: " + String(ESP.getVcc()));

  if (data[4] != calculatedParity)
  {
    sensorParityError++;
    Serial.println("Parity check failed");
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi not connected. Cannot send data to thingspeak");
    return;
  }

  if (client.connect(server, 80)) { // "184.106.153.149" or api.thingspeak.com
    String postStr = apiKey;
    postStr +="&field1=";
    postStr += String(data[2]);
    postStr +="&field2=";
    postStr += String(data[0]);
    postStr +="&field3=";
    postStr += String(ESP.getVcc());
    postStr +="&field4=";
    postStr += String(sensorWakeUpError);
    postStr +="&field5=";
    postStr += String(sensorParityError);
    postStr +="&status=";
    postStr += "Running for " + String(sleepCount) + " cycles";
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
}

bool checkWifiConnection(int maxRetry)
{
  int wifiRetryCount = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi not connected. Retrying..");

    if (wifiRetryCount++ >= maxRetry)
    {
      Serial.println("Cannot connect to Wifi.");
      return false;
    }

    delay(1000);
  }

  return true;
}

int getResetReason() {
  rst_info* ri = system_get_rst_info();
  if (ri == NULL)
    return -1;

  return ri->reason;
}

void writeRtcMemory()
{
  ESP.rtcUserMemoryWrite(sleepCountOffset, &sleepCount, sizeof(sleepCount));
  Serial.println("SleepCount is written to Rtc mem as " + String(sleepCount));

  ESP.rtcUserMemoryWrite(sensorWakeUpErrorOffset, &sensorWakeUpError, sizeof(sensorWakeUpError));
  Serial.println("SensorWakeUpError is written to Rtc mem as " + String(sensorWakeUpError));

  ESP.rtcUserMemoryWrite(sensorParityErrorOffset, &sensorParityError, sizeof(sensorParityError));
  Serial.println("SensorParityError is written to Rtc mem as " + String(sensorParityError));
}

void readRtcMemory()
{
  if (getResetReason() == REASON_DEEP_SLEEP_AWAKE) {
    ESP.rtcUserMemoryRead(sleepCountOffset, &sleepCount, sizeof(sleepCount));
    Serial.println("SleepCount is read from Rtc mem as " + String(sleepCount));

    ESP.rtcUserMemoryRead(sensorWakeUpErrorOffset, &sensorWakeUpError, sizeof(sensorWakeUpError));
    Serial.println("SensorWakeUpError is read from Rtc mem as " + String(sensorWakeUpError));

    ESP.rtcUserMemoryRead(sensorParityErrorOffset, &sensorParityError, sizeof(sensorParityError));
    Serial.println("SensorParityError is read from Rtc mem as " + String(sensorParityError));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println("Initialized..");

  initialize(_pin);
  WiFi.begin(ssid, password);

  readRtcMemory();
}

void loop() {
  if (checkWifiConnection(maxWifiRetryCount))
  {
    delay(2000);
    read(_pin);
  }

  sleepCount++;

  writeRtcMemory();

  Serial.println("Sleeping for " + String(sleepTimeS) + " seconds..");

  ESP.deepSleep(sleepTimeS * 1000000);
}
