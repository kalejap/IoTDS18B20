# IoTDS18B20

Arduino/ESP8266 library for DS18B20 temperature sensors with single-sensor and multi-sensor (up to 8) support, NVS-backed slot assignment, a web configuration UI, and Home Assistant / MQTT publishing.

## Features

- **Single-sensor** wrapper (`IoTDS18B20SingleSensor`) — simple drop-in for one DS18B20
- **Multi-sensor template** (`IoTDS18B20MultiSensor<N>`) — up to 8 sensors on one OneWire bus
- **NVS persistence** (`DS18B20Settings<N>`) — saves ROM address → slot mapping across reboots
- **Web config UI** (`DS18B20ConfigScript`) — interactive sensor ordering and live temperature display served from the device portal
- **Home Assistant / MQTT integration** — publishes values as HA sensor entities via `IoTHASensorNumberWrapper`
- **Fahrenheit / Celsius** selectable per instance
- **ESP8266 iCache workaround** — 3-attempt retry with delay in `begin()` to survive OneWire bus scan failures after WiFi initialisation
- Header-only; no `.cpp` compilation units required

## Dependencies

- [OneWire](https://github.com/PaulStoffregen/OneWire)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- IoT framework base classes: `IoTHASensorNumberWrapper`, `NCompositeWrapper`, `AppSettings` (from the IoT framework library)

## Installation

Clone or copy this repository into your Arduino `libraries/` folder or your PlatformIO `lib/` directory:

```bash
git clone https://github.com/kalejap/IoTDS18B20
```

Or add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/kalejap/IoTDS18B20
```

## Usage

### Single Sensor

```cpp
#include <IoTDS18B20SingleSensor.h>

// GPIO pin connected to DS18B20 data line
IoTDS18B20SingleSensor sensor(D4);

void setup() {
    sensor.setUseFahrenheit(false);   // optional, default is Celsius
    sensor.begin();
    if (!sensor.sensorFound()) {
        Serial.println("No DS18B20 found");
    }
}

void loop() {
    sensor.requestTemperatures();
    float tempC = sensor.getTemperatureC();
    if (tempC != IoTDS18B20SingleSensor::INVALID_TEMPERATURE) {
        Serial.println(tempC);
    }
    sensor.publishValue(false);       // push to HA / MQTT if value changed
    delay(5000);
}
```

### Multi-Sensor (up to N sensors)

```cpp
#include <IoTDS18B20MultiSensor.h>

// 2 sensor slots on GPIO D4
IoTDS18B20MultiSensor<2> sensors(D4);

void setup() {
    sensors.setUseFahrenheit(false);
    sensors.begin();
    Serial.print("Found ");
    Serial.print(sensors.sensorCount());
    Serial.println(" sensor(s)");
}

void loop() {
    sensors.requestTemperatures();
    for (int i = 0; i < sensors.sensorCount(); i++) {
        if (sensors.isValid(i)) {
            Serial.println(sensors.getTemperatureC(i));
        }
    }
    sensors.publishValue(false);
    delay(5000);
}
```

## Class Reference

### `IoTDS18B20SingleSensor`

| Method | Description |
|---|---|
| `setUseFahrenheit(bool)` | Select temperature unit before `begin()` |
| `sensorFound()` | Returns `true` if `begin()` discovered a sensor |
| `requestTemperatures()` | Trigger conversion and cache result (~750 ms at 12-bit) |
| `getTemperatureC()` | Return cached temperature in Celsius |
| `publishValue(force)` | Push value to HA entity / MQTT |
| `update(force)` | Called by IoTDevice; triggers `requestTemperatures()` |
| `statusJSON()` | JSON fragment describing this sensor |
| `INVALID_TEMPERATURE` | Sentinel value (`DEVICE_DISCONNECTED_C`, −127 °C) |

---

### `IoTDS18B20MultiSensor<N>`

Template parameter `N`: number of sensor slots (1–8).

| Method | Description |
|---|---|
| `setUseFahrenheit(bool)` | Select temperature unit before `begin()` |
| `sensorCount()` | Number of sensors discovered on the bus |
| `isValid(index)` | Returns `true` if slot `index` has a physical sensor |
| `requestTemperatures()` | Trigger conversion for all sensors and cache results |
| `getTemperatureC(index)` | Return cached temperature for slot `index` |
| `getSensorAddress(index, addr)` | Copy 8-byte ROM address for slot `index` into `addr` |
| `reorderByAddresses(addrs[N])` | Remap internal slots to match known ROM addresses |
| `publishValue(force)` | Push all values to HA entities / MQTT |
| `update(force)` | Called by IoTDevice; triggers `requestTemperatures()` |
| `statusJSON()` | JSON fragment for all live sensors |
| `MAX_SENSORS` | Compile-time slot count (equals `N`) |
| `INVALID_TEMPERATURE` | Sentinel value (`DEVICE_DISCONNECTED_C`, −127 °C) |

---

### `DS18B20Settings<N>`

NVS-backed persistence for slot → ROM address mapping.

| Method | Description |
|---|---|
| `isConfigured()` | `true` if at least one slot has a saved address |
| `getSlotAddress(slot, addr)` | Retrieve stored ROM address for `slot` |
| `setSlotAddress(slot, addr)` | Save ROM address for `slot`; marks dirty if changed |
| `addressToHex(addr)` | Convert 8-byte ROM to 16-char uppercase hex string |
| `hexToAddress(hex, addr)` | Parse 16-char hex string to 8-byte ROM address |

NVS namespace: `DS18B20`. Keys: `SET<i>` (bool), `ADDR<i>` (8 bytes).

---

### `DS18B20ConfigScript`

PROGMEM constants for the web configuration portal.

| Constant | Description |
|---|---|
| `DS18B20_HEAD_ELEMENTS` | CSS and `<script>` tag to inject into the portal `<head>` |
| `DS18B20_CONFIG_JS` | JavaScript served at `/ds18b20.js`; provides `ds18Render()`, `ds18Up()`, `ds18Dn()`, `ds18UpdateTemps()`, `ds18Save()`, `ds18Init()` |

## License

MIT License — Copyright (c) 2026 Peter Kaleja. See [LICENSE](LICENSE).
