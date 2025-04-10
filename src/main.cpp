#define DHCPS_DEBUG

#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <MHZ.h>
#include <WS2812FX.h>
#include <RecurringTask.h>
#include <ArduinoJson.h>

#include "config.h"
#include "util.h"

// === Constants ===
constexpr uint8_t PIN_MHZ_RX = 14;
constexpr uint8_t PIN_MHZ_TX = 16;

constexpr uint8_t PIN_LED = 15;
constexpr uint8_t LED_COUNT = 1;

constexpr int SPEED_SLOW = 5000;
constexpr int SPEED_FAST = 1000;

constexpr int LEVEL_GOOD = 1000;
constexpr int LEVEL_WARNING = 1500;
constexpr int LEVEL_DANGER = 2000;

// === Globals ===
MHZ co2(PIN_MHZ_RX, PIN_MHZ_TX, MHZ::MHZ19B);
bool isCalibrated = false;

WS2812FX ws2812fx(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String deviceId = String("co2mon_") + ESP.getChipId();

// === LED Utils ===
inline void setBrightness(uint8_t brightness)
{
    static uint8_t prev = 0;
    setIfChanged(prev, brightness, [](uint8_t b)
                 { ws2812fx.setBrightness(b); });
}

inline void setColor(uint32_t color)
{
    static uint32_t prev = 0;
    setIfChanged(prev, color, [](uint32_t c)
                 { ws2812fx.setColor(c); });
}

inline void setSpeed(uint16_t speed)
{
    static uint16_t prev = 0;
    setIfChanged(prev, speed, [](uint16_t s)
                 { ws2812fx.setSpeed(s); });
}

inline void setMode(uint8_t mode)
{
    static uint8_t prev = 0;
    setIfChanged(prev, mode, [](uint8_t m)
                 { ws2812fx.setMode(m); });
}

// === OTA ===
void setupOTA()
{
    ArduinoOTA.setHostname(hostName);
    ArduinoOTA.onStart([]()
                       {
        ws2812fx.setBrightness(5);
        ws2812fx.setColor(rgb(0, 0, 255));
        ws2812fx.setMode(FX_MODE_STATIC);
        ws2812fx.service(); });
    ArduinoOTA.onEnd([]()
                     {
        ws2812fx.setBrightness(25);
        ws2812fx.setColor(rgb(0, 0, 255));
        ws2812fx.setMode(FX_MODE_STATIC);
        ws2812fx.service(); });
    ArduinoOTA.begin();
}

// === WiFi ===
void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostName);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10);
        ws2812fx.service();
    }
    Serial.println("WiFi connected");
}

// === MQTT ===
String buildDiscoveryPayload()
{
    StaticJsonDocument<512> doc;

    doc["name"] = "CO2";
    doc["state_topic"] = "sensors/mhz19b/co2";
    doc["unit_of_measurement"] = "ppm";
    doc["device_class"] = "carbon_dioxide";
    doc["state_class"] = "measurement";
    doc["expire_after"] = 120;
    doc["unique_id"] = "co2-" + String(ESP.getChipId());

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = deviceId;
    device["name"] = deviceName;
    device["manufacturer"] = "DIY";
    device["model"] = "MH-Z19B CO2 Sensor";
    device["sw_version"] = "1.0";

    String output;
    serializeJson(doc, output);
    return output;
}

void reconnectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");

        if (mqttClient.connect("MHZ19B_Client"))
        {
            Serial.println("connected");

            String payload = buildDiscoveryPayload();
            mqttClient.publish("homeassistant/sensor/mhz19b/config", payload.c_str(), true);
        }
        else
        {
            Serial.printf("failed, rc=%d. Retrying in 5s...\n", mqttClient.state());
            delay(5000);
        }
    }
}

void publishCO2Data(int ppm)
{
    mqttClient.publish("sensors/mhz19b/co2", String(ppm).c_str(), true);
}

void publishTemperatureData(float temperature)
{
    mqttClient.publish("sensors/mhz19b/temperature", String(temperature).c_str(), true);
}

// === CO2 Reading ===
void updateLedEffect(int ppm)
{
    if (ppm <= LEVEL_GOOD)
    {
        setBrightness(5);
        setColor(rgb(0, 255, 0));
        setMode(FX_MODE_STATIC);
        setSpeed(SPEED_SLOW);
    }
    else if (ppm <= LEVEL_WARNING)
    {
        setBrightness(5);

        uint32_t r = transform(ppm, range(LEVEL_GOOD, LEVEL_WARNING), range(0, 255));
        setColor(rgb(r, 255, 0));

        uint16_t speed = transform(ppm, range(LEVEL_GOOD, LEVEL_DANGER), range(SPEED_FAST, SPEED_SLOW));
        setSpeed(speed);
        setMode(FX_MODE_BREATH);
    }
    else if (ppm <= LEVEL_DANGER)
    {
        uint8_t brightness = transform(ppm, range(LEVEL_WARNING, LEVEL_DANGER), range(5, 20));
        uint32_t g = transform(ppm, range(LEVEL_WARNING, LEVEL_DANGER), range(0, 255));
        uint16_t speed = transform(ppm, range(LEVEL_GOOD, LEVEL_DANGER), range(SPEED_FAST, SPEED_SLOW));

        setBrightness(brightness);
        setColor(rgb(255, g, 0));
        setSpeed(speed);
        setMode(FX_MODE_BREATH);
    }
    else
    {
        setBrightness(20);
        setColor(rgb(255, 0, 0));
        setSpeed(SPEED_FAST);
        setMode(FX_MODE_STROBE);
    }
}

void measureCO2()
{
    if (co2.isPreHeating())
    {
        setBrightness(5);
        setColor(rgb(0, 192, 255));
        setMode(FX_MODE_STATIC);
        return;
    }

    digitalWrite(LED_BUILTIN, LOW);
    int ppm = co2.readCO2UART();
    float temperature = co2.getLastTemperature();
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.print("PPMuart: ");
    Serial.println(ppm > 0 ? String(ppm) : "n/a");

    if (ppm > 0)
    {
        updateLedEffect(ppm);
        publishCO2Data(ppm);
    }
}

// === Optional: Calibration ===
void calibrateSensor()
{
    const unsigned long waitMs = TIME_TO_MS(0, 30, 0);
    if (!isCalibrated && millis() > waitMs)
    {
        co2.calibrateZero();
        isCalibrated = true;
    }
}

// === Setup ===
void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    co2.setAutoCalibrate(false);
    co2.setBypassCheck(true, true);
    co2.setDebug(false);

    ws2812fx.init();
    ws2812fx.start();

    setBrightness(15);
    setColor(rgb(255, 255, 255));
    setMode(FX_MODE_BREATH);
    setSpeed(SPEED_FAST);

    connectWiFi();
    setBrightness(5);
    setColor(rgb(0, 0, 255));
    setMode(FX_MODE_BREATH);
    setSpeed(SPEED_SLOW);

    MDNS.begin(deviceName);
    setupOTA();

    mqttClient.setServer(mqttServer, mqttPort);
}

// === Main loop ===
void loop()
{
    if (!mqttClient.connected())
    {
        reconnectMQTT();
    }
    mqttClient.loop();
    ws2812fx.service();
    ArduinoOTA.handle();

    RecurringTask::interval(10000, []()
                            { measureCO2(); });
    // RecurringTask::interval(1000, []() { calibrateSensor(); });

    delay(1);
}
