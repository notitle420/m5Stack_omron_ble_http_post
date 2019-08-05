#include "BLEDevice.h"
#include <WiFi.h>
#include <HTTPClient.h>
bool MODE = false; //false = BLEMODE true = WIFIMODE
BLEDevice* device;
BLEScan* pBLEScan;
static BLERemoteCharacteristic *pRemoteCharacteristic;
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  180        /* Time ESP32 will go to sleep (in seconds) */




char* ssid     = "ssid";
char* password = "password";

//postするサーバー
char* server = "script.google.com";
char* path = "/macros/s/{sheedID}/exec";

BLEUUID serviceUUID("0c4c3000-7700-46f4-aa96-d5e974e32a54");
BLEUUID    charUUID("0c4C3001-7700-46F4-AA96-D5E974E32A54");


const char* root_ca = "root_ca";

                      static BLEAddress *pServerAddress;
BLEClient*  pClient = NULL;
static boolean doConnect = false;
float temp = 0;
float humi = 0;
float press = 0;
float illuminance = 0;


//BLEのサーバーへ接続し、最新の値を取得する
bool connectToServer(BLEAddress pAddress) {
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());
  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");

  //BLEサーバーへ接続
  bool _connected = pClient->connect(pAddress, BLE_ADDR_TYPE_RANDOM);
  if (!_connected) {
    delete pClient;
    return false;
    Serial.println("not connected");
  }
  Serial.println(" - Connected to server");

  // サービスを取得
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
  // キャラクたりスティックを取得
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);

  //キャラクタリスティックを見つけられなかった場合
  if (pRemoteCharacteristic == nullptr) {
    ;
    return false;
  }

  //生データを取得
  uint8_t* pData = pRemoteCharacteristic->readRawData();
  temp = (float)(pData[2] << 8 | pData[1]) / 100.0;  // データーから温度等を取り出す
  humi = (float)(pData[4] << 8 | pData[3]) / 100.0;
  press = (float)(pData[10] << 8 | pData[9]) / 10.0;
  illuminance = (float)(pData[6] << 8 | pData[5]);
  MODE = true;
}


//BLEをスキャンするクラス
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
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
      }
    }
};


//deep_sleepから立ち上がった時
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}


void setup() {
  print_wakeup_reason();
  Serial.begin(115200);
  set_BLE();
}


//BLEをセットアップ
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

//wifiをセットアップ
void set_wifi() {
  Serial.println("WIFI_START");
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
  //json形式直書き
  char json[100];
  sprintf(json, "{\"temp\": %2.2f , \"humid\": %.1f , \"pres\": %2.2f , \"shodo\": %.1f}", temp, humi , press, illuminance);
  Serial.print("[HTTP] begin...\n");
  HTTPClient http;
  http.begin(server, 443, path, root_ca);
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

void loop() {
  //BLEモードの時
  if (!MODE)  {
    if (doConnect == true) {
      if (connectToServer(*pServerAddress)) {
        Serial.println("We are now connected to the BLE Server.");
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
      doConnect = false;
    }
  } else { //Wifi Modeの時
    //BLEclientをストップ
    pClient->disconnect();
    delay(1000);
    //スキャンをストップ
    pBLEScan->stop();
    delay(1000);
    //BLE関連をシャットダウン
    device->deinit(true);
    delay(10000);

    //WifiStart
    set_wifi();
    delay(10000);

    //データをポスト
    data_post();
    delay(10000);
    //wifiを切断
    while (WiFi.status() == WL_CONNECTED ) {
      WiFi.disconnect(true);
      delay(2000);
      Serial.println("disconnectting...");
    }
    Serial.println("disconected");
    //ディープスリープを設定し、スタート
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                   " Seconds");
    Serial.flush();
    esp_deep_sleep_start();
  }
  delay(1000);
} // End of loop
