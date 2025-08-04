#include <Arduino.h>
#include <NimBLEDevice.h>
#include <opendroneid.h>  // ตรวจสอบ path ให้ถูกต้อง

#define ODID_MESSAGE_SIZE 25

// Encoded messages
ODID_BasicID_encoded BasicID_enc;
ODID_Location_encoded Location_enc;
ODID_Auth_encoded Auth0_enc, Auth1_enc;
ODID_SelfID_encoded SelfID_enc;
ODID_System_encoded System_enc;
ODID_OperatorID_encoded OperatorID_enc;

// Raw data
ODID_BasicID_data BasicID;
ODID_Location_data Location;
ODID_Auth_data Auth0, Auth1;
ODID_SelfID_data SelfID;
ODID_System_data System_data;
ODID_OperatorID_data OperatorID;

NimBLECharacteristic* pCharacteristic;

#define MINIMUM(a,b) (((a)<(b))?(a):(b))

void setupRemoteID() {
  // ----- BasicID -----
  odid_initBasicIDData(&BasicID);
  BasicID.IDType = ODID_IDTYPE_CAA_REGISTRATION_ID;
  BasicID.UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  strncpy(BasicID.UASID, "12345678901234567890", sizeof(BasicID.UASID));
  encodeBasicIDMessage(&BasicID_enc, &BasicID);

  // ----- Location -----
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
  encodeLocationMessage(&Location_enc, &Location);

  // ----- Auth (2 pages) -----
  odid_initAuthData(&Auth0);
  Auth0.AuthType = ODID_AUTH_UAS_ID_SIGNATURE;
  Auth0.DataPage = 0;
  Auth0.LastPageIndex = 1;
  Auth0.Length = 40;
  Auth0.Timestamp = 28000000;
  memcpy(Auth0.AuthData, "12345678901234567", 17);
  encodeAuthMessage(&Auth0_enc, &Auth0);

  odid_initAuthData(&Auth1);
  Auth1.AuthType = ODID_AUTH_UAS_ID_SIGNATURE;
  Auth1.DataPage = 1;
  memcpy(Auth1.AuthData, "12345678901234567890123", 23);
  encodeAuthMessage(&Auth1_enc, &Auth1);

  // ----- SelfID -----
  odid_initSelfIDData(&SelfID);
  SelfID.DescType = ODID_DESC_TYPE_TEXT;
  strncpy(SelfID.Desc, "DronesRUS: Real Estate", sizeof(SelfID.Desc));
  encodeSelfIDMessage(&SelfID_enc, &SelfID);

  // ----- System -----
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
  encodeSystemMessage(&System_enc, &System_data);

  // ----- OperatorID -----
  odid_initOperatorIDData(&OperatorID);
  OperatorID.OperatorIdType = ODID_OPERATOR_ID;
  strncpy(OperatorID.OperatorId, "98765432100123456789", sizeof(OperatorID.OperatorId));
  encodeOperatorIDMessage(&OperatorID_enc, &OperatorID);
}

void setupBLE() {
  NimBLEDevice::init("DroneID_BLE");
  NimBLEServer* pServer = NimBLEDevice::createServer();

  NimBLEService* pService = pServer->createService("0000fffa-0000-1000-8000-00805f9b34fb");
  pCharacteristic = pService->createCharacteristic(
    "0000fffb-0000-1000-8000-00805f9b34fb",
    NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->start();

  Serial.println("BLE Ready");
}

void sendMessage(const uint8_t* data, const char* label) {
  Serial.printf("Sending %s: ", label);
  for (int i = 0; i < ODID_MESSAGE_SIZE; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();

  pCharacteristic->setValue(data, ODID_MESSAGE_SIZE);
  pCharacteristic->notify();
  delay(100);  // Delay for receiver stability
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupRemoteID();
  setupBLE();
}

void loop() {
  sendMessage((uint8_t*)&BasicID_enc, "BasicID");
  sendMessage((uint8_t*)&Location_enc, "Location");
  sendMessage((uint8_t*)&Auth0_enc, "Auth0");
  sendMessage((uint8_t*)&Auth1_enc, "Auth1");
  sendMessage((uint8_t*)&SelfID_enc, "SelfID");
  sendMessage((uint8_t*)&System_enc, "System");
  sendMessage((uint8_t*)&OperatorID_enc, "OperatorID");

  Serial.println("All messages sent. Waiting 3 seconds...\n");
  delay(3000);
}
