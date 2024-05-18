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
#include "roboto.h"

TFT_eSPI tft = TFT_eSPI();

#define TFTW 320 // tft width
#define TFTH 480 // tft height

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
  char value[8];
  char previousValue[8];
  const char *text;
  unsigned long lastUpdate;
  int color;
};

#define TAG_COUNT 9
KeyValuePair addresses[TAG_COUNT] = {
    {"ff:ff:ff:ff:ff:ff", TAG_SINGLE_FLOAT, "????", "", "Vesi", 0, TFT_BLUE},
    {"48:70:1e:92:20:f2", TAG_MOPEKA, "????", "", "Gaas 1", 0, TFT_YELLOW},
    {"cc:33:31:c8:73:34", TAG_MOPEKA, "????", "", "Gaas 2", 0, TFT_YELLOW},
    {"ff:ff:ff:ff:ff:ff", TAG_DUAL_FLOAT, "????", "", "Must", 0, TFT_BLACK},
    {"ff:ff:ff:ff:ff:ff", TAG_DUAL_FLOAT, "????", "", "Hall", 0, TFT_BLACK},
    {"d4:aa:40:07:a8:0a", TAG_RUUVI, "????", "", "Kulmik", 0, TFT_GREEN},
    {"f9:e0:62:88:10:bb", TAG_RUUVI, "????", "", "Sugavkulm", 0, TFT_GREEN},
    {"d4:e1:02:40:59:96", TAG_RUUVI, "????", "", "Valistemp", 0, TFT_GREEN},
    {"dc:91:4d:4f:9d:08", TAG_RUUVI, "????", "", "Sisetemp", 0, TFT_GREEN},
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
  tft.setRotation(2);
  tft.loadFont(Roboto_16);
  tft.fillScreen(TFT_WHITE);
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
    if (millis() - tag->lastUpdate > 10000)
    {
      // If no update in last 10 sec, change text
      sprintf(tag->previousValue, "%s", tag->value);
      strncpy(tag->value, "----  ", sizeof(tag->value));
    }
    if (strcmp(tag->value, tag->previousValue) != 0)
    {
      tft.fillRect(0, drawy - 48 / 2, TFTW, 48, TFT_WHITE);
      tft.setTextDatum(3);
      tft.setTextColor(tag->color);
      tft.drawString(tag->text, 16, drawy);
      tft.setTextDatum(5);
      tft.drawString(tag->value, TFTW - 16, drawy);
    }
    drawy = drawy + 48 + 2;
  }
}

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
        handleDualFloat(device, tag);
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

    updateValue(tag, temperature, "%.1f `C");
  }

  struct mopeka_std_values
  {
    u_int16_t time_0 : 5;
    u_int16_t value_0 : 5;
    u_int16_t time_1 : 5;
    u_int16_t value_1 : 5;
    u_int16_t time_2 : 5;
    u_int16_t value_2 : 5;
    u_int16_t time_3 : 5;
    u_int16_t value_3 : 5;
  } __attribute__((packed));

  struct mopeka_std_package
  {
    u_int8_t data_0 : 8;
    u_int8_t data_1 : 8;
    u_int8_t raw_voltage : 8;

    u_int8_t raw_temp : 6;
    bool slow_update_rate : 1;
    bool sync_pressed : 1;

    mopeka_std_values val[4];
  } __attribute__((packed));

  void handleMopeka(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t* data = (uint8_t*)device.getManufacturerData().data();
    char* manufacturerdata = BLEUtils::buildHexData(NULL, (uint8_t*)device.getManufacturerData().data(), device.getManufacturerData().length());
    Serial.println(manufacturerdata);
    const auto *mopeka_data = (const mopeka_std_package *) &data;
    uint8_t temp_in_c = parse_temperature(mopeka_data);
    float lpg_speed_of_sound = get_lpg_speed_of_sound(temp_in_c);

    std::array<u_int8_t, 12> measurements_time = {};
    std::array<u_int8_t, 12> measurements_value = {};
    {
      u_int8_t measurements_index = 0;
      for (u_int8_t i = 0; i < 3; i++)
      {
        measurements_time[measurements_index] = mopeka_data->val[i].time_0 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_0;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_1 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_1;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_2 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_2;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_3 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_3;
        measurements_index++;
      }
    }

    u_int16_t best_value = 0;
    u_int16_t best_time = 0;
    {
      u_int16_t measurement_time = 0;
      for (u_int8_t i = 0; i < 12; i++)
      {
        // Time is summed up until a value is reported. This allows time values larger than the 5 bits in transport.
        measurement_time += measurements_time[i];
        if (measurements_value[i] != 0)
        {
          // I got a value
          if (measurements_value[i] > best_value)
          {
            // This value is better than a previous one.
            best_value = measurements_value[i];
            best_time = measurement_time;
          }
          // Reset measurement_time or next values.
          measurement_time = 0;
        }
      }
    }

    uint32_t distance_value = lpg_speed_of_sound * best_time / 100.0f;
    uint8_t tank_level = 0;
    if (distance_value >= 366)
    {
      tank_level = 100; // cap at 100%
    }
    else if (distance_value > 38)
    {
      tank_level = ((100.0f / (366 - 38)) * (distance_value - 38));
    }
    Serial.printf("%i level, %i dist, %i temp, %i time\n", tank_level, distance_value, temp_in_c, best_time);
    updateValue(tag, tank_level, "%.1f %%");
  }

  uint8_t parse_temperature(const mopeka_std_package *message)
  {
    uint8_t tmp = message->raw_temp;
    if (tmp == 0x0)
    {
      return -40;
    }
    else
    {
      return (uint8_t)((tmp - 25.0f) * 1.776964f);
    }
  }

  float get_lpg_speed_of_sound(float temperature)
  {
    float propane_butane_mix = 1;
    return 1040.71f - 4.87f * temperature - 137.5f * propane_butane_mix - 0.0107f * temperature * temperature -
           1.63f * temperature * propane_butane_mix;
  }

  void handleMopekaOld(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t *data = device.getPayload();
    int dataLen = device.getPayloadLength();

    // This is wrong since it should account for temperature
    // and speed of sound inside fluid
    uint8_t level = 0x35;
    for (uint8_t i = 8; i < 27; i++)
    {
      level ^= data[i];
    }
    level = level * .762; // cm

    updateValue(tag, level, "%.1f %%");
  }

  void handleDualFloat(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t *data = device.getPayload();

    updateValue(tag, data[10], "%.0f %%");

    KeyValuePair *tag2 = getTag("ff:ff:ff:ff:ff:ff");
    updateValue(tag2, data[9], "%.0f %%");
  }

  void handleSingleFloat(BLEAdvertisedDevice device, KeyValuePair *tag)
  {
    uint8_t *data = device.getPayload();
    updateValue(tag, data[9], "%.0f L");
  }

  void updateValue(KeyValuePair *tag, float value, char *format)
  {
    char newValue[8];
    sprintf(newValue, format, value);
    strncpy(tag->previousValue, tag->value, sizeof(tag->previousValue));
    strncpy(tag->value, newValue, sizeof(tag->value));
    tag->lastUpdate = millis();
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
