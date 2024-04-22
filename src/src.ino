#include <Arduino.h>


//#define ST7796_DRIVER
//#define TFT_MISO  19
//#define TFT_MOSI  5
//#define TFT_SCLK  18
//#define TFT_CS    15
//#define TFT_DC    4
//#define TFT_RST   2
//#define TOUCH_CS 22 
#include <SPI.h>
#include <TFT_eSPI.h>
#include "roboto.h"

TFT_eSPI tft = TFT_eSPI();

#define TFTW 320 // tft width
#define TFTH 480 // tft height

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define TAG_RUUVI   1
#define TAG_MOPEKA  2
#define TAG_DUAL_FLOAT  3

BLEScan *pBLEScan;

typedef struct KeyValuePair {
  const char* address;
  int tagType;
  float value;
  float previousValue;
  const char* text;
  const char* unit;
};

#define TAG_COUNT 9
KeyValuePair addresses[TAG_COUNT] = {
  {"", TAG_RUUVI, 0.0f, 0.1f, "Vesi", "L"},
  {"48:70:1e:92:20:f2", TAG_MOPEKA, 0.0f, 0.1f, "Gaas 1", "%"},
  {"", TAG_MOPEKA, 0.0f, 0.1f, "Gaas 2", "%"},
  {"0c:b8:15:cd:31:fe", TAG_DUAL_FLOAT, 0.0f, 0.1f, "Must", "%"},
  {"alt", TAG_DUAL_FLOAT, 0.0f, 0.1f, "Hall", "%"},
  {"", TAG_RUUVI, 0.0f, 0.1f, "Kulmik", "`C"},
  {"", TAG_RUUVI, 0.0f, 0.1f, "Sugavkulm", "`C"},
  {"", TAG_RUUVI, 0.0f, 0.1f, "Valistemp", "`C"},
  {"d4:aa:40:07:a8:0a", TAG_RUUVI, 0.0f, 0.1f, "Sisetemp", "`C"},
};

KeyValuePair* getTag(const char* address) {
  for(int i = 0; i < TAG_COUNT; i++) {
    if (strcmp(addresses[i].address, address) == 0) {
      return &addresses[i];
    }
  }
  return NULL;
}

void setup() {
  tft.init();
  tft.setRotation(2);
  tft.loadFont(Roboto_16);
  tft.fillScreen(TFT_BLACK);
  setupBluetooth();
  Serial.begin(115200);
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(5, false);
  pBLEScan->clearResults();
  tft.setCursor(0,0, 4);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  int drawy = 48/2 + 10;
  for (int i = 0; i < TAG_COUNT; i++)
  {
    KeyValuePair* tag = &addresses[i];
    if(tag->value != tag->previousValue){
      tft.fillRect(0, drawy - 48/2, TFTW, 48, TFT_BLACK);
      tft.setTextDatum(3);
      tft.drawString(tag->text, 16, drawy);
      char signedValue[6];
      sprintf(signedValue, "%.1f %s", tag->value, tag->unit);
      tft.setTextDatum(5);
      tft.drawString(signedValue, TFTW-16, drawy);
    }
    drawy = drawy + 48 + 2;
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    //Serial.print("Found: ");
    //Serial.println(device.getAddress().toString().c_str());
    KeyValuePair* tag = getTag(device.getAddress().toString().c_str());
    if(tag == NULL) return;
    switch (tag->tagType){
      case TAG_RUUVI:
        handleRuuvi(device, tag);
        break;
      case TAG_MOPEKA:
        handleMopeka(device, tag);
        break;
      case TAG_DUAL_FLOAT:
        handleDualFloat(device, tag);
        break;
      default:
        break;
    }
  }

  void handleRuuvi(BLEAdvertisedDevice device, KeyValuePair* tag){
    int dataLen = device.getPayloadLength();
    uint8_t* data = device.getPayload();
    int payloadStart = 8;
    int16_t raw_temperature = (int16_t)data[payloadStart]<<8 | (int16_t)data[payloadStart+1];
    int16_t raw_humidity = (int16_t)data[payloadStart+2]<<8 | (int16_t)data[payloadStart+3];
  
    float temperature = raw_temperature * 0.005; // Celcius
    //float humidity = raw_humidity * 0.0025; // Percent
    tag->previousValue = tag->value;
    tag->value = temperature;
  }

  void handleMopeka(BLEAdvertisedDevice device, KeyValuePair* tag){
    uint8_t* data = device.getPayload();
    int dataLen = device.getPayloadLength();

    // This is wrong since it should account for temperature
    // and speed of sound inside fluid
    uint8_t level = 0x35;
    for (uint8_t i = 8; i < 27; i++) {
      level ^= data[i];
    }
    tag->previousValue = tag->value;
    tag->value = level * .762; // cm
  }

  void handleDualFloat(BLEAdvertisedDevice device, KeyValuePair* tag){
    uint8_t* data = device.getPayload();
    tag->previousValue = tag->value;
    tag->value = data[10];
    KeyValuePair* tag2 = getTag("alt");
    tag2->previousValue = tag2->value;
    tag2->value = data[9];
  }
};

void setupBluetooth(){
  BLEDevice::init("Monitor");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); 
}
