#include "BLEDevice.h"
#include <WiFi.h>
#include <HTTPClient.h>
bool MODE = false; //false = BLEMODE true = WIFIMODE
BLEDevice* device;
BLEScan* pBLEScan;
static BLERemoteCharacteristic *pRemoteCharacteristic;
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  180        /* Time ESP32 will go to sleep (in seconds) */


const char* shome = \
                    "-----BEGIN CERTIFICATE-----\n" \
                    "MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n" \
                    "A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\n" \
                    "b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\n" \
                    "MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\n" \
                    "YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\n" \
                    "aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\n" \
                    "jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\n" \
                    "xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\n" \
                    "1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\n" \
                    "snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\n" \
                    "U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\n" \
                    "9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\n" \
                    "BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\n" \
                    "AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\n" \
                    "yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\n" \
                    "38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\n" \
                    "AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\n" \
                    "DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\n" \
                    "HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n" \
                    "-----END CERTIFICATE-----\n";


//static String omronSensorAdress = "ff:f7:ef:33:f4:31";
//static BLEUUID serviceUUID("0c4c3000-7700-46f4-aa96-d5e974e32a54");
//static BLEUUID    charUUID("0c4C3001-7700-46F4-AA96-D5E974E32A54");

static BLEAddress *pServerAddress;
BLEClient*  pClient = NULL;
static boolean doConnect = false;
float temp = 0;
float humi = 0;
float press = 0;
float illuminance = 0;

//
//static void notifyCallback(
//  BLERemoteCharacteristic* pBLERemoteCharacteristic,
//  uint8_t* pData,
//  size_t length,
//  bool isNotify) {
//  //  Serial.print("Notify callback for characteristic ");
//  //  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
//  //  Serial.print(" of data length ");
//  //  Serial.println(length);
//  temp = (float)(pData[2] << 8 | pData[1]) / 100.0;  // データーから温度等を取り出す
//  press = (float)(pData[10] << 8 | pData[9]) / 10.0;
//  illuminance = (float)(pData[6] << 8 | pData[5]);
//  //  Serial.print("temp:");
//  //  Serial.println(temp);
//  //  Serial.print("press:");
//  //  Serial.println(press);
//  //  Serial.print("shodo:");
//  //  Serial.println(illuminance);
//  MODE = true;
//  pRemoteCharacteristic->~BLERemoteCharacteristic();
//}

bool connectToServer(BLEAddress pAddress) {
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());
  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");

  // Connect to the remove BLE Server.
  bool _connected = pClient->connect(pAddress, BLE_ADDR_TYPE_RANDOM);
  if (!_connected) {
    delete pClient;
    return false;
    Serial.println("not connected");
  }
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLEUUID serviceUUID("0c4c3000-7700-46f4-aa96-d5e974e32a54");
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);

  Serial.println("Getting Service");

  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    return false;
  } else {
    Serial.println(" - Found our service");
  }
  static BLERemoteCharacteristic *pRemoteCharacteristic;
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLEUUID    charUUID("0c4C3001-7700-46F4-AA96-D5E974E32A54");
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);

  //Serial.println("Getting Chara");

  if (pRemoteCharacteristic == nullptr) {
    //Serial.print("Failed to find our characteristic UUID: ");
    //Serial.println(charUUID.toString().c_str());
    return false;
  }
  //Serial.println(" - Found our characteristic");
  // Read the value of the characteristic.
  std::string value = pRemoteCharacteristic->readValue();
  //Serial.print("The characteristic value was: ");
  //Serial.println(value.c_str());
  uint8_t* pData = pRemoteCharacteristic->readRawData();
  temp = (float)(pData[2] << 8 | pData[1]) / 100.0;  // データーから温度等を取り出す
  humi = (float)(pData[4] << 8 | pData[3]) / 100.0;
  press = (float)(pData[10] << 8 | pData[9]) / 10.0;
  illuminance = (float)(pData[6] << 8 | pData[5]);
  MODE = true;
  //pRemoteCharacteristic->registerForNotify(notifyCallback);
}
/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      BLEScan* pBLEScan = BLEDevice::getScan();
      Serial.println("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());


      String deviceAddress = advertisedDevice.getAddress().toString().c_str();
      //Serial.println(deviceAddress.c_str());
      //ペリフェラルをアドレスで判断
      if (deviceAddress.equalsIgnoreCase("ff:f7:ef:33:f4:31")) {
        advertisedDevice.getScan()->stop();
        Serial.print("Found our device!  address: ");
        //サーバーのアドレスを変数に代入
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
      }// Found our server
    } // onResult
}; // MyAdvertisedDeviceCallbacks

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


void setup() {
  print_wakeup_reason();
  Serial.begin(115200);
  set_BLE();
} // End of setup.

void set_BLE() {
  Serial.println("BLE_START");
  BLEDevice::init("");
  //device->init("");
  pBLEScan = device->getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
  Serial.println("SCAN_START");
}

void set_wifi() {
  Serial.println("WIFI_START");
  char* ssid     = "mudaiphonexr";
  char* password = "1qa2ws3ed";
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void data_post() {
  char json[100];
  sprintf(json, "{\"temp\": %2.2f , \"humid\": %.1f , \"pres\": %2.2f , \"shodo\": %.1f}", temp, humi ,press, illuminance);
  Serial.print("[HTTP] begin...\n");
  HTTPClient http;
  http.begin("script.google.com", 443, "/macros/s/AKfycbxt1Trb-Sq5kigRTLGnaRszT4yNEhCekb3ALf1DGEaDI5e2YnU/exec", shome);
  Serial.print("[HTTP] POST...\n");
  int httpCode = http.POST(json);
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    delay(1000);
  } else {
    delay(1000);
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// This is the Arduino main loop function.
void loop() {
  if (!MODE)  {
    if (doConnect == true) {
      if (connectToServer(*pServerAddress)) {
        Serial.println("We are now connected to the BLE Server.");
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
      doConnect = false;
    } // Delay a second between loops.
  } else {
    pClient->disconnect();
    delay(1000);
    pBLEScan->stop();
    delay(1000);
    device->deinit(true);
    delay(10000);
    set_wifi();
    delay(10000);
    data_post();
    delay(10000);
    while (WiFi.status() == WL_CONNECTED ) {
      WiFi.disconnect(true);
      delay(2000);
      Serial.println("disconnectting...");
    }
    Serial.println("disconected");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
  Serial.flush(); 
    esp_deep_sleep_start();
  }
  delay(1000);
} // End of loop
