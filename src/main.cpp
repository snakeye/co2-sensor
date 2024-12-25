#define DHCPS_DEBUG

#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include <SoftwareSerial.h>
#include <MHZ.h>

#include <WS2812FX.h>

#include <RecurringTask.h>

#include "config.h"

// CO2 sensor
const auto MH_Z19_RX = 14;
const auto MH_Z19_TX = 16;
// SoftwareSerial swSerial(MH_Z19_RX, MH_Z19_TX);
MHZ co2(MH_Z19_RX, MH_Z19_TX, MHZ::MHZ19B);
// MHZ19 co2;

// WS2812
#define LED_COUNT 1
#define LED_PIN 15
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define LEVEL_BASE 400
#define LEVEL_WARNING 1000
#define LEVEL_DANGER 1500

#define SPEED_SLOW 5000
#define SPEED_FAST 1000

/**
 *
 * @brief
 *
 */
void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println(deviceName);

    //
    pinMode(LED_BUILTIN, OUTPUT);

    //
    co2.setBypassCheck(true, true);
    co2.setDebug(false);

    // Indicator setup
    ws2812fx.init();
    ws2812fx.setBrightness(5);
    ws2812fx.setColor(0x8800FF);
    ws2812fx.setMode(FX_MODE_BREATH);
    ws2812fx.setSpeed(SPEED_SLOW);
    ws2812fx.start();

    //
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(deviceName);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }
    Serial1.println("WiFi connected");

    //
    MDNS.begin(hostName);

    //
    ArduinoOTA.setHostname(hostName);
    ArduinoOTA.begin();
}

void measureCO2()
{
    if (co2.isPreHeating())
    {
        // Serial.println("Preheating");
        return;
    }

    int ppm_uart = co2.readCO2UART();
    Serial.print("PPMuart: ");
    if (ppm_uart > 0)
    {
        Serial.println(ppm_uart);
    }
    else
    {
        Serial.println("n/a");
    }

    if (ppm_uart <= LEVEL_WARNING)
    {
        uint32_t r = ((ppm_uart - LEVEL_BASE) * 255 / (LEVEL_WARNING - LEVEL_BASE)) & 0xFF;
        uint32_t color = (r << 16) | (0xFF << 8) | 0x00;
        ws2812fx.setColor(color);

        ws2812fx.setBrightness(5);

        uint32_t speed = SPEED_FAST + (LEVEL_DANGER - ppm_uart) * (SPEED_SLOW - SPEED_FAST) / (LEVEL_DANGER - LEVEL_BASE);
        ws2812fx.setSpeed(speed);
    }
    else if (ppm_uart <= LEVEL_DANGER)
    {
        uint32_t g = ((LEVEL_DANGER - ppm_uart) * 255 / (LEVEL_DANGER - LEVEL_WARNING)) & 0xFF;
        uint32_t color = (0xFF << 16) | (g << 8) | 0x00;
        ws2812fx.setColor(color);

        uint8_t brightness = 5 + (ppm_uart - LEVEL_WARNING) * (20 - 5) / (LEVEL_DANGER - LEVEL_WARNING);
        ws2812fx.setBrightness(brightness);

        uint32_t speed = SPEED_FAST + (LEVEL_DANGER - ppm_uart) * (SPEED_SLOW - SPEED_FAST) / (LEVEL_DANGER - LEVEL_BASE);
        ws2812fx.setSpeed(speed);
    }
    else
    {
        ws2812fx.setBrightness(30);
        ws2812fx.setColor(0xFF0000);
        ws2812fx.setSpeed(SPEED_FAST);
    }
}

/**
 * @brief
 *
 */
void loop()
{
    ws2812fx.service();

    ArduinoOTA.handle();

    RecurringTask::interval(5000, []()
                            { measureCO2(); });

    // relax a bit
    delay(1);
}