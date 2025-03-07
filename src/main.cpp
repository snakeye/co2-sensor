#define DHCPS_DEBUG

#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include <SoftwareSerial.h>
#include <MHZ.h>

#include <WS2812FX.h>

#include <RecurringTask.h>

#include "config.h"
#include "util.h"

// CO2 sensor
const auto MH_Z19_RX = 14;
const auto MH_Z19_TX = 16;
// SoftwareSerial swSerial(MH_Z19_RX, MH_Z19_TX);
MHZ co2(MH_Z19_RX, MH_Z19_TX, MHZ::MHZ19B);
bool isCalibrated = false;
// MHZ19 co2;

// WS2812
#define LED_COUNT 1
#define LED_PIN 15
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define LEVEL_BASE 400
#define LEVEL_GOOD 1000
#define LEVEL_WARNING 1500
#define LEVEL_DANGER 2000

#define SPEED_SLOW 5000
#define SPEED_FAST 1000

/**
 *
 */
void onOTAStart()
{
    ws2812fx.setBrightness(5);
    ws2812fx.setColor(rgb(0, 0, 255));
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.service();
}

/**
 *
 */
void onOTAEnd()
{
    ws2812fx.setBrightness(25);
    ws2812fx.setColor(rgb(0, 0, 255));
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.service();
}

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
    digitalWrite(LED_BUILTIN, HIGH);

    //
    co2.setAutoCalibrate(false);
    co2.setBypassCheck(true, true);
    co2.setDebug(false);

    // Indicator setup
    ws2812fx.init();
    ws2812fx.start();

    setBrightness(15);
    setColor(rgb(255, 255, 255));
    setMode(FX_MODE_BREATH);
    setSpeed(SPEED_FAST);

    //
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(deviceName);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10);
        ws2812fx.service();
    }
    Serial1.println("WiFi connected");

    setBrightness(5);
    setColor(rgb(0, 0, 255));
    setMode(FX_MODE_BREATH);
    setSpeed(SPEED_SLOW);

    //
    MDNS.begin(hostName);

    //
    ArduinoOTA.setHostname(hostName);
    ArduinoOTA.onStart([]()
                       { onOTAStart(); });
    ArduinoOTA.onEnd([]()
                     { onOTAEnd(); });
    ArduinoOTA.begin();
}

/**
 *
 */
void measureCO2()
{
    if (co2.isPreHeating())
    {
        setBrightness(5);
        setColor(rgb(0, 192, 255));
        setMode(FX_MODE_STATIC);
        // Serial.println("Preheating");
        return;
    }

    digitalWrite(LED_BUILTIN, LOW);

    int ppm_uart = co2.readCO2UART();

    digitalWrite(LED_BUILTIN, HIGH);

    Serial.print("PPMuart: ");
    if (ppm_uart > 0)
    {
        Serial.println(ppm_uart);
    }
    else
    {
        Serial.println("n/a");
    }

    if (ppm_uart <= LEVEL_GOOD)
    {
        setBrightness(5);
        setColor(rgb(0, 255, 0));
        setSpeed(5000);
        setMode(FX_MODE_STATIC);
    }
    else if (ppm_uart <= LEVEL_WARNING)
    {
        setBrightness(5);

        uint32_t r = transform(ppm_uart, range(LEVEL_GOOD, LEVEL_WARNING), range(0, 255));
        setColor(rgb(r, 255, 0));

        uint16_t speed = transform(ppm_uart, range(LEVEL_GOOD, LEVEL_DANGER), range(SPEED_FAST, SPEED_SLOW));
        setSpeed(speed);

        setMode(FX_MODE_BREATH);
    }
    else if (ppm_uart <= LEVEL_DANGER)
    {
        uint8_t brightness = transform(ppm_uart, range(LEVEL_WARNING, LEVEL_DANGER), range(5, 20));
        setBrightness(brightness);

        uint32_t g = transform(ppm_uart, range(LEVEL_WARNING, LEVEL_DANGER), range(0, 255));
        setColor(rgb(255, g, 0));

        uint16_t speed = transform(ppm_uart, range(LEVEL_GOOD, LEVEL_DANGER), range(SPEED_FAST, SPEED_SLOW));
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

void calibrate()
{
    unsigned long now = millis();
    const unsigned long waitUntilCalibration = TIME_TO_MS(0, 30, 0);

    if (!isCalibrated && now > waitUntilCalibration)
    {
        co2.calibrateZero();
        isCalibrated = true;
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

    RecurringTask::interval(10000, []()
                            { measureCO2(); });

    // RecurringTask::interval(1000, []()
    //                         { calibrate(); });

    // relax a bit
    delay(1);
}
