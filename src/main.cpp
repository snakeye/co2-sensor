#define DHCPS_DEBUG

#include <Arduino.h>
#include <ESP8266mDNS.h>

// CO2 sensor
#include <SoftwareSerial.h>
SoftwareSerial swSerial(14, 16);

#include <MHZ19.h>
MHZ19 mhz19;

// Configuration
#include <WiFiConfig.h>

//
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>

struct Config
{
    struct CO2
    {
        int warning = 800;
        int danger = 1400;
    } co2;
    struct Firebase
    {
        char host[100] = {0};
        char auth[100] = {0};
    } firebase;
} config;

ConfigManager configManager;

// WS2812
#include <WS2812FX.h>

#define LED_COUNT 1
#define LED_PIN 15
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

//
#include <RecurringTask.h>

//
#include <ArduinoOTA.h>

const char *hostname = "CO2 Sensor Test";

const String deviceId = String("Sensor-") + String(ESP.getChipId(), HEX);
const String firebasePath = "/" + deviceId + "/";

// #define FIREBASE_HOST "https://co2-sensor-test.firebaseio.com"
// #define FIREBASE_AUTH "0iamLjkkgc4SfwqoBOSyV6P02FOTo9YshzzMT54K"

/**
 *
 * @brief
 *
 */
void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);

    // configuration setup
    configManager.setAPName(hostname);

    configManager.addParameterGroup("co2", new Metadata("CO2", "CO2 levels for indication"))
        .addParameter("warning", &config.co2.warning, new Metadata("Warning level"))
        .addParameter("danger", &config.co2.danger, new Metadata("Danger level"));

    configManager.addParameterGroup("firebase", new Metadata("Firebase", "Firebase configuration"))
        .addParameter("host", config.firebase.host, 100, new Metadata("Host"))
        .addParameter("auth", config.firebase.auth, 100, new Metadata("Token or Secret"));

    configManager.begin(config);

    // MH-z19b sensor setup
    swSerial.begin(9600);
    mhz19.setSerial(&swSerial);
    // mhz19.setRange(5000);

    // Indicator setup
    ws2812fx.init();
    ws2812fx.setBrightness(50);
    ws2812fx.setColor(0x8800FF);
    ws2812fx.setSpeed(200);
    ws2812fx.start();

    //
    MDNS.begin(hostname);
    MDNS.addService("http", "tcp", 80);

    //
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.begin();

    //
    Serial.println("Starting firebase");
    Serial.println(config.firebase.host);
    Firebase.begin(config.firebase.host, config.firebase.auth);
    // Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

    //
    Serial.println("Go!");
    Serial.println(deviceId);
    Serial.println(firebasePath);
}

enum DisplayStatus
{
    UNDEFINED,
    LOADING,
    BREATH,
    WARNING,
    DANGER,
    ERROR,
};

/**
 * @brief
 *
 */
void loop()
{
    static DisplayStatus lastStatus = DisplayStatus::UNDEFINED;
    static DisplayStatus status = DisplayStatus::LOADING;

    // measure CO2 level every 10 seconds
    RecurringTask::interval(10000, [&]() {
        if (!mhz19.isReady())
        {
            Serial.println("CO2 not ready");
            return;
        }

        int co2ppm = mhz19.readValue();

        if (co2ppm == -1)
        {
            Serial.println("Error");

            status = DisplayStatus::ERROR;
        }
        else
        {
            Serial.print("CO2: ");
            Serial.println(co2ppm);

            if (co2ppm >= config.co2.danger)
            {
                status = DisplayStatus::DANGER;
            }
            else if (co2ppm >= config.co2.warning)
            {
                status = DisplayStatus::WARNING;
            }
            else
            {
                status = DisplayStatus::BREATH;
            }

            Firebase.setInt("co2", co2ppm);
            if (Firebase.failed())
            {
                Serial.print("setting /co2 failed: ");
                Serial.println(Firebase.error());
            }
        }
    });

    // Change indication if status has changed and previous cycle finished
    if (ws2812fx.isFrame() && lastStatus != status)
    {
        switch (status)
        {
        case DisplayStatus::LOADING:
            Serial.println("Loading");
            ws2812fx.setColor(0x0000FF);
            ws2812fx.setMode(FX_MODE_BREATH);
            ws2812fx.setBrightness(32);
            break;
        case DisplayStatus::BREATH:
            Serial.println("Breath");
            ws2812fx.setColor(0x00FF00);
            ws2812fx.setMode(FX_MODE_BREATH);
            ws2812fx.setBrightness(16);
            break;
        case DisplayStatus::WARNING:
            Serial.println("Warning");
            ws2812fx.setColor(0xFFFF00);
            ws2812fx.setMode(FX_MODE_FADE);
            ws2812fx.setBrightness(64);
            break;
        case DisplayStatus::DANGER:
            Serial.println("Danger");
            ws2812fx.setColor(0xFF0000);
            ws2812fx.setMode(FX_MODE_STROBE);
            ws2812fx.setBrightness(128);
            break;
        case DisplayStatus::ERROR:
            Serial.println("Error");
            ws2812fx.setColor(0xFF0000);
            ws2812fx.setMode(FX_MODE_STATIC);
            break;
        default:
            break;
        }

        lastStatus = status;
    }

    // run service tasks
    configManager.loop();
    ws2812fx.service();
    ArduinoOTA.handle();

    // relax a bit
    delay(10);
}