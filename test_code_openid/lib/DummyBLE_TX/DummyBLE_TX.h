#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <opendroneid.h>

class DummyBLE_TX {
public:
    DummyBLE_TX();
    bool init();
    bool transmit_longrange();
    bool transmit_legacy();

private:
    void fill_dummy_data();
    uint8_t dBm_to_tx_power(float dBm) const;

    bool initialised = false;
    bool started = false;

    ODID_UAS_Data UAS_data;
    uint8_t msg_counters[10]{};

    uint8_t longrange_payload[255];
    uint8_t legacy_payload[255];
};
