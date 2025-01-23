#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <RPC.h>
#include <Wire.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme(&Wire);

#if defined(ARDUINO_PORTENTA_X8)
// Connect an UART/USB adapter to UART0 on a Portenta Max Carrier or Breakout Carrier
#define SerialDebug Serial1
#else
#define SerialDebug Serial
#endif

constexpr auto readInterval_us { 10 * 1000ul };
auto readNow_us { readInterval_us };

String line;

void setup()
{
    SerialDebug.begin(115200);

    if (!bme.begin()) {
        while (1)
            ;
    }

    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms

    line.reserve(64);
}

void loop()
{
    if (micros() > readNow_us) {
        if (!bme.performReading())
            return;

        line = "";
        line += bme.temperature;
        line += ',';
        line += bme.pressure / 100.0;
        line += ',';
        line += bme.humidity;
        line += ',';
        line += bme.gas_resistance / 1000.0;

        SerialDebug.println(line);
        readNow_us = micros() + readInterval_us;
    }
}
