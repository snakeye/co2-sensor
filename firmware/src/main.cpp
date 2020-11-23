#include <Arduino.h>
#include <Wire.h>

#include <RecurringTask.h>

#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp; // I2C

#include <WS2812FX.h>

#define LED_COUNT 1
#define LED_PIN 2

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting...");

    if (!bmp.begin(0x76))
    {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
        while (1)
            ;
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X4,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X4,     /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_4000); /* Standby time. */

    ws2812fx.init();
    ws2812fx.setBrightness(0);
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.start();

    Serial.println("Started!");
}

void loop()
{
    const unsigned int wake_interval = 10;

    RecurringTask::interval(wake_interval * 1000, []() {
        Serial.print(F("Temperature = "));
        Serial.print(bmp.readTemperature());
        Serial.println(" *C");

        Serial.print(F("Pressure = "));
        Serial.print(bmp.readPressure() / 100.0);
        Serial.println(" hPa");

        Serial.println();
    });

    RecurringTask::interval(50, []() {
        ws2812fx.service();
    });

    delay(50);
}