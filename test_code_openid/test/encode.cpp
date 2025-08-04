#include <Arduino.h>
#include <opendroneid.h>
#include "BLE_TX.h"

// BLE Transmitter
static BLE_TX ble;

// OpenDroneID Raw Data (individual message types)
ODID_BasicID_data BasicID;
ODID_Location_data Location;
ODID_SelfID_data SelfID;
ODID_System_data System_data;
ODID_OperatorID_data operatorID;

// Encoded Data
ODID_BasicID_encoded BasicID_enc;
ODID_Location_encoded Location_enc;
ODID_SelfID_encoded SelfID_enc;
ODID_System_encoded System_enc;
ODID_OperatorID_encoded OperatorID_enc;

// UAS Data structure for BLE_TX
ODID_UAS_Data UAS_data;

// Message state for cycling
enum MSG_STATE {
    SEND_BASIC_ID = 0,
    SEND_LOCATION,
    SEND_SELF_ID, 
    SEND_SYSTEM,
    SEND_OPERATOR_ID,
    MAX_STATES
};

static MSG_STATE current_state = SEND_BASIC_ID;

// Fill and encode all dummy data
void fill_and_encode_data() {
    Serial.println("Filling and encoding OpenDroneID data...");
    
    // Initialize UAS Data structure
    odid_initUasData(&UAS_data);
    
    // === BasicID ===
    odid_initBasicIDData(&BasicID);
    BasicID.IDType = ODID_IDTYPE_CAA_REGISTRATION_ID;
    BasicID.UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    char id[] = "12345678901234567890";
    strncpy(BasicID.UASID, id, sizeof(BasicID.UASID));
    Serial.println("✓ BasicID data filled");
    
    // Encode BasicID
    if (encodeBasicIDMessage(&BasicID_enc, &BasicID) == ODID_SUCCESS) {
        // Copy to UAS_data
        UAS_data.BasicIDValid[0] = 1;
        memcpy(&UAS_data.BasicID[0], &BasicID, sizeof(BasicID));
        Serial.println("✓ BasicID encoded successfully");
    } else {
        Serial.println("✗ BasicID encoding failed");
    }
    
    // === Location ===
    odid_initLocationData(&Location);
    Location.Status = ODID_STATUS_AIRBORNE;
    Location.Direction = 215.7f;
    Location.SpeedHorizontal = 5.4f;
    Location.SpeedVertical = 5.25f;
    Location.Latitude = 45.539309;
    Location.Longitude = -122.966389;
    Location.AltitudeBaro = 100;
    Location.AltitudeGeo = 110;
    Location.HeightType = ODID_HEIGHT_REF_OVER_GROUND;
    Location.Height = 80;
    Location.HorizAccuracy = createEnumHorizontalAccuracy(2.5f);
    Location.VertAccuracy = createEnumVerticalAccuracy(0.5f);
    Location.BaroAccuracy = createEnumVerticalAccuracy(1.5f);
    Location.SpeedAccuracy = createEnumSpeedAccuracy(0.5f);
    Location.TSAccuracy = createEnumTimestampAccuracy(0.2f);
    Location.TimeStamp = 360.52f;
    Serial.println("✓ Location data filled");
    
    // Encode Location
    if (encodeLocationMessage(&Location_enc, &Location) == ODID_SUCCESS) {
        // Copy to UAS_data
        UAS_data.LocationValid = 1;
        memcpy(&UAS_data.Location, &Location, sizeof(Location));
        Serial.println("✓ Location encoded successfully");
    } else {
        Serial.println("✗ Location encoding failed");
    }
    
    // === SelfID ===
    odid_initSelfIDData(&SelfID);
    SelfID.DescType = ODID_DESC_TYPE_TEXT;
    char description[] = "DronesRUS: Real Estate";
    strncpy(SelfID.Desc, description, sizeof(SelfID.Desc));
    Serial.println("✓ SelfID data filled");
    
    // Encode SelfID
    if (encodeSelfIDMessage(&SelfID_enc, &SelfID) == ODID_SUCCESS) {
        // Copy to UAS_data
        UAS_data.SelfIDValid = 1;
        memcpy(&UAS_data.SelfID, &SelfID, sizeof(SelfID));
        Serial.println("✓ SelfID encoded successfully");
    } else {
        Serial.println("✗ SelfID encoding failed");
    }
    
    // === System ===
    odid_initSystemData(&System_data);
    System_data.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    System_data.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
    System_data.OperatorLatitude = Location.Latitude + 0.00001;
    System_data.OperatorLongitude = Location.Longitude + 0.00001;
    System_data.AreaCount = 35;
    System_data.AreaRadius = 75;
    System_data.AreaCeiling = 176.9f;
    System_data.AreaFloor = 41.7f;
    System_data.CategoryEU = ODID_CATEGORY_EU_SPECIFIC;
    System_data.ClassEU = ODID_CLASS_EU_CLASS_3;
    System_data.OperatorAltitudeGeo = 20.5f;
    System_data.Timestamp = 28000000;
    Serial.println("✓ System data filled");
    
    // Encode System
    if (encodeSystemMessage(&System_enc, &System_data) == ODID_SUCCESS) {
        // Copy to UAS_data
        UAS_data.SystemValid = 1;
        memcpy(&UAS_data.System, &System_data, sizeof(System_data));
        Serial.println("✓ System encoded successfully");
    } else {
        Serial.println("✗ System encoding failed");
    }
    
    // === OperatorID ===
    odid_initOperatorIDData(&operatorID);
    operatorID.OperatorIdType = ODID_OPERATOR_ID;
    char operatorId[] = "98765432100123456789";
    strncpy(operatorID.OperatorId, operatorId, sizeof(operatorID.OperatorId));
    Serial.println("✓ OperatorID data filled");
    
    // Encode OperatorID
    if (encodeOperatorIDMessage(&OperatorID_enc, &operatorID) == ODID_SUCCESS) {
        // Copy to UAS_data
        UAS_data.OperatorIDValid = 1;
        memcpy(&UAS_data.OperatorID, &operatorID, sizeof(operatorID));
        Serial.println("✓ OperatorID encoded successfully");
    } else {
        Serial.println("✗ OperatorID encoding failed");
    }
}

// Print encoded data in hex format (like the test example)
void print_encoded_data() {
    Serial.println("\n===== Encoded Data (Hex) =====");
    Serial.println("         0- 1- 2- 3- 4- 5- 6- 7- 8- 9-10-11-12-13-14-15-16-17-18-19-20-21-22-23-24");
    
    Serial.print("BasicID:    ");
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
        Serial.printf("%02X ", ((uint8_t*)&BasicID_enc)[i]);
    }
    Serial.println();
    
    Serial.print("Location:   ");
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
        Serial.printf("%02X ", ((uint8_t*)&Location_enc)[i]);
    }
    Serial.println();
    
    Serial.print("SelfID:     ");
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
        Serial.printf("%02X ", ((uint8_t*)&SelfID_enc)[i]);
    }
    Serial.println();
    
    Serial.print("System:     ");
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
        Serial.printf("%02X ", ((uint8_t*)&System_enc)[i]);
    }
    Serial.println();
    
    Serial.print("OperatorID: ");
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
        Serial.printf("%02X ", ((uint8_t*)&OperatorID_enc)[i]);
    }
    Serial.println();
    Serial.println("===============================\n");
}

// Print current transmission info
void print_tx_info(const char* msg_type) {
    static uint32_t tx_count = 0;
    tx_count++;
    
    Serial.printf("[%lu] BT4 TX: %s (Count: %lu)\n", millis(), msg_type, tx_count);
}

// Arduino Setup
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("===============================");
    Serial.println("OpenDroneID BLE Demo with Encoding");
    Serial.println("===============================");
    
    // Initialize BLE
    if (!ble.init()) {
        Serial.println("ERROR: BLE initialization failed!");
        while(1) {
            delay(1000);
            Serial.println("BLE init failed - check hardware");
        }
    }
    Serial.println("✓ BLE initialized successfully");
    
    // Fill and encode all data
    fill_and_encode_data();
    
    // Print encoded data for debugging
    print_encoded_data();
    
    // Print loaded data summary
    Serial.println("Data Summary:");
    Serial.printf("- BasicID: %s (Type: %d)\n", UAS_data.BasicID[0].UASID, UAS_data.BasicID[0].UAType);
    Serial.printf("- Location: %.6f, %.6f @ %dm\n", 
                 UAS_data.Location.Latitude, UAS_data.Location.Longitude, UAS_data.Location.AltitudeBaro);
    Serial.printf("- SelfID: %s\n", UAS_data.SelfID.Desc);
    Serial.printf("- OperatorID: %s\n", UAS_data.OperatorID.OperatorId);
    
    Serial.println("\n🚁 Ready to transmit encoded OpenDroneID data!");
    Serial.println("Messages will cycle: BasicID → Location → SelfID → System → OperatorID");
    Serial.println("===============================\n");
}

// Arduino Loop
void loop() {
    static uint32_t last_bt4_tx = 0;
    static uint32_t last_bt5_tx = 0;
    
    // Transmission intervals
    const uint32_t BT4_INTERVAL = 200;  // 200ms between BT4 messages (state cycling)
    const uint32_t BT5_INTERVAL = 1000; // 1000ms between BT5 messages (all data)
    
    uint32_t now = millis();
    
    // BT4 Legacy transmission (cycling through message types)
    if (now - last_bt4_tx >= BT4_INTERVAL) {
        last_bt4_tx = now;
        
        if (ble.transmit_legacy(UAS_data)) {
            const char* msg_names[] = {"BasicID", "Location", "SelfID", "System", "OperatorID"};
            print_tx_info(msg_names[current_state]);
            
            // Move to next state for BT4 cycling
            current_state = (MSG_STATE)((current_state + 1) % MAX_STATES);
        } else {
            Serial.printf("BT4 TX Failed at state %d\n", current_state);
        }
    }
    
    // BT5 Long Range transmission (all data at once)
    if (now - last_bt5_tx >= BT5_INTERVAL) {
        last_bt5_tx = now;
        
        if (ble.transmit_longrange(UAS_data)) {
            Serial.printf("[%lu] BT5 LongRange: All encoded data transmitted\n", now);
        } else {
            Serial.println("BT5 LongRange TX Failed");
        }
    }
    
    delay(1);
}