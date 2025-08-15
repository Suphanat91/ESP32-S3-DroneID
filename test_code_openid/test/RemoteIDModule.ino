/*
  implement OpenDroneID MAVLink and DroneCAN support
 */
/*
  released under GNU GPL v2 or later
 */

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
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#if AP_DRONECAN_ENABLED
static DroneCAN dronecan;
#endif

#if AP_MAVLINK_ENABLED
static MAVLinkSerial mavlink1{Serial1, MAVLINK_COMM_0};
static MAVLinkSerial mavlink2{Serial, MAVLINK_COMM_1};
#endif

static WiFi_TX wifi;
static BLE_TX ble;

// #define DEBUG_BAUDRATE 57600

// OpenDroneID output data structure
ODID_UAS_Data UAS_data;
String status_reason;
static uint32_t last_location_ms;
static WebInterface webif;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
// คืนค่า "วินาทีตั้งแต่ต้นชั่วโมง" (0..3599.xxx) สำหรับ ODID Location.TimeStamp
static inline float odid_seconds_in_current_hour() {
  time_t now = time(NULL);
  if (now > 0) {
    struct tm *utc = gmtime(&now); // ODID ใช้เวลามาตรฐาน
    return (float)(utc->tm_min * 60 + utc->tm_sec);
  }
  // ถ้ายังไม่มีเวลา (ยังไม่ sync NTP) ให้ใช้ millis() วนภายใน 1 ชั่วโมง
  return ((millis() / 1000.0f));
}

// คืนค่า Unix time ถ้ามี; ถ้าไม่มีให้สร้าง timebase จาก millis()
static inline uint32_t safe_unix_time() {
  time_t now = time(NULL);
  if (now > 100000) { // ถ้าได้เวลาจริงแล้ว (หลังปี 1970 พอสมควร)
    return (uint32_t)now;
  }
  // timebase จำลอง: เริ่มที่ 0 แล้ววิ่งด้วย millis()/1000
  return (uint32_t)(millis() / 1000);
}

// ทำให้ค่ามุมอยู่ในช่วง 0..360
static inline float wrap360(float deg) {
  while (deg >= 360.0f) deg -= 360.0f;
  while (deg < 0.0f) deg += 360.0f;
  return deg;
}
void setup()
{
  Serial.begin(115200);
  delay(1000);

  odid_initUasData(&UAS_data);

  // ---------------------------
  // Basic ID
  UAS_data.BasicIDValid[0] = 1;
  UAS_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  UAS_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
  snprintf(UAS_data.BasicID[0].UASID, sizeof(UAS_data.BasicID[0].UASID), "DUMMY1234567890");

  // ---------------------------
  // Basic ID #2 (ทำให้ phase วนถึง 6)
  UAS_data.BasicIDValid[1] = 1; // สำคัญ!
  UAS_data.BasicID[1].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  UAS_data.BasicID[1].IDType = ODID_IDTYPE_SERIAL_NUMBER; // หรือชนิดที่คุณต้องการ
  snprintf(UAS_data.BasicID[1].UASID, sizeof(UAS_data.BasicID[1].UASID), "DUMMY2_ABCDEFG");

  // ---------------------------
  // Location
  UAS_data.Location.BaroAccuracy = ODID_VER_ACC_3_METER; // เพิ่มให้ครบตามต้นฉบับ
  UAS_data.LocationValid = 1;
  UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
  UAS_data.Location.Direction = 90.0f;
  UAS_data.Location.SpeedHorizontal = 5.5f;
  UAS_data.Location.SpeedVertical = 0.0f;
  UAS_data.Location.Latitude = 13.0890; // Bangkok
  UAS_data.Location.Longitude = 100.9531;
  UAS_data.Location.AltitudeBaro = 50.0f;
  UAS_data.Location.AltitudeGeo = 48.0f;
  UAS_data.Location.HeightType = ODID_HEIGHT_REF_OVER_GROUND;
  UAS_data.Location.Height = 48.0f;
  UAS_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
  UAS_data.Location.VertAccuracy = ODID_VER_ACC_3_METER;
  UAS_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
  UAS_data.Location.TSAccuracy = ODID_TIME_ACC_0_5_SECOND;
  UAS_data.Location.TimeStamp = odid_seconds_in_current_hour();

  // ---------------------------
  // Self ID
  UAS_data.SelfIDValid = 1;
  UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
  snprintf(UAS_data.SelfID.Desc, sizeof(UAS_data.SelfID.Desc), "Test flight");

  // ---------------------------
  // System
  UAS_data.SystemValid = 1;
  UAS_data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
  UAS_data.System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
  UAS_data.System.OperatorLatitude = 13.0890;
  UAS_data.System.OperatorLongitude = 100.9531; // 13.089051466442617, 100.95314807868982
  UAS_data.System.AreaCount = 1;
  UAS_data.System.AreaRadius = 100;
  UAS_data.System.AreaCeiling = 120.0f;
  UAS_data.System.AreaFloor = 0.0f;
  UAS_data.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
  UAS_data.System.ClassEU = ODID_CLASS_EU_CLASS_1;
  UAS_data.System.OperatorAltitudeGeo = 48.0f;
  UAS_data.System.Timestamp = safe_unix_time();

  // ---------------------------
  // Operator ID
  UAS_data.OperatorIDValid = 1;
  UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
  snprintf(UAS_data.OperatorID.OperatorId, sizeof(UAS_data.OperatorID.OperatorId), "OPERATOR123");

  // แสดงข้อมูลบน Serial
#ifndef ODID_DISABLE_PRINTF
  printBasicID_data(&UAS_data.BasicID[0]);
  printLocation_data(&UAS_data.Location);
  printSelfID_data(&UAS_data.SelfID);
  printSystem_data(&UAS_data.System);
  printOperatorID_data(&UAS_data.OperatorID);
#endif
}
void loop()
{
  // อัปเดตค่าทุก ~1 วินาที ให้เฟรมดู "สด" เสมอ
  static uint32_t last_ms = 0;
  uint32_t now_ms = millis();
  if (now_ms - last_ms >= 1000) {
    last_ms = now_ms;

    // 1) เวลา: สำคัญที่สุดสำหรับการรีแพ็กเก็ต
    UAS_data.Location.TimeStamp = odid_seconds_in_current_hour();
    UAS_data.System.Timestamp   = safe_unix_time();

    // 2) อัปเดตค่า dynamic เล็กน้อย (ตัวอย่าง)
    // หมุนหัว 1°/s
    UAS_data.Location.Direction = wrap360(UAS_data.Location.Direction + 1.0f);

    // แกว่งความสูง +/-0.5 m แบบช้า ๆ
    static bool up = true;
    if (up) {
      UAS_data.Location.AltitudeGeo += 0.05f;
      UAS_data.Location.Height      += 0.05f;
      if (UAS_data.Location.AltitudeGeo > 48.5f) up = false;
    } else {
      UAS_data.Location.AltitudeGeo -= 0.05f;
      UAS_data.Location.Height      -= 0.05f;
      if (UAS_data.Location.AltitudeGeo < 47.5f) up = true;
    }

    // (ถ้าต้องการ) ขยับพิกัดเล็กน้อยให้ดูเหมือนเคลื่อนที่ตรงไปทางตะวันออก
    // 5.5 m/s ≈ 0.0000495 deg lon/s ที่ละติจูด ~13°
    const float meters_per_deg_lon = 111320.0f * cosf(radians(13.0890f));
    const float dlon = (UAS_data.Location.SpeedHorizontal / meters_per_deg_lon);
    UAS_data.Location.Longitude += dlon;
  }

  static uint32_t last_update_wifi_nan_ms;
    if (g.wifi_nan_rate > 0 &&
        now_ms - last_update_wifi_nan_ms > 1000/g.wifi_nan_rate) {
        last_update_wifi_nan_ms = now_ms;
        wifi.transmit_nan(UAS_data);
    }

    static uint32_t last_update_wifi_beacon_ms;
    if (g.wifi_beacon_rate > 0 &&
        now_ms - last_update_wifi_beacon_ms > 1000/g.wifi_beacon_rate) {
        last_update_wifi_beacon_ms = now_ms;
        wifi.transmit_beacon(UAS_data);
    }

    static uint32_t last_update_bt5_ms;
    if (g.bt5_rate > 0 &&
        now_ms - last_update_bt5_ms > 1000/g.bt5_rate) {
        last_update_bt5_ms = now_ms;
        ble.transmit_longrange(UAS_data);
    }

    static uint32_t last_update_bt4_ms;
    int bt4_states = UAS_data.BasicIDValid[1] ? 7 : 6;
    if (g.bt4_rate > 0 &&
        now_ms - last_update_bt4_ms > (1000.0f/bt4_states)/g.bt4_rate) {
        last_update_bt4_ms = now_ms;
        ble.transmit_legacy(UAS_data);
    }

    // sleep for a bit for power saving
    delay(1);
}