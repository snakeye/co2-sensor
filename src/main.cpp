#include <Arduino.h>

// CO2 sensor
#include <SoftwareSerial.h>
SoftwareSerial swSerial(14, 16, false, 256);

#include <MHZ19.h>
MHZ19 mhz19;

// Configuration
#include <WiFiConfig.h>
#define SERVER_NAME_LENGTH 120

struct Config
{
    struct CO2
    {
        int warning = 800;
        int danger = 1400;
    } co2;
    struct MQTT
    {
        bool enabled = false;
        char server[SERVER_NAME_LENGTH] = {0};
        int port = 0;
    } mqtt;
} config;

ConfigManager configManager;

// WS2812
#include <WS2812FX.h>

#define LED_COUNT 1
#define LED_PIN 4
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

//
#include <RecurringTask.h>

/**
 * @brief
 *
 */
void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);

    // configuration setup
    configManager.setAPName("CO2 Sensor");

    configManager.addParameterGroup("co2", new Metadata("CO2", "CO2 levels for indication"))
        .addParameter("warning", &config.co2.warning, new Metadata("Warning level"))
        .addParameter("danger", &config.co2.danger, new Metadata("Danger level"));

    configManager.addParameterGroup("mqtt", new Metadata("MQTT", "MQTT Server configuration"))
        .addParameter("enabled", &config.mqtt.enabled, new Metadata("Enabled"))
        .addParameter("server", config.mqtt.server, SERVER_NAME_LENGTH, new Metadata("Server"))
        .addParameter("port", &config.mqtt.port, new Metadata("Port"));

    configManager.begin(config);

    // MH-z19b sensor setup
    swSerial.begin(9600);
    mhz19.setSerial(&swSerial);
    // mhz19.setRange(5000);

    // Indicator setup
    ws2812fx.init();
    ws2812fx.setBrightness(100);
    ws2812fx.setColor(0x8800FF);
    ws2812fx.setSpeed(200);
    ws2812fx.start();
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

    // measure CO2 level every second
    RecurringTask::interval(1000, [&]() {
        if (mhz19.isReady())
        {
            int co2ppm = 0;
            MHZ19::ErrorCode res = mhz19.readValue(&co2ppm);

            if (res != MHZ19::ErrorCode::OK)
            {
                Serial.print("Error: ");
                Serial.println(res);

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
            }
        }
        else
        {
            Serial.println("CO2 not ready");
        }
    });

    //
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
            ws2812fx.setMode(FX_MODE_BLINK);
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

    //
    configManager.loop();

    ws2812fx.service();

    //
    delay(1);
}