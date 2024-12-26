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
#define LEVEL_GOOD 1000
#define LEVEL_WARNING 1500
#define LEVEL_DANGER 2000

#define SPEED_SLOW 5000
#define SPEED_FAST 1000

/**
 *
 */
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

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
    ws2812fx.setBrightness(15);
    ws2812fx.setColor(rgb(0, 255, 255));
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.service();
}

template <typename T, typename Func>
inline void setIfChanged(T &prev, T value, Func action)
{
    if (prev == value)
        return;
    action(value);
    prev = value;
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

template <typename T>
class range
{
public:
    range(T value_min, T value_max) : min(value_min), max(value_max) {}

    T min;
    T max;
};

/**
 * Ensure input is clamped within the input range
 */
template <typename T>
inline T clamp(T value, const range<T> input_range)
{
    if (value < input_range.min)
        return input_range.min;
    if (value > input_range.max)
        return input_range.max;
    return value;
}

/**
 * Perform linear transformation
 */
template <typename T>
inline T transform(T input, const range<T> input_range, const range<T> output_range)
{
    input = clamp(input, input_range);

    T input_range_diff = input_range.max - input_range.min;
    T output_range_diff = output_range.max - output_range.min;

    return output_range.min + (input - input_range.min) * (output_range_diff) / (input_range_diff);
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
        setBrightness(1);
        setColor(rgb(255, 0, 0));
        setMode(FX_MODE_STATIC);
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

/**
 * @brief
 *
 */
void loop()
{
    ws2812fx.service();

    ArduinoOTA.handle();

    RecurringTask::interval(15000, []()
                            { measureCO2(); });

    // relax a bit
    delay(1);
}