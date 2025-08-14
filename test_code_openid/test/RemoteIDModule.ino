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
  // Location
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
  UAS_data.Location.TimeStamp = 1234.0f;

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
  UAS_data.System.Timestamp = 12345678;

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
  // ble.transmit_legacy(UAS_data);
  ble.transmit_longrange(UAS_data);
  wifi.transmit_nan(UAS_data);
  wifi.transmit_beacon(UAS_data);

  delay(1000);
}
