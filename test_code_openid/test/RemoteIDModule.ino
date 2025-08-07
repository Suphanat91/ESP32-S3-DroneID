// // ------------------ ระบบเข้ารหัสสำหรับ User และ Dev พร้อม base64 password decode ------------------
// #include "options.h"
// #include <Arduino.h>
// #include "version.h"
// #include <math.h>
// #include <time.h>
// #include <sys/time.h>
// #include <opendroneid.h>
// #include "mavlink.h"
// #include "DroneCAN.h"
// #include "WiFi_TX.h"
// #include "BLE_TX.h"
// #include <esp_wifi.h>
// #include <WiFi.h>
// #include "parameters.h"
// #include "webinterface.h"
// #include "check_firmware.h"
// #include <esp_ota_ops.h>
// #include "efuse.h"
// #include "led.h"
// #include "soc/soc.h"
// #include "soc/rtc_cntl_reg.h"

// #include "mbedtls/aes.h"
// #include "mbedtls/sha256.h"
// #include "mbedtls/base64.h"
// #include <string.h>
// #include <esp_system.h>

// #if AP_DRONECAN_ENABLED
// static DroneCAN dronecan;
// #endif

// #if AP_MAVLINK_ENABLED
// static MAVLinkSerial mavlink1{Serial1, MAVLINK_COMM_0};
// static MAVLinkSerial mavlink2{Serial, MAVLINK_COMM_1};
// #endif

// static WiFi_TX wifi;
// ODID_UAS_Data UAS_data;
// BLE_TX ble;
// // #define DEBUG_BAUDRATE 57600

// String status_reason;
// static uint32_t last_location_ms;
// static WebInterface webif;

// static bool arm_check_ok = false;
// static bool pfst_check_ok = false;

// // ---------------- AES Key, Salt, IV ----------------
// unsigned char user_key[32];
// unsigned char dev_key[32];
// unsigned char user_salt[16], user_iv[16];
// unsigned char dev_salt[16], dev_iv[16];

// // ---------------- Generate AES Key from Password + Salt ----------------
// void generateAESKeyFromPasswordAndSalt(const char *password, const unsigned char *salt, size_t salt_len, unsigned char aes_key_out[32])
// {
//     mbedtls_sha256_context ctx;
//     mbedtls_sha256_init(&ctx);
//     mbedtls_sha256_starts_ret(&ctx, 0);
//     mbedtls_sha256_update_ret(&ctx, (const unsigned char *)password, strlen(password));
//     mbedtls_sha256_update_ret(&ctx, salt, salt_len);
//     mbedtls_sha256_finish_ret(&ctx, aes_key_out);
//     mbedtls_sha256_free(&ctx);
// }

// // ---------------- AES Encryption ----------------
// void aes256_encrypt(const unsigned char *input, unsigned char *output, size_t length,
//                     const unsigned char key[32], const unsigned char iv_in[16])
// {
//     mbedtls_aes_context aes;
//     mbedtls_aes_init(&aes);
//     mbedtls_aes_setkey_enc(&aes, key, 256);

//     unsigned char iv_copy[16];
//     memcpy(iv_copy, iv_in, 16);

//     mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, input, output);
//     mbedtls_aes_free(&aes);
// }

// // ---------------- Serialization ----------------
// void serializeUASData(ODID_UAS_Data &data, unsigned char *buffer, size_t *len)
// {
//     *len = sizeof(ODID_UAS_Data);
//     memcpy(buffer, &data, *len);
// }

// // ---------------- Setup ----------------
// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     odid_initUasData(&UAS_data);

//   // ---------------------------
//   // Basic ID
//   UAS_data.BasicIDValid[0] = 1;
//   UAS_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
//   UAS_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
//   snprintf(UAS_data.BasicID[0].UASID, sizeof(UAS_data.BasicID[0].UASID), "DUMMY1234567890");

//   // ---------------------------
//   // Location
//   UAS_data.LocationValid = 1;
//   UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
//   UAS_data.Location.Direction = 90.0f;
//   UAS_data.Location.SpeedHorizontal = 5.5f;
//   UAS_data.Location.SpeedVertical = 0.0f;
//   UAS_data.Location.Latitude = 13.0890;   // Bangkok
//   UAS_data.Location.Longitude = 100.9531;
//   UAS_data.Location.AltitudeBaro = 50.0f;
//   UAS_data.Location.AltitudeGeo = 48.0f;
//   UAS_data.Location.HeightType = ODID_HEIGHT_REF_OVER_GROUND;
//   UAS_data.Location.Height = 48.0f;
//   UAS_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
//   UAS_data.Location.VertAccuracy = ODID_VER_ACC_3_METER;
//   UAS_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
//   UAS_data.Location.TSAccuracy = ODID_TIME_ACC_0_5_SECOND;
//   UAS_data.Location.TimeStamp = 1234.0f;

//   // ---------------------------
//   // Self ID
//   UAS_data.SelfIDValid = 1;
//   UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
//   snprintf(UAS_data.SelfID.Desc, sizeof(UAS_data.SelfID.Desc), "Test flight");

//   // ---------------------------
//   // System
//   UAS_data.SystemValid = 1;
//   UAS_data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
//   UAS_data.System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
//   UAS_data.System.OperatorLatitude = 13.0890;
//   UAS_data.System.OperatorLongitude = 100.9531;  // 13.089051466442617, 100.95314807868982
//   UAS_data.System.AreaCount = 1;
//   UAS_data.System.AreaRadius = 100;
//   UAS_data.System.AreaCeiling = 120.0f;
//   UAS_data.System.AreaFloor = 0.0f;
//   UAS_data.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
//   UAS_data.System.ClassEU = ODID_CLASS_EU_CLASS_1;
//   UAS_data.System.OperatorAltitudeGeo = 48.0f;
//   UAS_data.System.Timestamp = 12345678;

//   // ---------------------------
//   // Operator ID
//   UAS_data.OperatorIDValid = 1;
//   UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
//   snprintf(UAS_data.OperatorID.OperatorId, sizeof(UAS_data.OperatorID.OperatorId), "OPERATOR123");

//   // แสดงข้อมูลบน Serial
// #ifndef ODID_DISABLE_PRINTF
//   printBasicID_data(&UAS_data.BasicID[0]);
//   printLocation_data(&UAS_data.Location);
//   printSelfID_data(&UAS_data.SelfID);
//   printSystem_data(&UAS_data.System);
//   printOperatorID_data(&UAS_data.OperatorID);
// #endif
// }

// void loop() {
//     ble.transmit_legacy(UAS_data);
//     ble.transmit_longrange(UAS_data);

//     delay(1000);
// }










// ระบบซ่อนข้อมูล encrypted ใน OpenDroneID format ที่ valid
// วิธีนี้จะผ่าน validation แต่มีข้อมูลลับซ่อนอยู่

#include "options.h"
#include <Arduino.h>
#include "version.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <opendroneid.h>
#include "mavlink.h"
#include "DroneCAN.h"
#include "WiFi_TX.h"
#include "BLE_TX.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include "parameters.h"
#include "webinterface.h"
#include "check_firmware.h"
#include <esp_ota_ops.h>
#include "efuse.h"
#include "led.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include <string.h>
#include <esp_system.h>

#if AP_DRONECAN_ENABLED
static DroneCAN dronecan;
#endif

#if AP_MAVLINK_ENABLED
static MAVLinkSerial mavlink1{Serial1, MAVLINK_COMM_0};
static MAVLinkSerial mavlink2{Serial, MAVLINK_COMM_1};
#endif

static WiFi_TX wifi;
ODID_UAS_Data UAS_data;
ODID_UAS_Data fake_UAS_data;  // ข้อมูลปลอมสำหรับซ่อนข้อมูลจริง
BLE_TX ble;

String status_reason;
static uint32_t last_location_ms;
static WebInterface webif;

static bool arm_check_ok = false;
static bool pfst_check_ok = false;

// ---------------- Encryption Configuration ----------------
#define ENABLE_STEGANOGRAPHY true  // เปิดใช้การซ่อนข้อมูล

// ---------------- AES Key, Salt, IV ----------------
unsigned char user_key[32];
unsigned char user_salt[16], user_iv[16];

// ---------------- Generate AES Key from Password + Salt ----------------
void generateAESKeyFromPasswordAndSalt(const char *password, const unsigned char *salt, size_t salt_len, unsigned char aes_key_out[32])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char *)password, strlen(password));
    mbedtls_sha256_update_ret(&ctx, salt, salt_len);
    mbedtls_sha256_finish_ret(&ctx, aes_key_out);
    mbedtls_sha256_free(&ctx);
}

// ---------------- AES Encryption ----------------
void aes256_encrypt(const unsigned char *input, unsigned char *output, size_t length,
                    const unsigned char key[32], const unsigned char iv_in[16])
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);

    unsigned char iv_copy[16];
    memcpy(iv_copy, iv_in, 16);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
}

// ---------------- AES Decryption ----------------
void aes256_decrypt(const unsigned char *input, unsigned char *output, size_t length,
                    const unsigned char key[32], const unsigned char iv_in[16])
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 256);

    unsigned char iv_copy[16];
    memcpy(iv_copy, iv_in, 16);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
}

// ---------------- Steganography Functions ----------------

// ซ่อนข้อมูล encrypted ใน precision fields และ unused bytes
void hideEncryptedDataInOpenDroneID(ODID_UAS_Data &real_data, ODID_UAS_Data &fake_data, 
                                   unsigned char *encrypted_data, size_t encrypted_len) {
    
    // เริ่มต้นด้วยข้อมูลปลอมที่ valid
    memcpy(&fake_data, &real_data, sizeof(ODID_UAS_Data));
    
    // เปลี่ยนข้อมูลให้ดูปกติ แต่ซ่อน encrypted data
    snprintf(fake_data.BasicID[0].UASID, sizeof(fake_data.BasicID[0].UASID), "FAKE123456789");
    snprintf(fake_data.SelfID.Desc, sizeof(fake_data.SelfID.Desc), "Normal flight");
    snprintf(fake_data.OperatorID.OperatorId, sizeof(fake_data.OperatorID.OperatorId), "NORMAL123");
    
    // ซ่อนข้อมูล encrypted ในตำแหน่งต่างๆ
    size_t hidden_index = 0;
    
    // 1. ซ่อนใน decimal precision ของ coordinates
    if (hidden_index < encrypted_len) {
        // แปลง latitude ให้มี precision พิเศษ
        double lat_base = 13.0890;
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.Latitude = lat_base + (hidden_bits * 0.000001);
    }
    
    if (hidden_index < encrypted_len) {
        // แปลง longitude ให้มี precision พิเศษ  
        double lon_base = 100.9531;
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.Longitude = lon_base + (hidden_bits * 0.000001);
    }
    
    // 2. ซ่อนใน timestamp microseconds
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.TimeStamp = 1234.0f + (hidden_bits * 0.001f);
    }
    
    // 3. ซ่อนใน altitude precision
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.AltitudeBaro = 50.0f + (hidden_bits * 0.01f);
    }
    
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.AltitudeGeo = 48.0f + (hidden_bits * 0.01f);
    }
    
    // 4. ซ่อนใน speed precision
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.SpeedHorizontal = 5.5f + (hidden_bits * 0.01f);
    }
    
    // 5. ซ่อนใน operator coordinates
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.System.OperatorLatitude = 13.0890 + (hidden_bits * 0.000001);
    }
    
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.System.OperatorLongitude = 100.9531 + (hidden_bits * 0.000001);
    }
    
    // 6. ซ่อนใน direction precision
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.Location.Direction = 90.0f + (hidden_bits * 0.1f);
    }
    
    // 7. ซ่อนใน area radius และ ceiling
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.System.AreaRadius = 100 + hidden_bits;
    }
    
    if (hidden_index < encrypted_len) {
        uint32_t hidden_bits = encrypted_data[hidden_index++];
        fake_data.System.AreaCeiling = 120.0f + (hidden_bits * 0.1f);
    }
    
    Serial.printf("[+] Hidden %d bytes of encrypted data in OpenDroneID fields\n", hidden_index);
}

// ดึงข้อมูล encrypted ออกจาก OpenDroneID
void extractEncryptedDataFromOpenDroneID(ODID_UAS_Data &received_data, unsigned char *extracted_data, size_t *extracted_len) {
    size_t extract_index = 0;
    
    // 1. ดึงจาก latitude precision
    double lat_base = 13.0890;
    double lat_diff = received_data.Location.Latitude - lat_base;
    uint32_t hidden_bits = (uint32_t)(lat_diff / 0.000001);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 2. ดึงจาก longitude precision
    double lon_base = 100.9531;
    double lon_diff = received_data.Location.Longitude - lon_base;
    hidden_bits = (uint32_t)(lon_diff / 0.000001);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 3. ดึงจาก timestamp
    double time_diff = received_data.Location.TimeStamp - 1234.0f;
    hidden_bits = (uint32_t)(time_diff / 0.001f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 4. ดึงจาก altitude
    double alt_baro_diff = received_data.Location.AltitudeBaro - 50.0f;
    hidden_bits = (uint32_t)(alt_baro_diff / 0.01f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    double alt_geo_diff = received_data.Location.AltitudeGeo - 48.0f;
    hidden_bits = (uint32_t)(alt_geo_diff / 0.01f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 5. ดึงจาก speed
    double speed_diff = received_data.Location.SpeedHorizontal - 5.5f;
    hidden_bits = (uint32_t)(speed_diff / 0.01f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 6. ดึงจาก operator coordinates
    double op_lat_diff = received_data.System.OperatorLatitude - 13.0890;
    hidden_bits = (uint32_t)(op_lat_diff / 0.000001);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    double op_lon_diff = received_data.System.OperatorLongitude - 100.9531;
    hidden_bits = (uint32_t)(op_lon_diff / 0.000001);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 7. ดึงจาก direction
    double dir_diff = received_data.Location.Direction - 90.0f;
    hidden_bits = (uint32_t)(dir_diff / 0.1f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    // 8. ดึงจาก area
    hidden_bits = received_data.System.AreaRadius - 100;
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    double ceiling_diff = received_data.System.AreaCeiling - 120.0f;
    hidden_bits = (uint32_t)(ceiling_diff / 0.1f);
    extracted_data[extract_index++] = hidden_bits & 0xFF;
    
    *extracted_len = extract_index;
    
    Serial.printf("[+] Extracted %d bytes from OpenDroneID fields\n", extract_index);
}

// ---------------- Initialize Encryption ----------------
void initEncryption() {
    if (!ENABLE_STEGANOGRAPHY) return;
    
    // สร้าง fixed salt และ iv สำหรับ demo (ในการใช้งานจริงควร random)
    const unsigned char fixed_salt[16] = {
        0x21, 0x34, 0xA9, 0x55, 0x76, 0x4B, 0x2E, 0x8F,
        0x90, 0xC3, 0x1D, 0x7E, 0xFA, 0x62, 0xB5, 0x88
    };
    const unsigned char fixed_iv[16] = {
        0xB1, 0xF4, 0x7A, 0x29, 0x3C, 0xE5, 0x92, 0x08,
        0xD6, 0x41, 0x58, 0xFF, 0x2A, 0x91, 0xC7, 0x34
    };
    
    memcpy(user_salt, fixed_salt, 16);
    memcpy(user_iv, fixed_iv, 16);

    // รหัสผ่าน
    const char *user_password = "MySecretPassword123!";

    // สร้าง AES key
    generateAESKeyFromPasswordAndSalt(user_password, user_salt, sizeof(user_salt), user_key);
    Serial.println("[+] Steganographic encryption initialized");
}

// ---------------- Transmit with Steganography ----------------
void transmitData() {
    if (!ENABLE_STEGANOGRAPHY) {
        // ส่งแบบเดิม
        ble.transmit_legacy(UAS_data);
        ble.transmit_longrange(UAS_data);
        Serial.println("[+] Transmitted normal OpenDroneID data");
        return;
    }

    // เข้ารหัสข้อมูลจริงแบบย่อ (เฉพาะข้อมูลสำคัญ)
    String secret_message = String(UAS_data.BasicID[0].UASID) + "|" + 
                           String(UAS_data.SelfID.Desc) + "|" + 
                           String(UAS_data.OperatorID.OperatorId);
    
    // Pad ข้อความให้เป็นทวีคูณของ 16
    size_t msg_len = secret_message.length();
    size_t padded_len = (msg_len + 15) & ~15;
    unsigned char padded_msg[64] = {0};
    memcpy(padded_msg, secret_message.c_str(), msg_len);
    
    // เข้ารหัส
    unsigned char encrypted_msg[64];
    aes256_encrypt(padded_msg, encrypted_msg, padded_len, user_key, user_iv);
    
    // ซ่อนข้อมูล encrypted ใน OpenDroneID format
    hideEncryptedDataInOpenDroneID(UAS_data, fake_UAS_data, encrypted_msg, min(padded_len, (size_t)11));
    
    // ส่งข้อมูลปลอมที่มีข้อมูลจริงซ่อนอยู่
    ble.transmit_legacy(fake_UAS_data);
    ble.transmit_longrange(fake_UAS_data);
    
    Serial.println("\n[+] Transmitted steganographic OpenDroneID data");
    Serial.printf("Fake UASID: %s\n", fake_UAS_data.BasicID[0].UASID);
    Serial.printf("Hidden Message: %s\n", secret_message.c_str());
    
    // Demo การถอดข้อมูล
    demoExtraction();
}

// ---------------- Demo Extraction ----------------
void demoExtraction() {
    unsigned char extracted_data[64];
    size_t extracted_len;
    
    // ดึงข้อมูลที่ซ่อนไว้
    extractEncryptedDataFromOpenDroneID(fake_UAS_data, extracted_data, &extracted_len);
    
    // ถอดรหัส
    unsigned char decrypted_msg[64] = {0};
    size_t decrypt_len = (extracted_len + 15) & ~15;
    if (decrypt_len > 0) {
        aes256_decrypt(extracted_data, decrypted_msg, decrypt_len, user_key, user_iv);
        
        Serial.printf("[+] Extracted and decrypted: %s\n", (char*)decrypted_msg);
    }
}

// ---------------- Setup ----------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("OpenDroneID Steganographic Transmitter Starting...");
    Serial.printf("Steganography: %s\n", ENABLE_STEGANOGRAPHY ? "ENABLED" : "DISABLED");

    odid_initUasData(&UAS_data);

    // ข้อมูลจริงที่จะซ่อน
    UAS_data.BasicIDValid[0] = 1;
    UAS_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    UAS_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    snprintf(UAS_data.BasicID[0].UASID, sizeof(UAS_data.BasicID[0].UASID), "SECRET123456");

    UAS_data.LocationValid = 1;
    UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
    UAS_data.Location.Direction = 90.0f;
    UAS_data.Location.SpeedHorizontal = 5.5f;
    UAS_data.Location.SpeedVertical = 0.0f;
    UAS_data.Location.Latitude = 13.0890;
    UAS_data.Location.Longitude = 100.9531;
    UAS_data.Location.AltitudeBaro = 50.0f;
    UAS_data.Location.AltitudeGeo = 48.0f;
    UAS_data.Location.HeightType = ODID_HEIGHT_REF_OVER_GROUND;
    UAS_data.Location.Height = 48.0f;
    UAS_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
    UAS_data.Location.VertAccuracy = ODID_VER_ACC_3_METER;
    UAS_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    UAS_data.Location.TSAccuracy = ODID_TIME_ACC_0_5_SECOND;
    UAS_data.Location.TimeStamp = 1234.0f;

    UAS_data.SelfIDValid = 1;
    UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    snprintf(UAS_data.SelfID.Desc, sizeof(UAS_data.SelfID.Desc), "Secret mission");

    UAS_data.SystemValid = 1;
    UAS_data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    UAS_data.System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
    UAS_data.System.OperatorLatitude = 13.0890;
    UAS_data.System.OperatorLongitude = 100.9531;
    UAS_data.System.AreaCount = 1;
    UAS_data.System.AreaRadius = 100;
    UAS_data.System.AreaCeiling = 120.0f;
    UAS_data.System.AreaFloor = 0.0f;
    UAS_data.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
    UAS_data.System.ClassEU = ODID_CLASS_EU_CLASS_1;
    UAS_data.System.OperatorAltitudeGeo = 48.0f;
    UAS_data.System.Timestamp = 12345678;

    UAS_data.OperatorIDValid = 1;
    UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    snprintf(UAS_data.OperatorID.OperatorId, sizeof(UAS_data.OperatorID.OperatorId), "SECRET_OP");

    // เริ่มต้นระบบเข้ารหัส
    initEncryption();
    
    Serial.println("Setup completed!");
}

// ---------------- Loop ----------------
void loop() {
    // อัปเดต timestamp
    UAS_data.Location.TimeStamp = millis() / 1000.0f;
    
    // ส่งข้อมูล
    transmitData();
    
    delay(2000);
}