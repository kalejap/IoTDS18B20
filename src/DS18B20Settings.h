/*
  DS18B20Settings.h - NVS-backed slot→ROM-address mapping for DS18B20 sensors.
  Copyright (c) 2024 Peter Kaleja.  All right reserved.

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

#pragma once

#include <DallasTemperature.h>
#include <Settings.h>

/**
 * @class DS18B20Settings
 * @brief Persists a slot→ROM-address assignment for N DS18B20 sensor slots to NVS.
 *
 * NVS namespace: "DS18B20"
 * Per-slot keys:  "SET<i>"  bool    — whether slot i has a saved address
 *                 "ADDR<i>" bytes   — 8-byte ROM address for slot i
 *
 * Header-only because it is a template (instantiated per N in the application TU).
 *
 * @tparam N  Number of sensor slots (must match IoTDS18B20MultiSensor<N>).
 */
template<size_t N>
class DS18B20Settings : public Settings
{
public:
    DS18B20Settings() : Settings(namespaceName()) {}

    /**
     * @brief True if at least one slot has a saved address.
     */
    bool isConfigured() const
    {
        for (size_t i = 0; i < N; ++i)
            if (m_slots[i].set) return true;
        return false;
    }

    /**
     * @brief Copy the saved ROM address for a slot into addr.
     * @param slot  Slot index (0..N-1).
     * @param addr  Output buffer (DeviceAddress / uint8_t[8]).
     * @return true if the slot has a saved address.
     */
    bool getSlotAddress(size_t slot, DeviceAddress addr) const
    {
        if (slot >= N || !m_slots[slot].set) return false;
        memcpy(addr, m_slots[slot].addr, sizeof(DeviceAddress));
        return true;
    }

    /**
     * @brief Save a ROM address for a slot (marks dirty if changed).
     * @param slot  Slot index (0..N-1). Out-of-range values are ignored.
     * @param addr  8-byte ROM address to store.
     */
    void setSlotAddress(size_t slot, const DeviceAddress addr)
    {
        if (slot >= N) return;
        if (!m_slots[slot].set ||
            memcmp(m_slots[slot].addr, addr, sizeof(DeviceAddress)) != 0)
        {
            memcpy(m_slots[slot].addr, addr, sizeof(DeviceAddress));
            m_slots[slot].set = true;
            _isDirty = true;
        }
    }

    /**
     * @brief Convert an 8-byte ROM address to a 16-character uppercase hex string.
     */
    static String addressToHex(const DeviceAddress addr)
    {
        char buf[17];
        for (uint8_t i = 0; i < 8; ++i)
            sprintf(buf + i * 2, "%02X", addr[i]);
        buf[16] = '\0';
        return String(buf);
    }

    /**
     * @brief Parse a 16-character hex string into an 8-byte ROM address.
     * @return true on success; false if hex is not exactly 16 valid hex characters.
     */
    static bool hexToAddress(const String& hex, DeviceAddress addr)
    {
        if (hex.length() != 16) return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            uint8_t hi = hexNibble(hex.charAt(i * 2));
            uint8_t lo = hexNibble(hex.charAt(i * 2 + 1));
            if (hi == 0xFF || lo == 0xFF) return false;
            addr[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

protected:
    void readFields(Preferences& pref) override
    {
        for (size_t i = 0; i < N; ++i)
        {
            char setKey[6], addrKey[7];
            buildKeys(i, setKey, addrKey);
            m_slots[i].set = pref.getBool(setKey, false);
            if (m_slots[i].set)
                pref.getBytes(addrKey, m_slots[i].addr, sizeof(DeviceAddress));
        }
    }

    bool saveFields(Preferences& pref) const override
    {
        for (size_t i = 0; i < N; ++i)
        {
            char setKey[6], addrKey[7];
            buildKeys(i, setKey, addrKey);
            pref.putBool(setKey, m_slots[i].set);
            if (m_slots[i].set)
                pref.putBytes(addrKey, m_slots[i].addr, sizeof(DeviceAddress));
        }
        return true;
    }

private:
    struct SlotEntry
    {
        DeviceAddress addr;
        bool          set;
        SlotEntry() : set(false) { memset(addr, 0, sizeof(DeviceAddress)); }
    };

    SlotEntry m_slots[N];

    static const char* namespaceName()
    {
        static const char ns[] = "DS18B20";
        return ns;
    }

    static uint8_t hexNibble(char c)
    {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
        return 0xFF;
    }

    static void buildKeys(size_t i, char setKey[6], char addrKey[7])
    {
        sprintf(setKey,  "SET%u",  (unsigned)i);
        sprintf(addrKey, "ADDR%u", (unsigned)i);
    }
};
