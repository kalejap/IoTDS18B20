# IoTDS18B20

Arduino / ESP8266 library for DS18B20 temperature sensors with single-sensor and
multi-sensor (up to 8) support, NVS-backed slot assignment, a self-contained web
configuration UI, localisation, and Home Assistant / MQTT publishing.

## Features

- **Single-sensor** wrapper (`IoTDS18B20SingleSensor`) — simple drop-in for one DS18B20
- **Multi-sensor template** (`IoTDS18B20MultiSensor<N>`) — up to 8 sensors on one OneWire bus
- **NVS persistence** (`DS18B20Settings<N>`) — saves ROM address → slot mapping across reboots; owned by `IoTDS18B20MultiSensor`, no separate member needed in the device class
- **Self-contained web UI** — standalone HTML+JS page (`/ds18b20config`) served entirely from PROGMEM; interactive sensor ordering with live temperature refresh; no portal-side HTML required
- **REST API** — `/api/ds18b20/slots`, `/api/ds18b20/sensors`, `/api/ds18b20/assign`
- **Localisation** — English (default) and Slovak via `LANGUAGE_SK_SK` build flag
- **Home Assistant / MQTT integration** — publishes values as HA sensor entities via `IoTHASensorNumberWrapper`
- **Fahrenheit / Celsius** selectable per instance
- **ESP8266 iCache workaround** — 3-attempt retry with delay in `begin()` to survive OneWire bus scan failures after WiFi initialisation
- Header-only; no `.cpp` compilation units required

---

## Dependencies

| Library | Purpose |
|---|---|
| `OneWire` | OneWire bus communication |
| `DallasTemperature` | DS18B20 protocol |
| `ESPAsyncWebServer` | Web config UI routes (multi-sensor only) |
| `IoTApplication` framework | `IoTHASensorNumberWrapper`, `NCompositeWrapper`, `AppSettings`, `Settings` |

---

## Installation

```ini
; platformio.ini
lib_deps =
    https://github.com/kalejap/IoTDS18B20
```

---

## Quick start

### Single sensor

```cpp
#include <IoTDS18B20SingleSensor.h>

IoTDS18B20SingleSensor sensor(D4, "temp_room");

// In IoTDevice::postSetup():
sensor.setName("Room temperature");
sensor.setDeviceClass("temperature");

// In IoTDevice::preLoop() via a Timer:
sensor.requestTemperatures();
sensor.publishValue();
```

### Multi-sensor with web config UI

```cpp
#include <IoTDS18B20MultiSensor.h>

IoTDS18B20MultiSensor<2> tempBus(D4, "uid_input", "uid_output");

// In IoTDevice constructor:
registerComponent(tempBus);

// In IoTDevice::postSetup():
tempBus.get<0>().setName("Input temperature");
tempBus.get<1>().setName("Output temperature");
tempBus.loadSettings();   // apply saved slot → address mapping from NVS

// In IoTDevice::onAddConfigRoutes():
tempBus.registerWebRoutes(server);

// Return the nav button on the index page:
PGM_P onCustomIndexButtons() override {
    return IoTDS18B20MultiSensor<2>::indexPageButton();
}

// In IoTDevice::preLoop() via a Timer:
tempBus.requestTemperatures();
tempBus.publishValue();
```

---

## Class reference

### `IoTDS18B20SingleSensor`

| Method | Description |
|---|---|
| `IoTDS18B20SingleSensor(pin, uid)` | Constructor |
| `setUseFahrenheit(bool)` | Select unit before `begin()` |
| `sensorFound()` | `true` if `begin()` found a sensor |
| `requestTemperatures()` | Trigger conversion and cache result (~750 ms) |
| `getTemperatureC()` | Cached temperature in °C |
| `publishValue(force)` | Push value to HA / MQTT |
| `update(force)` | Called by IoTDevice; calls `requestTemperatures()` |
| `statusJSON()` | JSON fragment for the web status table |
| `INVALID_TEMPERATURE` | `DEVICE_DISCONNECTED_C` (−127 °C) sentinel |

---

### `IoTDS18B20MultiSensor<N>`

Template parameter `N`: number of sensor slots (1–8). Inherits all HA entity
accessors via `get<I>()` from `IoTHACompositeDeviceWrapper`.

#### Sensor bus

| Method | Description |
|---|---|
| `IoTDS18B20MultiSensor(pin, uid0, uid1, ...)` | Constructor — exactly N uid strings required |
| `setUseFahrenheit(bool)` | Select unit before `begin()` |
| `sensorCount()` | Sensors found on the bus |
| `isValid(index)` | `true` if slot `index` has a physical sensor |
| `requestTemperatures()` | Convert and cache all sensors |
| `getTemperatureC(index)` | Cached temperature for slot `index` |
| `getSensorAddress(index, addr)` | Copy 8-byte ROM address into `addr` |
| `reorderByAddresses(addrs[N])` | Remap slots to match known ROM addresses |

#### Settings and web integration

| Method | Description |
|---|---|
| `loadSettings()` | Read NVS slot→address map and call `reorderByAddresses()`; call from `postSetup()` |
| `registerWebRoutes(server)` | Register all DS18B20 HTTP routes; call from `onAddConfigRoutes()` |
| `indexPageButton()` | `static` — PROGMEM `<a>` element for the portal index page; return from `onCustomIndexButtons()` |
| `getSlotName(index)` | Runtime slot name lookup |

#### HA / MQTT

| Method | Description |
|---|---|
| `get<I>()` | Access the I-th `IoTHASensorNumberWrapper<float>` entity (0-based, compile-time) |
| `publishValue(force)` | Push all slot values to MQTT |
| `update(force)` | Called by IoTDevice; calls `requestTemperatures()` |
| `statusJSON()` | JSON fragment for all live sensors |

#### Web API endpoints (registered by `registerWebRoutes()`)

| Endpoint | Method | Description |
|---|---|---|
| `/ds18b20config` | GET | Full sensor assignment page (PROGMEM HTML) |
| `/api/ds18b20/slots` | GET | JSON array of slot names and saved ROM addresses |
| `/api/ds18b20/sensors` | GET | JSON array of live physical sensors and temperatures |
| `/api/ds18b20/assign` | GET | Save slot→address mapping (`?s0=ADDR&s1=ADDR…`) |

---

### `DS18B20Settings<N>`

NVS-backed persistence for slot → ROM address mapping.
Owned internally by `IoTDS18B20MultiSensor` — no separate member needed in application code.

| Method | Description |
|---|---|
| `isConfigured()` | `true` if at least one slot has a saved address |
| `getSlotAddress(slot, addr)` | Retrieve stored ROM address |
| `setSlotAddress(slot, addr)` | Save ROM address; marks dirty if changed |
| `addressToHex(addr)` | 8-byte ROM → 16-char uppercase hex |
| `hexToAddress(hex, addr)` | 16-char hex → 8-byte ROM |

NVS namespace: `DS18B20`. Per-slot keys: `SET<i>` (bool), `ADDR<i>` (8 bytes).

---

## Localisation

Define `LANGUAGE_SK_SK` in your build flags to switch the web UI to Slovak.
English is the default.

```ini
; platformio.ini
build_flags =
    -D LANGUAGE_SK_SK
```

| String | English | Slovak |
|---|---|---|
| Page title | DS18B20 Sensor Assignment | Priradenie senzorov DS18B20 |
| Save button | Save assignment | Ulozit priradenie |
| Saved message | Saved! | Ulozene! |
| Index nav button | Configure DS18B20 Sensors | Konfiguracia senzorov DS18B20 |

---

## License

LGPL 2.1 — Copyright (c) 2026 Peter Kaleja. See [LICENSE](LICENSE).
