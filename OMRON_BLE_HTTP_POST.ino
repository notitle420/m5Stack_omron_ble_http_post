#include <M5Stack.h>
#include "BLEDevice.h"
#include <WiFi.h>
#include <HTTPClient.h>
bool MODE = false; //false = BLEMODE true = WIFIMODE

const char* root_ca = "your_root_ca";

static String SensorAddr = "your_sensoraddr";
static BLEUUID serviceUUID("0c4c3000-7700-46f4-aa96-d5e974e32a54");
static BLEUUID    charUUID("0c4C3001-7700-46F4-AA96-D5E974E32A54");
static BLEAddress *pServerAddress;

char* ssid     = "ssid";
char* password = "password";

char* server = "script.google.com";
char* path = "your_path";

static boolean doConnect = false;

float temp = 0;
float humi = 0;
float press = 0;
float illuminance = 0;




//BLEのサーバーへ接続し、最新の値を取得する
bool connectToServer(BLEAddress pAddress) {
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());
  BLEClient*  pClient  = BLEDevice::createClient();
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
  // キャラクたりスティックを取得
  static BLERemoteCharacteristic *pRemoteCharacteristic;
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);

  //キャラクタリスティックを見つけられなかった場合
  if (pRemoteCharacteristic == nullptr) {
    return false;
  }

  //生データを取得
  std::string value = pRemoteCharacteristic->readValue();
  Serial.print("The characteristic value was: ");
  Serial.println(value.c_str());
  uint8_t* pData = pRemoteCharacteristic->readRawData();
  temp = (float)(pData[2] << 8 | pData[1]) / 100.0;  // データーから温度等を取り出す
  humi = (float)(pData[4] << 8 | pData[3]) / 100.0;
  press = (float)(pData[10] << 8 | pData[9]) / 10.0;
  illuminance = (float)(pData[6] << 8 | pData[5]);
  MODE = true;
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(5, 5);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(4);
  M5.Lcd.printf("temp:%.2f\n", temp);
  M5.Lcd.printf("hum:%.2f\n", humi);
  M5.Lcd.printf("prs:%.1f\n", press);
  M5.Lcd.printf("ilm:%.1f\n", illuminance);
}


//BLEをスキャンするクラス
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //BLEScan* pBLEScan = BLEDevice::getScan();
      Serial.println("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());


      String deviceAddress = advertisedDevice.getAddress().toString().c_str();
      //Serial.println(deviceAddress.c_str());
      //ペリフェラルをアドレスで判断
      if (deviceAddress.equalsIgnoreCase(SensorAddr)) {
        advertisedDevice.getScan()->stop();
        Serial.print("Found our device!  address: ");
        //サーバーのアドレスを変数に代入
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
      }
    }
};




void setup() {
  //print_wakeup_reason();
  M5.begin();
  Serial.begin(115200);
  set_BLE();
}


//BLEをセットアップ
void set_BLE() {
  Serial.println("BLE_START");
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
  Serial.println("SCAN_START");
  pBLEScan->stop();
}

//wifiをセットアップ
void set_wifi() {
  Serial.println("WIFI_START");
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
    delay(5000);
  } else {
    delay(5000);
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
        doConnect = false;
      }
    }
  } else { //Wifi Modeの時
    //BLEclientをストップ
    delay(1000);
    //スキャンをストップ
    delay(1000);
    //BLE関連をシャットダウン
    BLEDevice::deinit(true);
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
    ESP.restart();
  }
} // End of loop
