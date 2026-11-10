
/*
  IoTDS18B20SingleSensor.h - Wrapper for a single DS18B20 temperature sensor on one OneWire bus.
  Copyright (c) 2026 Peter Kaleja.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef IOTDS18B20SINGLESENSOR_H
#define IOTDS18B20SINGLESENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <IoTHASensorNumberWrapper.h>
#include <AppSettings.h>
#include <IoTDebug.h>
#include <JSONUtils.h>

/**
 * @class IoTDS18B20SingleSensor
 * @brief HA wrapper for a single DS18B20 temperature sensor on one OneWire bus.
 *
 * Inherits from IoTHASensorNumberWrapper<float>, so this class itself is the HA
 * sensor entity. All HA configuration methods (setName, setUnitOfMeasurement,
 * setDeviceClass, etc.) are available directly.
 *
 * No slot-assignment UI or NVS settings are required: the first DS18B20 found
 * on the bus is used automatically.
 *
 * Usage pattern:
 *   1. Construct with the data pin and a unique HA entity ID.
 *   2. Optionally call setUseFahrenheit(true) before registering with IoTDevice.
 *   3. Register with IoTDevice::registerComponent() — begin() is called automatically by preSetup().
 *   4. Configure the HA entity (setName, setDeviceClass, etc.) in postSetup().
 *   5. In the loop (via a Timer): call requestTemperatures() then publishValue().
 *
 * Example:
 * @code
 *   IoTDS18B20SingleSensor sensor(D4, "temp_main");
 *
 *   // In postSetup():
 *   sensor.setName("Room temperature");
 *
 *   // In postLoop() via a Timer:
 *   sensor.requestTemperatures();
 *   sensor.publishValue();
 * @endcode
 */
class IoTDS18B20SingleSensor : public IoTHASensorNumberWrapper<float>
{
public:
    /**
     * Value returned by getTemperatureC() when no sensor is present or a read error occurred.
     * Equals DEVICE_DISCONNECTED_C (-127.0).
     */
    static constexpr float INVALID_TEMPERATURE = DEVICE_DISCONNECTED_C;

    /**
     * @brief Construct the sensor wrapper.
     *
     * @param pin  OneWire data pin (e.g. D4).
     * @param uid  Unique HA entity ID string.
     */
    IoTDS18B20SingleSensor(uint8_t pin, const char* uid)
        : IoTHASensorNumberWrapper<float>(uid)
        , _oneWire(pin)
        , _dallasTemp(&_oneWire)
    {
        memset(_address, 0, sizeof(DeviceAddress));
    }

    IoTDS18B20SingleSensor(const IoTDS18B20SingleSensor&)            = delete;
    IoTDS18B20SingleSensor& operator=(const IoTDS18B20SingleSensor&) = delete;
    IoTDS18B20SingleSensor(IoTDS18B20SingleSensor&&)                 = delete;
    IoTDS18B20SingleSensor& operator=(IoTDS18B20SingleSensor&&)      = delete;

    /**
     * @brief Select Fahrenheit output instead of the default Celsius.
     * Must be called before begin().
     */
    void setUseFahrenheit(bool useFahrenheit) { m_useFahrenheit = useFahrenheit; }

    /** @brief Returns true if begin() discovered a sensor on the bus. */
    bool sensorFound() const { return _sensorFound; }

    /**
     * @brief Trigger temperature conversion and cache the result.
     *        Blocks ~750 ms at 12-bit resolution. If the first read returns -127,
     *        retries up to 2 more times (3 total); yield() is called before each
     *        retry to keep background tasks alive.
     * @return true if a sensor is present and the read succeeded.
     */
    bool requestTemperatures()
    {
        if (!_sensorFound)
        {
            IOTLOGWARN(F("DS18B20Single: requestTemperatures — no sensor on bus"));
            return false;
        }
        _dallasTemp.requestTemperatures();
        float t = _dallasTemp.getTempC(_address);
        if (t == INVALID_TEMPERATURE)
        {
            t = readTempWithRetry(_dallasTemp, _address, 2);
            if (t == INVALID_TEMPERATURE)
            {
                IOTLOGWARN(F("DS18B20Single: all 3 read attempts failed"));
                return false;
            }
        }
        _rawTemperatureC = t;
        IOTLOGDEBUG1(F("DS18B20Single:"), _rawTemperatureC);
        return true;
    }

    /**
     * @brief Return the temperature cached by the last requestTemperatures() call.
     * @return Temperature in °C, or INVALID_TEMPERATURE on error or no sensor.
     */
    float getTemperatureC() const { return _rawTemperatureC; }

    /**
     * @brief Convert the cached temperature to the configured unit, set the HA
     *        sensor value, and publish to MQTT.
     * @param force  Pass true to publish even if the value has not changed.
     */
    bool publishValue(const bool force = false) override
    {
        if (!_sensorFound) return false;
        setCurrentValue(AppSettings::convertTemperature(_rawTemperatureC, !m_useFahrenheit));
        return IoTHASensorNumberWrapper<float>::publishValue(force);
    }

    /**
     * @brief Trigger a temperature conversion and cache results.
     *        Called by IoTDevice::updateAllComponents() each update cycle.
     */
    bool update(bool /*force*/ = false) override { return requestTemperatures(); }

    /**
     * @brief Return a JSON fragment for this sensor.
     *        Returns empty String when no sensor is present.
     */
    String statusJSON() const override
    {
        if (!_sensorFound) return String();
        const bool  inCelsius = !m_useFahrenheit;
        const char* unit      = AppSettings::temperatureUnit(inCelsius);
        float       val       = AppSettings::convertTemperature(_rawTemperatureC, inCelsius);
        const char* nm        = name();
        bool        hasName   = nm && nm[0] != '\0';
        String obj;
        if (hasName) obj += JSONUtils::Pair(F("name"), nm, true);
        obj += JSONUtils::Pair(F("value"), val, !hasName);
        obj += JSONUtils::Pair(F("unit"), unit);
        return JSONUtils::EncloseObject(obj);
    }

protected:
    /**
     * @brief Scan the bus, store the address of the first DS18B20 found,
     *        set 12-bit resolution and configure the HA entity unit.
     *        Called automatically by IoTDevice::preSetup().
     */
    void begin() override
    {
        setUnitOfMeasurement(AppSettings::temperatureUnit(!m_useFahrenheit));

        // Short pause after WiFi SDK init (WiFi.macAddress in IoTDevice::preSetup)
        // lets power transients settle before the time-critical OneWire bus scan.
        delay(100);

        // Retry: OneWire::reset() runs from flash cache on ESP8266 (no IRAM_ATTR).
        // A cold iCache after WiFi init can extend the 70 µs presence-pulse sampling
        // window past the DS18B20's response, making the first scan fail silently.
        for (uint8_t attempt = 0; attempt < 3 && !_sensorFound; ++attempt)
        {
            if (attempt > 0)
                delay(200);

            _dallasTemp.begin();

            const uint8_t found = _dallasTemp.getDeviceCount();
            for (uint8_t i = 0; i < found; ++i)
            {
                DeviceAddress addr;
                if (_dallasTemp.getAddress(addr, i) && _dallasTemp.validFamily(addr))
                {
                    memcpy(_address, addr, sizeof(DeviceAddress));
                    _sensorFound = true;
                    break;
                }
            }
        }

        if (_sensorFound)
            _dallasTemp.setResolution(_address, 12);

        IOTLOGINFO1(F("DS18B20Single: sensor found:"), _sensorFound);
    }

private:
    // _oneWire must be declared before _dallasTemp (member init order = declaration order).
    OneWire           _oneWire;
    DallasTemperature _dallasTemp;
    DeviceAddress     _address;
    float             _rawTemperatureC = INVALID_TEMPERATURE;
    bool              _sensorFound     = false;
    bool              m_useFahrenheit  = false;

    static float readTempWithRetry(DallasTemperature& dt, const uint8_t* addr, uint8_t maxAttempts)
    {
        for (uint8_t a = 0; a < maxAttempts; ++a)
        {
            yield();
            dt.requestTemperaturesByAddress(addr);
            float t = dt.getTempC(addr);
            if (t != DEVICE_DISCONNECTED_C)
                return t;
        }
        return DEVICE_DISCONNECTED_C;
    }
};

#endif // IOTDS18B20SINGLESENSOR_H
