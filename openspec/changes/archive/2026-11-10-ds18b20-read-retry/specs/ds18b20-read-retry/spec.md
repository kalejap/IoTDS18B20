## Purpose

Improves reliability of DS18B20 temperature reads by retrying up to 3 times
on transient 1-Wire bus failures, so a single bad read does not propagate
-127 °C to Home Assistant or downstream calculations.

## ADDED Requirements

### Requirement: Temperature read is retried on transient failure

When `requestTemperatures()` reads `INVALID_TEMPERATURE` (-127 °C) for a
sensor, the system SHALL retry the read up to 2 additional times (3 attempts
total) before accepting the failure for that cycle. Each retry SHALL yield
to the scheduler between attempts so that background tasks (WiFi stack,
AsyncWebServer) are not starved.

#### Scenario: First read fails, retry succeeds

- **WHEN** the first `getTempC()` call returns `INVALID_TEMPERATURE`
- **AND** a subsequent retry returns a valid temperature
- **THEN** the valid temperature SHALL be stored and the sensor SHALL NOT be
  marked invalid for that cycle

#### Scenario: All attempts fail

- **WHEN** all 3 attempts return `INVALID_TEMPERATURE`
- **THEN** `INVALID_TEMPERATURE` SHALL be stored for that sensor slot and
  the failure SHALL be logged

#### Scenario: First read succeeds

- **WHEN** the first `getTempC()` call returns a valid temperature
- **THEN** no retry SHALL be performed and the value SHALL be stored immediately

### Requirement: Retry applies to both sensor classes

The retry behaviour SHALL apply identically to `IoTDS18B20MultiSensor`
(all N slots) and `IoTDS18B20SingleSensor`.

#### Scenario: Multi-sensor partial failure

- **WHEN** one slot returns `INVALID_TEMPERATURE` on the first attempt and
  another slot reads successfully on the first attempt
- **THEN** only the failing slot SHALL be retried; the successful slot SHALL
  NOT trigger a second conversion

### Requirement: Background tasks are not starved during retry

Between each retry attempt the system SHALL call `yield()` to allow the
ESP8266 SDK and AsyncWebServer to process pending events.

#### Scenario: Yield between retries

- **WHEN** a retry is about to be attempted
- **THEN** `yield()` SHALL be called before the next conversion request
