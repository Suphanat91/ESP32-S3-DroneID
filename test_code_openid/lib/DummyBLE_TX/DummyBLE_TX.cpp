#include "DummyBLE_TX.h"

static BLEAdvertising *pAdvertising;

DummyBLE_TX::DummyBLE_TX() {}

bool DummyBLE_TX::init() {
    if (initialised) return true;
    initialised = true;

    BLEDevice::init("");  // No device name, pure beacon

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    memset(&msg_counters, 0, sizeof(msg_counters));

    fill_dummy_data();

    return true;
}

void DummyBLE_TX::fill_dummy_data() {
    odid_initUasData(&UAS_data);

    // Dummy Basic ID
    UAS_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    UAS_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strcpy((char*)UAS_data.BasicID[0].UASID, "DUMMY1234567890");
    UAS_data.BasicIDValid[0] = 1;

    // Dummy Location
    UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
    UAS_data.Location.Direction = 90.0;
    UAS_data.Location.SpeedHorizontal = 5.0;
    UAS_data.Location.Latitude = 13.7563;
    UAS_data.Location.Longitude = 100.5018;
    UAS_data.Location.AltitudeBaro = 50.0;
    UAS_data.LocationValid = 1;

    // Dummy Operator ID
    UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    strcpy((char*)UAS_data.OperatorID.OperatorId, "TH123456789");
    UAS_data.OperatorIDValid = 1;

    // Dummy Self ID
    UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strcpy((char*)UAS_data.SelfID.Desc, "Test Drone - Bangkok");
    UAS_data.SelfIDValid = 1;
}

bool DummyBLE_TX::transmit_longrange() {
    init();

    uint8_t payload[250];
    int length = odid_message_build_pack(&UAS_data, payload, sizeof(payload));
    if (length <= 0) return false;

    const uint8_t header[] = {
        uint8_t(length + 5), 0x16, 0xfa, 0xff, 0x0d,
        uint8_t(msg_counters[ODID_MSG_COUNTER_PACKED]++)
    };

    memcpy(longrange_payload, header, sizeof(header));
    memcpy(&longrange_payload[sizeof(header)], payload, length);
    int adv_length = sizeof(header) + length;

    BLEAdvertisementData adv;
    adv.addData(std::string((char*)longrange_payload, adv_length));
    pAdvertising->setAdvertisementData(adv);

    if (!started) {
        BLEDevice::startAdvertising();
        started = true;
    }

    return true;
}

bool DummyBLE_TX::transmit_legacy() {
    init();

    // Send only Basic ID as example
    ODID_BasicID_encoded basicid_encoded;
    if (encodeBasicIDMessage(&basicid_encoded, &UAS_data.BasicID[0]) != ODID_SUCCESS) {
        return false;
    }

    const uint8_t header[] = { 0x1e, 0x16, 0xfa, 0xff, 0x0d };
    memcpy(legacy_payload, header, sizeof(header));
    memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_BASIC_ID], 1);
    msg_counters[ODID_MSG_COUNTER_BASIC_ID]++;
    memcpy(&legacy_payload[sizeof(header) + 1], &basicid_encoded, sizeof(basicid_encoded));

    int adv_length = sizeof(header) + 1 + sizeof(basicid_encoded);

    BLEAdvertisementData adv;
    adv.addData(std::string((char*)legacy_payload, adv_length));
    pAdvertising->setAdvertisementData(adv);

    if (!started) {
        BLEDevice::startAdvertising();
        started = true;
    }

    return true;
}

uint8_t DummyBLE_TX::dBm_to_tx_power(float dBm) const {
    static const struct {
        uint8_t level;
        float dBm;
    } table[] = {
        { ESP_PWR_LVL_N24, -24 }, { ESP_PWR_LVL_N21, -21 }, { ESP_PWR_LVL_N18, -18 },
        { ESP_PWR_LVL_N15, -15 }, { ESP_PWR_LVL_N12, -12 }, { ESP_PWR_LVL_N9, -9 },
        { ESP_PWR_LVL_N6,  -6 },  { ESP_PWR_LVL_N3, -3 },   { ESP_PWR_LVL_N0, 0 },
        { ESP_PWR_LVL_P3,   3 },  { ESP_PWR_LVL_P6, 6 },    { ESP_PWR_LVL_P9, 9 },
        { ESP_PWR_LVL_P12, 12 },  { ESP_PWR_LVL_P15, 15 },  { ESP_PWR_LVL_P18, 18 }
    };
    for (auto &t : table) if (dBm <= t.dBm) return t.level;
    return ESP_PWR_LVL_P18;
}
