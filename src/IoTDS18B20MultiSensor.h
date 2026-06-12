
/*
  IoTDS18B20MultiSensor.h - Template composite wrapper for N DS18B20 temperature sensors
  on one OneWire bus.
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

#ifndef IOTDS18B20MULTISENSOR_H
#define IOTDS18B20MULTISENSOR_H

#include <utility>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <IoTHACompositeDeviceWrapper.h>
#include <IoTHASensorNumberWrapper.h>
#include <AppSettings.h>
#include <IoTDebug.h>
#include <JSONUtils.h>

/**
 * @class IoTDS18B20MultiSensor
 * @brief Template composite HA wrapper for N DS18B20 temperature sensors on one OneWire bus.
 *
 * The template parameter N specifies the maximum number of sensor slots (HA entities)
 * that will be created. N must be between 1 and 8.
 *
 * Slot indices are application-defined: call get<I>().setName(...) in postSetup()
 * to assign meaning to each slot (e.g. slot 0 = "Input temperature").
 *
 * Usage pattern:
 *   1. Construct with the data pin and exactly N unique HA entity IDs.
 *   2. Register with IoTDevice::registerComponent() — begin() is called automatically by preSetup().
 *   3. Optionally call reorderByAddresses() to map known ROM addresses to slots.
 *   4. Configure HA entities via get<I>() in postSetup().
 *   5. In the loop (via a Timer): call requestTemperatures() then publishValue().
 *
 * Example (2-sensor thermostat):
 * @code
 *   IoTDS18B20MultiSensor<2> tempBus(D4, "uid_input", "uid_output");
 *
 *   // In postSetup():
 *   tempBus.get<0>().setName("Input temperature");
 *   tempBus.get<1>().setName("Output temperature");
 *
 *   // In postLoop() via a Timer:
 *   tempBus.requestTemperatures();
 *   tempBus.publishValue();
 * @endcode
 *
 * @tparam N  Number of sensor slots (1–8). Must equal the number of uid arguments
 *            passed to the constructor (enforced by static_assert).
 */
template<size_t N>
class IoTDS18B20MultiSensor
    : public NCompositeWrapper<IoTHASensorNumberWrapper<float>, N>::type
{
    static_assert(N >= 1 && N <= 8, "IoTDS18B20MultiSensor: N must be between 1 and 8.");

    using BaseType = typename NCompositeWrapper<IoTHASensorNumberWrapper<float>, N>::type;

public:
    /** Maximum sensor slots — equals the template parameter N. */
    static constexpr size_t MAX_SENSORS = N;

    /**
     * Value returned by getTemperatureC() when a slot is empty or a read error occurred.
     * Equals DEVICE_DISCONNECTED_C (-127.0).
     */
    static constexpr float INVALID_TEMPERATURE = DEVICE_DISCONNECTED_C;

    /**
     * @brief Construct the sensor bus and N HA entity wrappers.
     *
     * @param pin   OneWire data pin (e.g. D4).
     * @param uids  Exactly N unique HA entity ID strings, one per slot, in order.
     *              The count is verified at compile time.
     */
    template<typename... UIDs>
    IoTDS18B20MultiSensor(uint8_t pin, UIDs&&... uids)
        : BaseType(std::forward<UIDs>(uids)...)
        , _oneWire(pin)
        , _dallasTemp(&_oneWire)
        , _sensorCount(0)
    {
        static_assert(sizeof...(UIDs) == N,
            "IoTDS18B20MultiSensor: must supply exactly N uid strings (one per sensor slot).");
        for (size_t i = 0; i < N; ++i)
        {
            memset(_addresses[i], 0, sizeof(DeviceAddress));
            _temperatures[i] = INVALID_TEMPERATURE;
        }
    }

    IoTDS18B20MultiSensor(const IoTDS18B20MultiSensor&)            = delete;
    IoTDS18B20MultiSensor& operator=(const IoTDS18B20MultiSensor&) = delete;
    IoTDS18B20MultiSensor(IoTDS18B20MultiSensor&&)                 = delete;
    IoTDS18B20MultiSensor& operator=(IoTDS18B20MultiSensor&&)      = delete;

    /**
     * @brief Select Fahrenheit output instead of the default Celsius.
     * Must be called before begin().
     */
    void setUseFahrenheit(bool useFahrenheit) { m_useFahrenheit = useFahrenheit; }

    /** @brief Number of sensors found after begin(). */
    uint8_t sensorCount() const { return _sensorCount; }

    /**
     * @brief Whether a sensor is physically present at the given slot index.
     * @param index  0-based slot (0..N-1).
     */
    bool isValid(uint8_t index) const { return index < _sensorCount; }

    /**
     * @brief Trigger temperature conversion on all bus sensors and cache the results.
     *        Blocks ~750 ms at 12-bit resolution.
     * @return true if at least one sensor is present.
     */
    bool requestTemperatures()
    {
        if (_sensorCount == 0)
        {
            IOTLOGWARN(F("DS18B20: requestTemperatures — no sensors on bus"));
            return false;
        }
        _dallasTemp.requestTemperatures();
        for (uint8_t i = 0; i < _sensorCount; ++i)
        {
            _temperatures[i] = _dallasTemp.getTempC(_addresses[i]);
            if (_temperatures[i] == INVALID_TEMPERATURE) {
                IOTLOGWARN1(F("DS18B20: read error, sensor"), i);
            } else {
                IOTLOGDEBUG2(F("DS18B20 sensor"), i, _temperatures[i]);
            }
        }
        return true;
    }

    /**
     * @brief Return the temperature cached by the last requestTemperatures() call.
     * @param index  Slot index (0..sensorCount()-1).
     * @return Temperature in °C, or INVALID_TEMPERATURE on error or empty slot.
     */
    float getTemperatureC(uint8_t index) const
    {
        return isValid(index) ? _temperatures[index] : INVALID_TEMPERATURE;
    }

    /**
     * @brief Copy the 8-byte ROM address of a sensor into addr.
     * @param index  Slot index (0..sensorCount()-1).
     * @param addr   Output buffer (DeviceAddress / uint8_t[8]).
     * @return true if the index is valid.
     */
    bool getSensorAddress(uint8_t index, DeviceAddress addr) const
    {
        if (!isValid(index)) return false;
        memcpy(addr, _addresses[index], sizeof(DeviceAddress));
        return true;
    }

    /**
     * @brief After begin(), remap internal slots so that addrs[i] → slot i.
     *
     * Pass nullptr for any slot you don't want to constrain.
     * Slots that are not matched retain their discovery order after all mapped slots.
     *
     * @param addrs  Array of N pointers to DeviceAddress. nullptr = leave slot unconstrained.
     * @return Number of addresses successfully matched and mapped (0..N).
     */
    uint8_t reorderByAddresses(const DeviceAddress* const addrs[N])
    {
        uint8_t mapped = 0;
        for (uint8_t target = 0; target < N && target < _sensorCount; ++target)
        {
            if (addrs[target] == nullptr) continue;
            for (uint8_t i = target; i < _sensorCount; ++i)
            {
                if (memcmp(_addresses[i], addrs[target], sizeof(DeviceAddress)) == 0)
                {
                    if (i != target) swapSlots(target, i);
                    ++mapped;
                    break;
                }
            }
        }
        return mapped;
    }

    /**
     * @brief Push cached temperatures into HA entity wrappers and publish to MQTT.
     *
     * Only slots with a physical sensor (index < sensorCount()) are published.
     *
     * @param force  Pass true to publish even if the value has not changed.
     */
    bool publishValue(const bool force = false) override
    {
        bool ok = true;
        publishImpl(force, ok, std::make_index_sequence<N>{});
        return ok;
    }

    /**
     * @brief Trigger a temperature conversion and cache results.
     *        Called by IoTDevice::updateAllComponents() each update cycle.
     */
    bool update(bool /*force*/ = false) override { return requestTemperatures(); }

    /**
     * @brief Return a JSON fragment for all live sensor slots.
     *        Returns empty String when no sensors are present.
     */
    String statusJSON() const override
    {
        if (_sensorCount == 0) return String();
        String result;
        statusImpl(result, std::make_index_sequence<N>{});
        return result;
    }

protected:
    /**
     * @brief Scan the bus, discover up to N Dallas temperature sensors,
     *        set 12-bit resolution and configure HA entity units.
     *        Called automatically by IoTDevice::preSetup().
     */
    void begin() override
    {
        _dallasTemp.begin();
        _dallasTemp.setResolution(12);

        const char* unit = AppSettings::temperatureUnit(!m_useFahrenheit);
        forEachSlot([&](auto& w){ w.setUnitOfMeasurement(unit); },
                    std::make_index_sequence<N>{});

        _sensorCount = 0;
        const uint8_t found = _dallasTemp.getDeviceCount();
        for (uint8_t i = 0; i < found && _sensorCount < N; ++i)
        {
            DeviceAddress addr;
            if (_dallasTemp.getAddress(addr, i) && _dallasTemp.validFamily(addr))
            {
                memcpy(_addresses[_sensorCount], addr, sizeof(DeviceAddress));
                ++_sensorCount;
            }
        }
        IOTLOGINFO1(F("DS18B20: sensors found:"), _sensorCount);
    }

private:
    // _oneWire must be declared before _dallasTemp (member init order = declaration order).
    OneWire           _oneWire;
    DallasTemperature _dallasTemp;
    uint8_t           _sensorCount;
    DeviceAddress     _addresses[N];
    float             _temperatures[N];
    bool              m_useFahrenheit = false;

    void swapSlots(uint8_t a, uint8_t b)
    {
        DeviceAddress tmp;
        memcpy(tmp,           _addresses[a], sizeof(DeviceAddress));
        memcpy(_addresses[a], _addresses[b], sizeof(DeviceAddress));
        memcpy(_addresses[b], tmp,           sizeof(DeviceAddress));
        float tmpT       = _temperatures[a];
        _temperatures[a] = _temperatures[b];
        _temperatures[b] = tmpT;
    }

    // -----------------------------------------------------------------------
    // Compile-time slot iteration via index_sequence + C++17 fold expressions.
    // These mirror the pattern already used in IoTHACompositeDeviceWrapper.
    // -----------------------------------------------------------------------

    template<typename Fn, size_t... Is>
    void forEachSlot(Fn fn, std::index_sequence<Is...>)
    {
        (fn(this->template get<Is>()), ...);
    }

    template<size_t... Is>
    void publishImpl(bool force, bool& ok, std::index_sequence<Is...>)
    {
        const bool inCelsius = !m_useFahrenheit;
        ([&]{
            if (Is < _sensorCount)
            {
                this->template get<Is>().setCurrentValue(
                    AppSettings::convertTemperature(_temperatures[Is], inCelsius));
                ok &= this->template get<Is>().publishValue(force);
            }
        }(), ...);
    }

    template<size_t... Is>
    void statusImpl(String& result, std::index_sequence<Is...>) const
    {
        const bool  inCelsius = !m_useFahrenheit;
        const char* unit      = AppSettings::temperatureUnit(inCelsius);
        ([&]{
            if (Is < _sensorCount)
            {
                if (!result.isEmpty()) result += ',';
                float       val  = AppSettings::convertTemperature(_temperatures[Is], inCelsius);
                const char* name = this->template get<Is>().name();
                bool        hasName = name && name[0] != '\0';
                String obj;
                if (hasName) obj += JSONUtils::Pair(F("name"), name, true);
                obj += JSONUtils::Pair(F("value"), val, !hasName);
                obj += JSONUtils::Pair(F("unit"), unit);
                result += JSONUtils::EncloseObject(obj);
            }
        }(), ...);
    }
};

#endif // IOTDS18B20MULTISENSOR_H
