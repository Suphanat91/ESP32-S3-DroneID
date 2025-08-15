/*
  BLE TX driver

  Many thanks to Roel Schiphorst from BlueMark for his assistance with the Bluetooth code

  Also many thanks to chegewara for his sample code:
  https://github.com/Tinyu-Zhao/Arduino-Borads/blob/b584d00a81e4ac6d7b72697c674ca1bbcb8aae69/libraries/BLE/examples/BLE5_multi_advertising/BLE5_multi_advertising.ino
 */

#include "BLE_TX.h"
#include "options.h"
#include <esp_system.h>

#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "parameters.h"
#include <Arduino.h>
#include <string.h>
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/aes.h"
// ===== AES-256 CTR helpers/config =====

// สร้างคีย์ AES จากรหัสผ่านที่ผู้ใช้กรอก
static void derive_aes_key_from_password(const char* password, uint8_t key_out[32]) {
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts_ret(&sha_ctx, 0); // 0 = SHA-256, 1 = SHA-224
    mbedtls_sha256_update_ret(&sha_ctx, (const unsigned char*)password, strlen(password));
    mbedtls_sha256_finish_ret(&sha_ctx, key_out);
    mbedtls_sha256_free(&sha_ctx);
}

// Example IV / nonce (16 bytes). In real usage, generate a fresh one per message.
static const uint8_t AES256_IV[16] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};

// Function to encrypt/decrypt a buffer in-place with AES-256 CTR
static void aes256_ctr_crypt(uint8_t* buf, size_t len,
                             const uint8_t key[32], const uint8_t iv[16]) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, key, 256); // set AES-256 key for encryption
  size_t nc_off = 0;
  unsigned char stream_block[16] = {0};
  unsigned char nonce_counter[16];
  memcpy(nonce_counter, iv, 16); // copy IV so we don't modify the original

  // Encrypt/decrypt (same function in CTR mode)
  mbedtls_aes_crypt_ctr(&ctx, len, &nc_off, nonce_counter, stream_block, buf, buf);

  mbedtls_aes_free(&ctx);
}

// Helper to print a buffer as hex
static void print_hex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  Serial.print(" (");
  Serial.print((unsigned)len);
  Serial.println(" bytes):");
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0'); // pad single-digit hex
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// interval min/max are configured for 1 Hz update rate. Somehow dynamic setting of these fields fails
// shorter intervals lead to more BLE transmissions. This would result in increased power consumption and can lead to more interference to other radio systems.
static esp_ble_gap_ext_adv_params_t legacy_adv_params = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_NONCONN,
    .interval_min = 192,
    .interval_max = 267,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST, // we want unicast non-connectable transmission
    .tx_power = 0,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_1M,
    .sid = 0,
    .scan_req_notif = false,
};

static esp_ble_gap_ext_adv_params_t ext_adv_params_coded = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED, // set to unicast advertising
    .interval_min = 1200,
    .interval_max = 1600,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST, // we want unicast non-connectable transmission
    .tx_power = 0,
    .primary_phy = ESP_BLE_GAP_PHY_CODED,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_CODED,
    .sid = 1,
    .scan_req_notif = false,
};

/*
  map dBm to a TX power
 */
uint8_t BLE_TX::dBm_to_tx_power(float dBm) const
{
    static const struct
    {
        uint8_t level;
        float dBm;
    } dBm_table[] = {
        {ESP_PWR_LVL_N27, -27},
        {ESP_PWR_LVL_N24, -24},
        {ESP_PWR_LVL_N21, -21},
        {ESP_PWR_LVL_N18, -18},
        {ESP_PWR_LVL_N15, -15},
        {ESP_PWR_LVL_N12, -12},
        {ESP_PWR_LVL_N9, -9},
        {ESP_PWR_LVL_N6, -6},
        {ESP_PWR_LVL_N3, -3},
        {ESP_PWR_LVL_N0, 0},
        {ESP_PWR_LVL_P3, 3},
        {ESP_PWR_LVL_P6, 6},
        {ESP_PWR_LVL_P9, 9},
        {ESP_PWR_LVL_P12, 12},
        {ESP_PWR_LVL_P15, 15},
        {ESP_PWR_LVL_P18, 18},
    };
    for (const auto &t : dBm_table)
    {
        if (dBm <= t.dBm)
        {
            return t.level;
        }
    }
    return ESP_PWR_LVL_P18;
}

static BLEMultiAdvertising advert(2);

bool BLE_TX::init(void)
{
    if (initialised)
    {
        return true;
    }
    initialised = true;
    BLEDevice::init("");

    // setup power levels
    legacy_adv_params.tx_power = dBm_to_tx_power(g.bt4_power);
    ext_adv_params_coded.tx_power = dBm_to_tx_power(g.bt5_power);

    // set min/max interval based on output rate
    legacy_adv_params.interval_max = (1000 / (g.bt4_rate * 7)) / 0.625;
    legacy_adv_params.interval_min = 0.75 * legacy_adv_params.interval_max;
    ext_adv_params_coded.interval_max = (1000 / (g.bt5_rate)) / 0.625;
    ext_adv_params_coded.interval_min = 0.75 * ext_adv_params_coded.interval_max;

    // generate random mac address
    uint8_t mac_addr[6];
    generate_random_mac(mac_addr);

    // set as a bluetooth random static address
    mac_addr[0] |= 0xc0;

    advert.setAdvertisingParams(0, &legacy_adv_params);
    advert.setInstanceAddress(0, mac_addr);
    advert.setDuration(0);

    advert.setAdvertisingParams(1, &ext_adv_params_coded);
    advert.setDuration(1);
    advert.setInstanceAddress(1, mac_addr);

    // prefer S8 coding
    if (esp_ble_gap_set_prefered_default_phy(ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING, ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING) != ESP_OK)
    {
        Serial.printf("Failed to setup S8 coding\n");
    }

    memset(&msg_counters, 0, sizeof(msg_counters));
    return true;
}

#define IMIN(a, b) ((a) < (b) ? (a) : (b))

bool BLE_TX::transmit_longrange(ODID_UAS_Data &UAS_data)
{
    init();
    uint8_t payload[250];
    int length = odid_message_build_pack(&UAS_data, payload, 255);
    if (length <= 0) return false;

    // ===== 1) ผู้ใช้กรอกรหัสผ่าน =====
    char user_password[64] = "helloworld"; 
    uint8_t AES256_KEY[32];
    derive_aes_key_from_password(user_password, AES256_KEY);

    // ===== 2) IV =====
    uint8_t AES256_IV[16] = { 
        0x00,0x01,0x02,0x03,
        0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,
        0x0c,0x0d,0x0e,0x0f
    };

    // Print payload
    print_hex("Payload before b64", payload, length);

    // ===== Base64 Encode =====
    size_t b64_len = 0;
    mbedtls_base64_encode(NULL, 0, &b64_len, payload, length);

    uint8_t base64_out[256];
    if (b64_len > sizeof(base64_out)) return false;

    if (mbedtls_base64_encode(base64_out, sizeof(base64_out), &b64_len, payload, length) != 0) return false;
    base64_out[b64_len] = '\0';
    Serial.print("payload after encode: ");
    Serial.println((char*)base64_out);

    // ===== AES Encrypt =====
    uint8_t b64_cipher[400];
    memcpy(b64_cipher, base64_out, b64_len);
    aes256_ctr_crypt(b64_cipher, b64_len, AES256_KEY, AES256_IV);
    print_hex("AES-CTR Cipher (of Base64)", b64_cipher, b64_len);

    // ===== AES Decrypt =====
    uint8_t b64_decrypted[400];
    memcpy(b64_decrypted, b64_cipher, b64_len);
    aes256_ctr_crypt(b64_decrypted, b64_len, AES256_KEY, AES256_IV);
    b64_decrypted[b64_len] = '\0';
    Serial.print("Base64 AFTER AES decrypt: ");
    Serial.println((char*)b64_decrypted);

    // ===== Base64 Decode =====
    uint8_t decoded[250];
    size_t decoded_len = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len, b64_decrypted, b64_len) != 0) {
        Serial.println("Base64 decoding failed!");
        return false;
    }

    // ===== Compare =====
    print_hex("Payload AFTER full roundtrip", decoded, decoded_len);
    bool match = (decoded_len == (size_t)length) && (memcmp(decoded, payload, length) == 0);
    Serial.print("Compare original vs roundtrip: ");
    Serial.println(match ? "MATCH ✅" : "MISMATCH ❌");

    // ===== ส่ง BLE =====
    const uint8_t header[]{uint8_t(length + 5), 0x16, 0xfa, 0xff, 0x0d, uint8_t(msg_counters[ODID_MSG_COUNTER_PACKED]++)};
    memcpy(longrange_payload, header, sizeof(header));
    memcpy(&longrange_payload[sizeof(header)], payload, length);
    int longrange_length = sizeof(header) + length;

    advert.setAdvertisingData(1, longrange_length, longrange_payload);
    if (!started) advert.start();
    started = true;

    return true;
}

bool BLE_TX::transmit_legacy(ODID_UAS_Data &UAS_data)
{
    init();
    static uint8_t legacy_phase = 0;
    int legacy_length = 0;
    // setup ASTM header
    const uint8_t header[]{0x1e, 0x16, 0xfa, 0xff, 0x0d}; // exclude the message counter
    // combine header with payload
    memset(legacy_payload, 0, sizeof(legacy_payload));
    memcpy(legacy_payload, header, sizeof(header));
    legacy_length = sizeof(header);

    switch (legacy_phase)
    {
    case 0:
    {
        if (UAS_data.LocationValid)
        {
            ODID_Location_encoded location_encoded;
            memset(&location_encoded, 0, sizeof(location_encoded));
            if (encodeLocationMessage(&location_encoded, &UAS_data.Location) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_LOCATION], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_LOCATION]++;
            // msg_counters[ODID_MSG_COUNTER_LOCATION] %= 256; //likely not be needed as it is defined as unint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &location_encoded, sizeof(location_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(location_encoded);
        }
        break;
    }

    case 1:
    {
        if (UAS_data.BasicIDValid[0])
        {
            ODID_BasicID_encoded basicid_encoded;
            memset(&basicid_encoded, 0, sizeof(basicid_encoded));
            if (encodeBasicIDMessage(&basicid_encoded, &UAS_data.BasicID[0]) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_BASIC_ID], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_BASIC_ID]++;
            // msg_counters[ODID_MSG_COUNTER_BASIC_ID] %= 256; //likely not be needed as it is defined as unint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &basicid_encoded, sizeof(basicid_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(basicid_encoded);
        }
        break;
    }

    case 2:
    {
        if (UAS_data.SelfIDValid)
        {
            ODID_SelfID_encoded selfid_encoded;
            memset(&selfid_encoded, 0, sizeof(selfid_encoded));
            if (encodeSelfIDMessage(&selfid_encoded, &UAS_data.SelfID) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_SELF_ID], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_SELF_ID]++;
            // msg_counters[ODID_MSG_COUNTER_SELF_ID] %= 256; //likely not be needed as it is defined as uint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &selfid_encoded, sizeof(selfid_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(selfid_encoded);
        }
        break;
    }

    case 3:
    {
        if (UAS_data.SystemValid)
        {
            ODID_System_encoded system_encoded;
            memset(&system_encoded, 0, sizeof(system_encoded));
            if (encodeSystemMessage(&system_encoded, &UAS_data.System) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_SYSTEM], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_SYSTEM]++;
            // msg_counters[ODID_MSG_COUNTER_SYSTEM] %= 256; //likely not be needed as it is defined as uint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &system_encoded, sizeof(system_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(system_encoded);
        }
        break;
    }

    case 4:
    {
        if (UAS_data.OperatorIDValid)
        {
            ODID_OperatorID_encoded operatorid_encoded;
            memset(&operatorid_encoded, 0, sizeof(operatorid_encoded));
            if (encodeOperatorIDMessage(&operatorid_encoded, &UAS_data.OperatorID) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_OPERATOR_ID], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_OPERATOR_ID]++;
            // msg_counters[ODID_MSG_COUNTER_OPERATOR_ID] %= 256; //likely not be needed as it is defined as uint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &operatorid_encoded, sizeof(operatorid_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(operatorid_encoded);
        }
        break;
    }

    case 5: // in case of dual basic ID
        if (UAS_data.BasicIDValid[1])
        {
            ODID_BasicID_encoded basicid2_encoded;
            memset(&basicid2_encoded, 0, sizeof(basicid2_encoded));
            if (encodeBasicIDMessage(&basicid2_encoded, &UAS_data.BasicID[1]) != ODID_SUCCESS)
            {
                break;
            }

            memcpy(&legacy_payload[sizeof(header)], &msg_counters[ODID_MSG_COUNTER_BASIC_ID], 1); // set packet counter
            msg_counters[ODID_MSG_COUNTER_BASIC_ID]++;
            // msg_counters[ODID_MSG_COUNTER_BASIC_ID] %= 256; //likely not be needed as it is defined as unint_8

            memcpy(&legacy_payload[sizeof(header) + 1], &basicid2_encoded, sizeof(basicid2_encoded));
            legacy_length = sizeof(header) + 1 + sizeof(basicid2_encoded);
        }
        break;

    case 6:
    {
        // set BLE name
        char legacy_name[28]{};
        const char *UAS_ID = (const char *)UAS_data.BasicID[0].UASID;
        const uint8_t ID_len = strlen(UAS_ID);
        const uint8_t ID_tail = IMIN(4, ID_len);
        snprintf(legacy_name, sizeof(legacy_name), "ArduRemoteID_%s", &UAS_ID[ID_len - ID_tail]);

        memset(legacy_payload, 0, sizeof(legacy_payload));
        const uint8_t legacy_name_header[]{0x02, 0x01, 0x06, uint8_t(strlen(legacy_name) + 1), ESP_BLE_AD_TYPE_NAME_SHORT};

        memcpy(legacy_payload, legacy_name_header, sizeof(legacy_name_header));
        memcpy(&legacy_payload[sizeof(legacy_name_header)], legacy_name, strlen(legacy_name) + 1);

        legacy_length = sizeof(legacy_name_header) + strlen(legacy_name) + 1; // add extra char for \0
        break;
    }
    }

    legacy_phase++;

    if (UAS_data.BasicIDValid[1])
    {
        legacy_phase %= 7;
    }
    else
    {
        legacy_phase %= 6;
    }

    advert.setAdvertisingData(0, legacy_length, legacy_payload);

    if (!started)
    {
        advert.start();
    }
    started = true;

    return true;
}