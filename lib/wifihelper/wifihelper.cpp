#include <ESP8266WiFi.h>

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
