## Why

`requestTemperatures()` in both `IoTDS18B20MultiSensor` and `IoTDS18B20SingleSensor`
reads each sensor exactly once. A transient 1-Wire bus error (noise, power glitch,
marginal cabling) causes `getTempC()` to return `-127 °C` (DEVICE_DISCONNECTED_C),
which propagates to Home Assistant and corrupts power/heat calculations that depend
on the temperature values.

## What Changes

- `IoTDS18B20MultiSensor::requestTemperatures()` — retry each individual sensor read
  up to 3 attempts when it returns `INVALID_TEMPERATURE`; call `yield()` between
  retries to keep background tasks (WiFi, AsyncWebServer) alive.
- `IoTDS18B20SingleSensor::requestTemperatures()` — same retry logic for the single
  sensor case.

## Capabilities

### New Capabilities

- `ds18b20-read-retry`: Temperature reads are retried up to 3 times on transient
  failure before the slot is marked invalid for that cycle.

### Modified Capabilities

## Impact

- `IoTDS18B20MultiSensor.h` — `requestTemperatures()` method body changes.
- `IoTDS18B20SingleSensor.h` — `requestTemperatures()` method body changes.
- No API changes — callers still call `requestTemperatures()` and read results via
  `getTemperatureC()`. Return value semantics are unchanged (`INVALID_TEMPERATURE`
  when all attempts fail).
