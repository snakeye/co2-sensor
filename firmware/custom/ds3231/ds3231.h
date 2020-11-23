#pragma once

#include "esphome.h"

#include "RTClib.h"

RTC_DS3231 rtc;

class DS3231Sensor : public PollingComponent
{
public:
    Sensor *year = new Sensor();
    Sensor *month = new Sensor();
    Sensor *day = new Sensor();
    Sensor *hour = new Sensor();
    Sensor *minute = new Sensor();
    Sensor *second = new Sensor();
    Sensor *temperature = new Sensor();

public:
    DS3231Sensor() : PollingComponent(30000)
    {
    }

    void setup()
    {
        if (!rtc.begin())
        {
        }
    }

    virtual void update()
    {
        DateTime now = rtc.now();

        year->publish_state(now.year());
        month->publish_state(now.month());
        day->publish_state(now.day());
        hour->publish_state(now.hour());
        minute->publish_state(now.minute());
        second->publish_state(now.second());
        temperature->publish_state(rtc.getTemperature());
    }
};
