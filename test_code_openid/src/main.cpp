#include <Arduino.h>
#include "BLE_TX.h"
#include "opendroneid.h"
#include <BLEDevice.h>

static BLE_TX ble;
ODID_UAS_Data UAS_data;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting OpenDroneID dummy broadcast...");

    // Initialize UAS data structure
    odid_initUasData(&UAS_data);

    // BasicID
    UAS_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    UAS_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strcpy((char*)UAS_data.BasicID[0].UASID, "DUMMY1234567890");
    UAS_data.BasicIDValid[0] = 1;

    // Location
    UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
    UAS_data.Location.Direction = 45.0;
    UAS_data.Location.SpeedHorizontal = 10.0;
    UAS_data.Location.SpeedVertical = 0.0;
    UAS_data.Location.Latitude = 13.7563;
    UAS_data.Location.Longitude = 100.5018;
    UAS_data.Location.AltitudeBaro = 50.0;
    UAS_data.Location.AltitudeGeo = 49.0;
    UAS_data.Location.Height = 10.0;
    UAS_data.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    UAS_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
    UAS_data.Location.VertAccuracy = ODID_VER_ACC_10_METER;
    UAS_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    UAS_data.Location.TSAccuracy = ODID_TIME_ACC_0_9_SECOND;
    UAS_data.Location.TimeStamp = 123.4;
    UAS_data.LocationValid = 1;

    // Operator IDODID_OPERATOR_ID
    UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    strcpy((char*)UAS_data.OperatorID.OperatorId, "TH123456789");
    UAS_data.OperatorIDValid = 1;

    // Self ID
    UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strcpy((char*)UAS_data.SelfID.Desc, "Test Drone - Bangkok");
    UAS_data.SelfIDValid = 1;

    // No LED needed
}

void loop() {
    static uint32_t last_bt5 = 0;
    static uint32_t last_bt4 = 0;
    uint32_t now = millis();

    // // Send BT5 (Long-range) every 1 sec
    // if (now - last_bt5 > 1000) {
    //     last_bt5 = now;
    //     ble.transmit_longrange(UAS_data);
    // }

    // Send BT4 (Legacy) in 6-frame cycle (~167ms per frame)
    static int bt4_phase = 0;
    static const int bt4_frames = 6;
    if (now - last_bt4 > (1000 / bt4_frames)) {
        last_bt4 = now;
        ble.transmit_legacy(UAS_data);
        bt4_phase = (bt4_phase + 1) % bt4_frames;
    }

    delay(1);
}
