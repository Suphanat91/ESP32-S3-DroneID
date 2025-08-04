#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <opendroneid.h>

#define SCAN_TIME  5  // seconds

BLEScan* pBLEScan;

// Decode buffer
uint8_t payload[25] = {0};

// Check for ODID UUID 0xFFFA in service data
bool extract_odid_payload(BLEAdvertisedDevice advertisedDevice, uint8_t* outPayload) {
    const uint8_t* data = advertisedDevice.getPayload();
    int len = advertisedDevice.getPayloadLength();

    for (int i = 0; i < len - 28; ++i) {
        if (data[i] == 0x16 && data[i+1] == 0xFA && data[i+2] == 0xFF) {
            memcpy(outPayload, &data[i+3], 25);
            return true;
        }
    }
    return false;
}

void decode_odid_payload(const uint8_t* raw) {
    uint8_t msg_type = raw[0];

    switch (msg_type) {
        case ODID_MESSAGETYPE_BASIC_ID: {
            ODID_BasicID_data basic;
            if (decodeBasicIDMessage(&basic, (ODID_BasicID_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[BasicID]");
                Serial.printf("  UAS ID: %s\n", basic.UASID);
                Serial.printf("  UA Type: %d\n", basic.UAType);
                Serial.printf("  ID Type: %d\n", basic.IDType);
            }
            break;
        }
        case ODID_MESSAGETYPE_LOCATION: {
            ODID_Location_data loc;
            if (decodeLocationMessage(&loc, (ODID_Location_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[Location]");
                Serial.printf("  Lat: %.6f\n", loc.Latitude);
                Serial.printf("  Lon: %.6f\n", loc.Longitude);
                Serial.printf("  Alt: %.2f m\n", loc.AltitudeBaro);
                Serial.printf("  H Speed: %.2f m/s\n", loc.SpeedHorizontal);
                Serial.printf("  V Speed: %.2f m/s\n", loc.SpeedVertical);
            }
            break;
        }
        case ODID_MESSAGETYPE_AUTH: {
            ODID_Auth_data auth;
            if (decodeAuthMessage(&auth, (ODID_Auth_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[Auth]");
                Serial.printf("  Page: %d\n", auth.DataPage);
                Serial.print("  Data: ");
                for (int i = 0; i < sizeof(auth.AuthData); i++) {
                    Serial.printf("%02X ", auth.AuthData[i]);
                }
                Serial.println();
            }
            break;
        }
        case ODID_MESSAGETYPE_SELF_ID: {
            ODID_SelfID_data self;
            if (decodeSelfIDMessage(&self, (ODID_SelfID_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[SelfID]");
                Serial.printf("  Desc: %s\n", self.Desc);
            }
            break;
        }
        case ODID_MESSAGETYPE_SYSTEM: {
            ODID_System_data sys;
            if (decodeSystemMessage(&sys, (ODID_System_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[System]");
                Serial.printf("  Area Radius: %.1f m\n", sys.AreaRadius);
                Serial.printf("  Floor: %.1f m\n", sys.AreaFloor);
                Serial.printf("  Ceiling: %.1f m\n", sys.AreaCeiling);
            }
            break;
        }
        case ODID_MESSAGETYPE_OPERATOR_ID: {
            ODID_OperatorID_data op;
            if (decodeOperatorIDMessage(&op, (ODID_OperatorID_encoded*)raw) == ODID_SUCCESS) {
                Serial.println("[OperatorID]");
                Serial.printf("  ID: %s\n", op.OperatorId);
            }
            break;
        }
        default:
            Serial.printf("[Unknown ODID message type: 0x%02X]\n", msg_type);
            break;
    }
}

// Callback when each device is discovered
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (extract_odid_payload(advertisedDevice, payload)) {
            Serial.println("\n--- OpenDroneID Message Received ---");
            decode_odid_payload(payload);
        }
    }
};

void setup() {
    Serial.begin(57600);
    Serial.println("Starting OpenDroneID BLE Scanner...");

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
}

void loop() {
    BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
    pBLEScan->clearResults();
    delay(500);
}
