# Carbon Dioxide Sensor MH-Z19b - ESP8266 Prototype


This project utilizes the MH-Z19b carbon dioxide sensor module, which provides a simple and cost-effective way to measure CO2 concentration via UART interface. The sensor measures concentrations in the range of 0-5000ppm, making it suitable for various environmental monitoring applications.

The device is built using the ESP12e module and features an RGB LED to indicate CO2 concentration levels.

![CO2 Sensor](images/co2-sensor.jpg)

## Features

- **CO2 concentration measurement** from 400ppm to 5000ppm using the MH-Z19b sensor.
- **RGB LED indication** with different light modes based on CO2 levels.
  
### LED Modes:

- **Steady Green Light**: CO2 concentration in the range of 400-1000ppm (normal levels).
- **Breathing Light**: Color changes from green to orange as CO2 levels increase from 1000 to 1500ppm.
- **Fast Breathing Light**: Color changes from orange to red as CO2 levels increase from 1500 to 2000ppm.
- **Fast Blinking Red Light**: CO2 concentration exceeds 2000ppm (dangerous levels).
- **Steady Blue Light**: Indicates that an OTA (Over-the-Air) update is in progress.

## Firmware

The firmware for the device is written using **PlatformIO** and the **Arduino** platform. The software controls the sensor, processes the data, and adjusts the LED indications based on the CO2 concentration.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
