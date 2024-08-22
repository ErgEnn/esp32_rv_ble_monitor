#include <Arduino.h>

// #define ST7796_DRIVER
// #define TFT_MISO  19
// #define TFT_MOSI  5
// #define TFT_SCLK  18
// #define TFT_CS    15
// #define TFT_DC    4
// #define TFT_RST   2
// #define TOUCH_CS 22
#include <SPI.h>
#include <TFT_eSPI.h>
#include "roboto_bold_38.h"

TFT_eSPI tft = TFT_eSPI();

#define TFTW 320 // tft width
#define TFTH 480 // tft height
#define BG TFT_BLACK

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define TAG_RUUVI 1
#define TAG_MOPEKA 2
#define TAG_DUAL_FLOAT 3
#define TAG_SINGLE_FLOAT 4

BLEScan *pBLEScan;

typedef struct KeyValuePair
{
  const char *address;
  int tagType;
  char value[10];
  char previousValue[10];
  const char *text;
  unsigned long lastUpdate;
  int color;
};

#define TAG_COUNT 9
KeyValuePair addresses[TAG_COUNT] = {
    {"c0:49:ef:d3:74:76", TAG_DUAL_FLOAT, "????", "", "Vesi", 0, TFT_BLUE},
    {"48:70:1e:92:20:f2", TAG_MOPEKA, "????", "", "Gaas 1", 0, 0x06af},
    {"cc:33:31:c8:73:34", TAG_MOPEKA, "????", "", "Gaas 2", 0, 0x06af},
    {"ff:ff:ff:ff:ff:01", TAG_DUAL_FLOAT, "????", "", "Must", 0, TFT_WHITE},
    {"ff:ff:ff:ff:ff:02", TAG_DUAL_FLOAT, "????", "", "Hall", 0, TFT_WHITE},
    {"d4:aa:40:07:a8:0a", TAG_RUUVI, "????", "", "Külmik", 0, 0x00ff42},
    {"f9:e0:62:88:10:bb", TAG_RUUVI, "????", "", "Sügavk.", 0, 0x00ff42},
    {"d4:e1:02:40:59:96", TAG_RUUVI, "????", "", "Välis", 0, 0x00ff42},
    {"dc:91:4d:4f:9d:08", TAG_RUUVI, "????", "", "Sise", 0, 0x00ff42},
};

KeyValuePair *getTag(const char *address)
{
  for (int i = 0; i < TAG_COUNT; i++)
  {
    if (strcmp(addresses[i].address, address) == 0)
    {
      return &addresses[i];
    }
  }
  return NULL;
}

void setup()
{
  tft.init();
  tft.setRotation(0);
  tft.loadFont(Roboto_Bold_38);
  tft.fillScreen(BG);
  setupBluetooth();
  Serial.begin(115200);
}

void loop()
{
  BLEScanResults foundDevices = pBLEScan->start(5, false);
  pBLEScan->clearResults();
  tft.setCursor(0, 0, 4);
  tft.setTextSize(1);
  int drawy = 48 / 2 + 10;
  for (int i = 0; i < TAG_COUNT; i++)
  {
    KeyValuePair *tag = &addresses[i];
    if (millis() - tag->lastUpdate > 30000)
    {
      // If no update in last 30 sec, change text
      sprintf(tag->previousValue, "%s", tag->value);
      strncpy(tag->value, "----  ", sizeof(tag->value));
    }
    if (strcmp(tag->value, tag->previousValue) != 0)
    {
      tft.fillRect(0, drawy - 48 / 2, TFTW, 48, BG);
      tft.setTextDatum(3);
      tft.setTextColor(tag->color);
      tft.drawString(tag->text, 16, drawy);
      tft.setTextDatum(5);
      tft.drawString(tag->value, TFTW - 16, drawy);
    }
    drawy = drawy + 48 + 2;
  }
}

template <size_t MaxSize>
class List {
private:
    double data[MaxSize];
    size_t size;

public:
    List() : size(0) {}

    void append(double value) {
      if(size >= MaxSize){
        Serial.println("Overappend");
        return;
      }
      data[size++] = value;
    }

    void set(size_t index, double value) {
      if(index > MaxSize){
        Serial.printf("set index %i over max size %i\n", index, MaxSize);
        return;
      }
      data[index] = value;
    }

    void incr(size_t index, double value) {
      if(index > MaxSize){
        Serial.printf("incr index %i over max size %i\n", index, MaxSize);
        return;
      }
      data[index] += value;
    }

    double operator[](size_t index) const {
      if(index > MaxSize){
        Serial.printf("get index %i over max size %i\n", index, MaxSize);
        return 0;
      }
        return data[index];
    }

    void dump(const char suffix) {
      Serial.printf("len(%c) = %i\n", suffix, size);
      for(int i = 0; i < size; i++){
        Serial.printf("%d = %.0f %c\n", i, data[i], suffix);
      }
    }

    size_t getSize() const {
        return size;
    }
};


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice device)
  {
    // Serial.print("Found: ");
    // Serial.println(device.getAddress().toString().c_str());
    KeyValuePair *tag = getTag(device.getAddress().toString().c_str());
    if (tag == NULL)
      return;
    switch(tag->tagType) {
      case TAG_RUUVI:
        handleRuuvi(device, tag);
        break;
      case TAG_MOPEKA:
        handleMopeka(device, tag);
        break;
      case TAG_DUAL_FLOAT:
        handleMultiFloat(device, tag);
        break;
      case TAG_SINGLE_FLOAT:
        handleSingleFloat(device, tag);
        break;
      default:
        break;
    }
  }

  void handleRuuvi(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    int dataLen = device.getPayloadLength();
    uint8_t *data = device.getPayload();
    int payloadStart = 8;
    int16_t raw_temperature = (int16_t)data[payloadStart] << 8 | (int16_t)data[payloadStart + 1];
    int16_t raw_humidity = (int16_t)data[payloadStart + 2] << 8 | (int16_t)data[payloadStart + 3];

    float temperature = raw_temperature * 0.005; // Celcius
    // float humidity = raw_humidity * 0.0025; // Percent

    updateValue(tag, temperature, "%.1f °C");
  }
  
  void handleMultiFloat(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t *data = device.getPayload();

    updateValue(tag, data[9], "%.0f L");

    KeyValuePair *tag2 = getTag("ff:ff:ff:ff:ff:01");
    updateValue(tag2, data[10], "%.0f %%");
    KeyValuePair *tag3 = getTag("ff:ff:ff:ff:ff:02");
    updateValue(tag3, data[11], "%.0f %%");
  }

  void handleSingleFloat(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t *data = device.getPayload();
    updateValue(tag, data[9], "%.0f L");
  }

  void updateValue(KeyValuePair *tag, float value, char *format)
  {
    char newValue[10];
    sprintf(newValue, format, value);
    strncpy(tag->previousValue, tag->value, sizeof(tag->previousValue));
    strncpy(tag->value, newValue, sizeof(tag->value));
    tag->lastUpdate = millis();
  }

  // ----------------- MOPEKA ----------------
 
  double a(double a, double b){
      double c = .5 * 1.2421875;
      double e = (255 - b) / 256;
      if(c >= a){
          return 0;
      }
      return (a-c)*e;
  }

  template <size_t Size,size_t Size2>
  void loop1(List<Size>& k,List<Size2>& advIndex){
      int b = 1;
      while(b<advIndex.getSize()){
        int g = advIndex[b-1] / 2;
        int h = advIndex[b] / 2;
        if((h-1) != g){
            k.append(b);
        }
        b+=1;
      }
  }

  template <size_t Size, size_t Size2, size_t Size3, size_t Size4>
  void loop2(List<Size>& adv, List<Size2>& k, List<Size3>& g, List<Size4>& l){
      int h = k.getSize();
      int p = 0;
      int q = 0;
      int f = 0;
      for(int b = 0; b < h; b++){
          int n = 0;
          int m = 0;
          if(b == h-1){
              q = adv.getSize();
          }else{
              q = k[b+1];
          }
          f = p;
          while(f < q){
              if(adv[f > m]){
                  n = f;
                  m = adv[f];
              }
              f+=1;
          }
          g.append(n);
          f = p;
          while (f < q){
              l.append(m);
              f+=1;
          }
          p = q;
      }
  }

  template <size_t Size, size_t Size2, size_t Size3>
  void loop3( List<Size>& adv, List<Size2>& advIndex, List<Size3>& n){
      int b = 0;
      while (b < adv.getSize()){
          int l = advIndex[b] / 2;
          double p = a(adv[b], l);
          n.incr(l, .5 * p);
          int f = b + 1;
          while (f < adv.getSize()){
              int q = advIndex[f] / 2;
              double r = a(adv[f], q);
              if(p < r){
                  r = p;
              }
              n.incr(q - l, r);
              f += 1;
          }
          b += 1;
      }
  }

  template <size_t Size, size_t Size2, size_t Size3, size_t Size4, size_t Size5>
  void loop4( List<Size>& adv, List<Size2>& advIndex, List<Size3>& k, List<Size4>& g, List<Size5>& m){
      int b = 0;
      while (b < k.getSize()){
          double l = advIndex[g[b]] / 2;
          double p = advIndex[g[b]];
          m.incr(l, a(p, l));
          int f = b + 1;
          while (f < k.getSize()){
              int q = advIndex[g[f]] / 2;
              m.incr(q - l, (a(adv[g[f]], q) + a(p, l)) / 2);
              f += 1;
          }
          b += 1;
      }
  }

  template <size_t Size>
  void fillZeroes(List<Size>& list){
      for(int b = 0; b < 100; b++){
          list.append(0);
      }
  }

  template <size_t Size>
  double loop5(List<Size>& score_filt){
      double n = 0;
      double m = .005;
      int b = 2;
      while (b < 100){
          if (score_filt[b] > m){
              m = score_filt[b];
              n = b;
          }
          b += 1;
      }
      return n;
  }

  template <size_t Size>
  double max(List<Size>& list){
      double max = 0;
      for(int i = 0; i < list.getSize(); i++){
          if(list[i] > max){
              max = list[i];
          }
      }
      return max;
  }

  template <size_t Size, size_t Size2, size_t Size3>
  void scoreProcessing(List<Size>& scoreFilt, List<Size2>& n, List<Size3>& m){
      scoreFilt.append(.25 * n[1] + .5 * n[0]);
      scoreFilt.incr(0, .5 * m[1] + .5 * m[0]);
      int b = 1;
      while (b < 99){
          scoreFilt.append(.25 * (n[b - 1] + n[b + 1]) + .5 * n[b]);
          scoreFilt.incr(b, .5 * (m[b - 1] + m[b + 1]) + .5 * m[b]);
          b += 1;
      }
      scoreFilt.append(.25 * n[98] + .5 * n[99]);
      scoreFilt.incr(99, .5 * m[98] + .5 * m[99]);
  }

  template <size_t Size, size_t Size2>
  float GetPulseEchoTime(List<Size>& adv, List<Size2>& advIndex){
      List<12> k;
      loop1(k, advIndex);

      List<12> g;
      List<100> l;
      loop2(adv, k, g, l);

      List<100> n;
      List<100> m;
      fillZeroes(n);
      fillZeroes(m);

      loop3(adv, advIndex, n);

      loop4(adv, advIndex, k, g, m);
      
      List<100> score_filt;
      scoreProcessing(score_filt, n, m);
      
      double loop5_result = loop5(score_filt);

      if (.63453125 >= max(adv)){
          return 0;
      }
      return 2E-5 * loop5_result;
  }

  double convertLevelToInches(double level, double temperature){
    double r = temperature;
    double a = r * r;
    
    if (r <= -39){
      if (level <= 0){
        return 0;
      }
      return 1E3 * level * 1E3 * 0.015 + 0.5;
    }
    double n = 1040.71 - 4.87 * r - 137.5 - .0107 * r * r - 1.63 * r;
            
    if(level <= 0){
      return 0;
    }

    return level * n / 2 * 39.3701;
  }

  int getPercentFromHeight(double e){
    double tank_max_height = 0.366;
    double tank_min_offset = 0.0381;

    double p = 100 * (e - tank_min_offset) / (tank_max_height - tank_min_offset);
    if(p < 0){
      return 0;
    }
    if(p > 100){
      return 100;
    }

    return (int) p;
  }

  void handleMopeka(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t* data = (uint8_t*)device.getManufacturerData().data();
    // char* manufacturerdata = BLEUtils::buildHexData(NULL, (uint8_t*)device.getManufacturerData().data(), device.getManufacturerData().length());
    // Serial.println(manufacturerdata);

    int r = 63 & data[5];
    float temperature;
    if(r == 0){
      temperature = -40;
    }else{
      temperature = 1.776964 * (r - 25);
    }

    int t = 0;
    int n = 0;
    r = 6;
    List<12> adv;
    List<12> advIndex;
    for (byte a = 0; a < 12; a++)
    {
      int i = 10 * a;
      int o = i / 8;
      int c = i % 8;
      int s = data[r + o] + 256 * data[r + o + 1];
      s >>= c;
      int l = 1 + (31 & s);
      s >>= 5;
      int u = 31 & s;
      int A = n + l;
      n = A;
      if(A > 255){
        break;
      }
      if(u != 0){
        u -= 1;
        u *= 4;
        u += 6;
        adv.append(u);
        advIndex.append(2*A);
        t += 1;
      }
    }
    
    float level = GetPulseEchoTime(adv, advIndex);
    float inches = convertLevelToInches(level, temperature);
    double meters = inches / 39.3701;
    int percent = getPercentFromHeight(meters);
    updateValue(tag, percent, "%.0f %%");
  }

};

void setupBluetooth()
{
  BLEDevice::init("Monitor");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}
