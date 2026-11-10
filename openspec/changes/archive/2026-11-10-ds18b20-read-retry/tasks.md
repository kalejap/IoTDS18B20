## 1. IoTDS18B20MultiSensor

- [x] 1.1 Add private static helper `readTempWithRetry(DallasTemperature&, const DeviceAddress, uint8_t maxAttempts)` that calls `requestTemperatures()` + `getTempC()`, calls `yield()` before each retry, and returns the first valid reading or `INVALID_TEMPERATURE`
- [x] 1.2 Replace the `getTempC()` call in `requestTemperatures()` for each slot with a call to `readTempWithRetry()` (up to 3 attempts)
- [x] 1.3 Log a warning when all attempts fail, including slot index and attempt count

## 2. IoTDS18B20SingleSensor

- [x] 2.1 Add the same private static helper `readTempWithRetry()` to `IoTDS18B20SingleSensor`
- [x] 2.2 Replace the `getTempC()` call in `requestTemperatures()` with a call to `readTempWithRetry()` (up to 3 attempts)
- [x] 2.3 Log a warning when all attempts fail

## 3. Verify

- [x] 3.1 Build `SolidFuelBoilerController` (debug env) — zero errors and warnings
- [x] 3.2 Flash device and confirm temperatures are read correctly under normal conditions
- [x] 3.3 Simulate a transient failure (briefly disconnect sensor) and confirm recovery on the next cycle without -127 propagating to HA
