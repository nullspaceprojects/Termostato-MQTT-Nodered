#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <ArduinoOTA.h>
#endif

// Must use forked library
#include <ESPAsyncTCP.h>      // https://github.com/khoih-prog/ESPAsyncTCP

#include "SyncClient.h"

const char* ssid = "your_ssid";
const char* password = "12345678";

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  delay(200);

  WiFi.begin(ssid, password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("WiFi Failed!\n");

    return;
  }

  Serial.printf("WiFi Connected!\n");
  Serial.println(WiFi.localIP());

#ifdef ESP8266
  ArduinoOTA.begin();
#endif

  SyncClient client;

  if (!client.connect("www.google.com", 80))
  {
    Serial.println("Connect Failed");

    return;
  }

  client.setTimeout(2);

  if (client.printf("GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n") > 0)
  {
    while (client.connected() && client.available() == 0)
    {
      delay(1);
    }

    while (client.available())
    {
      Serial.write(client.read());
    }

    if (client.connected())
    {
      client.stop();
    }
  }
  else
  {
    client.stop();
    Serial.println("Send Failed");

    while (client.connected())
      delay(0);
  }
}

void loop()
{
#ifdef ESP8266
  ArduinoOTA.handle();
#endif
}
