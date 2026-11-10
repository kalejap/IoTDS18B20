## Context

Both `IoTDS18B20MultiSensor::requestTemperatures()` and
`IoTDS18B20SingleSensor::requestTemperatures()` currently call
`_dallasTemp.requestTemperatures()` once and read each sensor address with
`getTempC()`. On a transient 1-Wire failure `getTempC()` returns
`DEVICE_DISCONNECTED_C` (-127 °C). The read result is stored directly with
no retry; -127 propagates to HA and invalidates downstream power/heat calculations.

`IoTDS18B20SingleSensor::begin()` already uses a retry loop with `delay(200)`
for bus initialisation — this change applies the same philosophy to per-cycle reads.

The ESP8266 runs cooperative multi-tasking. `_dallasTemp.requestTemperatures()`
blocks ~750 ms at 12-bit resolution (conversion time). Adding retry naively would
stall the loop for up to 3 × 750 ms. Using `yield()` before each retry attempt
(not between request and read) lets the SDK flush pending callbacks without
introducing a fixed delay.

## Goals / Non-Goals

**Goals:**
- Recover from single-cycle transient 1-Wire errors automatically.
- Keep background tasks (WiFi, AsyncWebServer) alive during retries.
- Apply consistently to both sensor classes.

**Non-Goals:**
- Configurable retry count — 3 is hardcoded (matches the initialisation retry).
- Persistent error counting or alerting on repeated failures.
- Fixing hardware or cabling issues.

## Decisions

### 1 — Retry per sensor slot, not per bus conversion

In `IoTDS18B20MultiSensor`, retry is done per slot independently: if slot 0
reads fine and slot 1 returns -127, only slot 1 triggers a retry (a new
`requestTemperatures()` + `getTempC()` for that address). Retrying the entire
bus conversion for one bad slot would re-read all slots and waste ~750 ms per
retry unnecessarily.

*Alternative considered:* retry the entire `_dallasTemp.requestTemperatures()`
call if any slot fails — rejected because it blocks all slots for extra time
when only one is flaky.

### 2 — `yield()` before each retry, not `delay()`

`yield()` returns control to the SDK immediately and comes back as soon as the
callback queue is drained — typically under 1 ms. `delay(N)` blocks for a fixed
duration. Since the retry path already re-triggers a full 750 ms conversion,
introducing extra fixed delay is unnecessary.

*Alternative considered:* `delay(10)` — rejected because it adds latency without
benefit when `yield()` is sufficient to drain the callback queue.

### 3 — Extract helper `readTempWithRetry()` (private static/free)

Both classes share identical retry logic. A private helper avoids duplication
and makes the intent clear at the call site. Defined as a private static method
in each class (not a shared base) to avoid a new dependency between the two.

## Risks / Trade-offs

- **Increased cycle time on failure**: each retry adds ~750 ms (conversion time).
  With 3 attempts the worst case per slot is ~2.25 s, up from ~750 ms. This is
  acceptable because failures are transient and infrequent; the 15 s update timer
  absorbs it.
- **`yield()` does not guarantee full processing**: if a large burst of events
  arrives during a retry storm, some callbacks may be delayed. This is an
  inherent limitation of cooperative multi-tasking on ESP8266 and not worsened
  by this change.
