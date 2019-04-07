#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <ArduinoOTA.h>


WiFiManager wifiManager;

//
#include <PubSubClient.h>

const char* mqtt_server = "192.168.178.29";

WiFiClient espClient;
PubSubClient client(espClient);

//
#include <SoftwareSerial.h>
SoftwareSerial co2(0, 2);

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);

  Serial.begin(115200);

  WiFi.hostname("CO2 Sensor");

  wifiManager.autoConnect("CO2 Sensor");

  randomSeed(micros());

  ArduinoOTA.setHostname("CO2 Sensor");
  ArduinoOTA.begin();

  client.setServer(mqtt_server, 1883);
  client.connect("CO2 Sensor");

  co2.begin(9600);
}

int co2ppm() {
  static byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  static byte response[9];

  co2.write(cmd, 9);
  co2.readBytes(response, 9);

  unsigned int responseHigh = (unsigned int) response[2];
  unsigned int responseLow = (unsigned int) response[3];

  return (256 * responseHigh) + responseLow;
}

void loop()
{
  ArduinoOTA.handle();

  static char msg[250];

  int ppm_uart = co2ppm();
  snprintf (msg, 250, "co2 ppm=%d", ppm_uart);

  client.publish("sensor/co2", msg);

  // wait for 1 second
  delay(1000);
}
