#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <time.h>
#include "epd_driver.h"
#include "opensans8b.h"
#include "opensans10b.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"
#include "opensans26b.h"
#include "lang.h"
#include "weather_data.h"
#include "ftp_server.h"
#include "web_server.h"
#include "esp_adc_cal.h"
#include "param_data.h"

#define PRINT_PARAM 1
#define PRINT_DATA 0
#define SAVE_LAST_DATA 1

#define AP_SSID "WEATHER_STATION"
#define AP_PASS "0123456789"

#define White 0xFF
#define LightGrey 0xBB
#define Grey 0x88
#define DarkGrey 0x44
#define Black 0x00

const char *ntpServer = "0.europe.pool.ntp.org";


enum alignment
{
  LEFT,
  RIGHT,
  CENTER
};

enum icon_size
{
  SmallIcon,
  LargeIcon
};

uint8_t currentHour = 0, currentMin = 0, currentSec = 0, eventCnt = 0;
long sleepDuration = 60; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
uint8_t wakeupHour = 5;  // Don't wakeup until after 04:00 to save battery power
uint8_t sleepHour = 1;   // Sleep after 01:00 to save battery power
long startTime = 0;
long sleepTimer = 0;
long delta = 30; // ESP32 rtc speed compensation, prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power

#define L_SIZE 250
#define S_SIZE 100

#define Large 20 // For icon drawing
#define Small 8  // For icon drawing

Web_Server server;
FTP_Server ftp;
param_t param;
weather_t weather;
int wifi_signal = -120;

GFXfont currentFont;
uint8_t *displayBuffer;

uint8_t start_WiFi();
void stop_WiFi();
boolean setup_time();
boolean update_local_time();
void begin_sleep();
void ap_config();
bool decode_json(char *jsonStr, int size);
bool getWeather();
void display_weather();
void display_info();
String convert_unix_time(int unix_time);
void draw_battery(int x, int y);
void draw_RSSI(int x, int y, int rssi);
void display_fact_weather();
void display_forecast_weather();
void draw_wind_section(int x, int y, String dir, float speed, float gust, int Cradius, bool fact);
void draw_thp_section(uint16_t x, uint16_t y);
void draw_sun_section(uint16_t x, uint16_t y);
void draw_moon_section(uint16_t x, uint16_t y, String hemisphere);
void draw_thp_forecasr_section(uint16_t x, uint16_t y, uint8_t part);
uint8_t *load_file(String fileName);
void draw_conditions_section(int x, int y, String IconName, uint8_t forecast_part, bool IconSize);
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength);
void fillCircle(int x, int y, int r, uint8_t color);
void drawString(int x, int y, String text, alignment align);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void drawCircle(int x0, int y0, int r, uint8_t color, bool fill);
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void drawPixel(int x, int y, uint8_t color);
void setFont(GFXfont const &font);
void edp_update();

void setup()
{
  bool _settingsEn = false;
  pinMode(39, INPUT_PULLUP);
  if (SPIFFS.begin())
  {
    epd_init();
    displayBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!displayBuffer)
      log_i("Memory alloc failed!");
    memset(displayBuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    log_i("SPIFFS begin");

    if (SPIFFS.exists("/param.json"))
    {
      File f = SPIFFS.open("/param.json", FILE_READ);
      int _size = f.size();
      char _param[_size];
      f.readBytes(_param, _size);
      DynamicJsonDocument jsonDoc(_size);                            // allocate the JsonDocument
      DeserializationError error = deserializeJson(jsonDoc, _param); // Deserialize the JSON document
      if (error)
      {
        log_i("deserializeJson() failed: %s", error.c_str());
      }
      else
      {
        // convert it to a JsonObject
        JsonObject jo = jsonDoc.as<JsonObject>();
        param.city = jo["city"].as<char *>();
        param.lat = jo["lat"].as<float>();
        param.lon = jo["lon"].as<float>();
        param.test_data = jo["test_data"].as<bool>();
        param.api_key = jo["api_key"].as<char *>();
        param.update_interval = jo["update_interval"].as<uint8_t>();
        param.time_zone = jo["time_zone"].as<int8_t>();
        param.ap_ssid = jo["ap_ssid"].as<char *>();
        param.ap_pass = jo["ap_pass"].as<char *>();
        param.hemisphere = jo["hemisphere"].as<char *>();
#if PRINT_PARAM
        log_i("\tcity: %s", param.city.c_str());
        log_i("\tlat: %s", String(param.lat, 6).c_str());
        log_i("\tlon: %s", String(param.lon, 6).c_str());
        log_i("\themisphere: %s", param.hemisphere.c_str());
        log_i("\ttest_data: %d", param.test_data);
        log_i("\tapi_key: %s", param.api_key.c_str());
        log_i("\tupdate_interval: %d", param.update_interval);
        log_i("\time_zone: %d", param.time_zone);
        log_i("\tap_ssid: %s", param.ap_ssid.c_str());
        log_i("\tap_pass: %s", param.ap_pass.c_str());
        log_i("param deserializeJson() success");
#endif
      }
    }
    else
    {
      log_i("param.json file not found");
    }

    if ((!digitalRead(39)) || (param.api_key == ""))
    {
      _settingsEn = true;
      if (!digitalRead(39))
        log_i("IO39 is pressing");
      if (param.api_key == "")
        log_i("api_key is empty");
      ap_config();
      server.begin(&SPIFFS);
      ftp.addFilesystem("SPIFFS", &SPIFFS);
      ftp.begin();
      epd_poweron();
      epd_clear();
      edp_update();

      setFont(OpenSans26B);
      drawString(30, 20, F("Weather station not configured!"), LEFT);

      setFont(OpenSans18B);
      drawString(30, 80, F("Wi-Fi access point  is started!"), LEFT);

      setFont(OpenSans12B);
      drawString(30, 140, "SSID: " + String(AP_SSID), LEFT);
      drawString(30, 180, "PASSWORD: " + String(AP_PASS), LEFT);
      drawString(30, 210, "Settigns page: http://192.168.4.1/index.html", LEFT);
      drawString(30, 250, "FTP-server is started. user: esp32, pass: esp32", LEFT);
      edp_update();

      uint8_t *_data;
      _data = load_file("wifi_img.bin");
      if (_data != NULL)
      {
        Rect_t area = {
            .x = 80, .y = 300, .width = 200, .height = 200};
        epd_draw_grayscale_image(area, (uint8_t *)_data);
        free(_data);
      }
      _data = load_file("url_img.bin");
      if (_data != NULL)
      {
        Rect_t area = {
            .x = 680, .y = 300, .width = 200, .height = 200};
        epd_draw_grayscale_image(area, (uint8_t *)_data);
        free(_data);
      }
      delay(5000);
      epd_poweroff_all();
    }
    else
    {
      if (param.test_data)
      {
        if (getWeather())
        {
          epd_poweron();
          epd_clear();
          display_info();
          display_weather();
          edp_update();
          delay(5000);
          epd_poweroff_all();
        }
      }
      else
      {
        if (start_WiFi() == WL_CONNECTED && setup_time() == true)
        {
          bool _wakeUp = false;
          if (wakeupHour > sleepHour)
            _wakeUp = (currentHour >= wakeupHour || currentHour <= sleepHour);
          else
            _wakeUp = (currentHour >= wakeupHour && currentHour <= sleepHour);
          if (_wakeUp)
          {
            byte _attempts = 1;
            bool _rxWeather = false;
            WiFiClient client; // wifi client object
            while (_rxWeather == false && _attempts <= 2)
            {
              if (_rxWeather == false)
                _rxWeather = getWeather();
              _attempts++;
            }
            if (_rxWeather)
            {
              stop_WiFi();
              epd_poweron();
              epd_clear();
              display_info();
              display_weather();
              edp_update();
              delay(5000);
              epd_poweroff_all();
            }
          }
        }
      }
      begin_sleep();
    }
  }
  else
  {
    log_i("SPIFFS begin failed");
  }
  if (!_settingsEn)
    begin_sleep();
}

void begin_sleep()
{
  epd_poweroff_all();
  update_local_time();
  sleepTimer = ((param.update_interval * sleepDuration * 60) - ((currentMin % sleepDuration) * 60 + currentSec)) + delta; // Some ESP32 have a RTC that is too fast to maintain accurate time, so add an offset
  esp_sleep_enable_timer_wakeup(sleepTimer * 1000000LL);                                                                  // in Secs, 1000000LL converts to Secs as unit = 1uSec
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0);                                                                           // 1 = High, 0 = Low
  log_i("Awake for: %d -secs", ((millis() - startTime) / 1000.0, 3));
  log_i("Entering %d (secs) of sleep time", sleepTimer);
  log_i("Starting deep-sleep period...");
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

void ap_config()
{
  WiFi.mode(WIFI_AP);
  log_i("Wi-Fi AP mode");
  log_i("AP configuring...");
  WiFi.softAP(AP_SSID, AP_PASS);
  log_i("done");
  IPAddress myIP = WiFi.softAPIP();
  log_i("AP IP address: %s", myIP.toString().c_str());
}

void display_weather()
{
  drawLine(0, 50, EPD_WIDTH, 50, Black);
  display_fact_weather();
  display_forecast_weather();
}

void display_info()
{
  setFont(OpenSans12B);
  drawString(10, 15, param.city, LEFT);
  drawString(400, 15, convert_unix_time(weather.now /* + (param.time_zone * 3600)*/), LEFT);
  draw_battery(680, 30);
  draw_RSSI(900, 35, wifi_signal);
}

String convert_unix_time(int unix_time)
{
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%H:%M %d.%m.%y", now_tm);
  return output;
}

void draw_battery(int x, int y)
{
  int vref = 1100;
  uint8_t _percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    log_i("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }
  float _voltage = analogRead(36) / 4096.0 * 6.566 * (vref / 1000.0);
  if (_voltage > 1)
  { // Only display if there is a valid reading
    log_i("Voltage = %.2f", _voltage);
    _percentage = 2836.9625 * pow(_voltage, 4) - 43987.4889 * pow(_voltage, 3) + 255233.8134 * pow(_voltage, 2) - 656689.7123 * _voltage + 632041.7303;
    if (_voltage >= 4.20)
      _percentage = 100;
    if (_voltage <= 3.20)
      _percentage = 0; // orig 3.5
    drawRect(x + 25, y - 14, 40, 15, Black);
    fillRect(x + 65, y - 10, 4, 7, Black);
    fillRect(x + 27, y - 12, 36 * _percentage / 100.0, 11, Black);
    drawString(x + 85, y - 14, String(_percentage) + "%  " + String(_voltage, 1) + "v", LEFT);
  }
}

void draw_RSSI(int x, int y, int rssi)
{
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20)
  {
    if (_rssi <= -20)
      WIFIsignal = 20; //            <-20dbm displays 5-bars
    if (_rssi <= -40)
      WIFIsignal = 16; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)
      WIFIsignal = 12; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)
      WIFIsignal = 8; //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100)
      WIFIsignal = 4; // -100dbm to  -81dbm displays 1-bar
    fillRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    xpos++;
  }
}

void display_fact_weather()
{
  draw_wind_section(800, 180, weather.fact.wind_dir, weather.fact.wind_speed, weather.fact.wind_gust, 100, true);
  draw_thp_section(480, 70);
  draw_conditions_section(20, 50, weather.fact.icon, 0, LargeIcon);
  draw_sun_section(370, 270);
  draw_moon_section(490, 180, param.hemisphere);
}

void draw_thp_section(uint16_t x, uint16_t y) // temperature, humidity, pressure section
{
  int xOffset = 150;
  setFont(OpenSans26B);
  drawString(x - xOffset / 2, y + 20, String(weather.fact.temp) + " °C", CENTER);
  setFont(OpenSans18B);
  drawString(x - xOffset / 2, y + 70, String(weather.fact.feels_like) + " °C", CENTER);

  setFont(OpenSans18B);
  drawString(x + xOffset / 2, y + 20, String(weather.fact.humidity) + " %", CENTER);
  setFont(OpenSans18B);
  drawString(x + xOffset / 2, y + 70, String(weather.fact.pressure_mm), CENTER);

  setFont(OpenSans8B);
  drawString(x + xOffset / 2, y + 100, "mm/Hg", CENTER);
}

void draw_sun_section(uint16_t x, uint16_t y)
{
  float x1, y1;
  int16_t r = 80;

  for (int a = 10; a <= 170; a++)
  {
    x1 = r * cos((a - 180.0) / 180.0 * PI) + x;
    y1 = r * sin((a - 180.0) / 180.0 * PI) + y;
    drawPixel(x1, y1, Black);
  }

  setFont(OpenSans10B);
  drawString(x - r, y + 10, weather.forecast.sunrise, CENTER);
  drawString(x + r, y + 10, weather.forecast.sunset, CENTER);

  uint8_t *data;
  data = load_file("sunrise.bin");
  if (data != NULL)
  {
    Rect_t area = {.x = x - r - 20, .y = y - 20, .width = 47, .height = 35};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }
  data = load_file("sunset.bin");
  if (data != NULL)
  {
    Rect_t area = {.x = x + r - 18, .y = y - 20, .width = 47, .height = 40};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }

  /*setFont(OpenSans12B);
  drawString(x, y, weather.forecast.sunrise, CENTER);
  drawString(x, y + 30, weather.forecast.sunset, CENTER);
  uint8_t *data;
  data = load_file("sunrise.bin");
  if (data != NULL)
  {
    Rect_t area = {.x = x + 50, .y = y - 15, .width = 47, .height = 35};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }
  data = load_file("sunset.bin");
  if (data != NULL)
  {
    Rect_t area = {.x = x + 50, .y = y + 15, .width = 47, .height = 40};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }*/
}

int JulianDate(int d, int m, int y)
{
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12)
    mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160)
    j = j - k3;
  return j;
}

double normalized_noon_phase(int d, int m, int y)
{
  int j = JulianDate(d, m, y);
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int)Phase);
}

void draw_moon_section(uint16_t x, uint16_t y, String hemisphere)
{
  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  const int diameter = 75;
  double Phase = normalized_noon_phase(day_utc, month_utc, year_utc);

  uint8_t *data;
  data = load_file("moon_new.bin");
  if (data != NULL)
  {
    Rect_t area = {.x = x + diameter / 2, .y = y + diameter / 2, .width = 75, .height = 75};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }

  hemisphere.toLowerCase();
  if (hemisphere == "south")
    Phase = 1 - Phase;
  // Draw dark part of moon
  fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, Black);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++)
  {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5)
    {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else
    {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    drawLine(pW1x, pW1y, pW2x, pW2y, White);
    drawLine(pW3x, pW3y, pW4x, pW4y, White);
  }
}

void draw_thp_forecasr_section(uint16_t x, uint16_t y, uint8_t part) // temperature, humidity, pressure section
{
  int xOffset = 90;
  setFont(OpenSans18B);
  drawString(x - xOffset / 2, y + 20, String(weather.forecast.parts[part].temp_avg) + " °C", CENTER);
  setFont(OpenSans12B);
  drawString(x - xOffset / 2, y + 50, String(weather.forecast.parts[part].feels_like) + " °C", CENTER);

  setFont(OpenSans8B);
  drawString(x - xOffset / 2, y + 65, String(weather.forecast.parts[part].temp_max) + " | " + String(weather.forecast.parts[part].temp_min), CENTER);

  setFont(OpenSans10B);
  drawString(x + xOffset / 2, y + 25, String(weather.forecast.parts[part].humidity) + " %", CENTER);
  setFont(OpenSans10B);
  drawString(x + xOffset / 2, y + 50, String(weather.forecast.parts[part].pressure_mm), CENTER);

  setFont(OpenSans8B);
  drawString(x + xOffset / 2, y + 63, "mm/Hg", CENTER);
}

uint8_t *load_file(String fileName)
{
  String _fileName = "/" + fileName;
  uint8_t *data;
  log_i("file name: %s", _fileName.c_str());
  if (SPIFFS.exists(_fileName))
  {
    log_i("file %s is exist", _fileName.c_str());
    File f = SPIFFS.open(_fileName, FILE_READ);
    int size = f.size();
    log_i("file size: %d", size);
    data = (uint8_t *)ps_calloc(sizeof(uint8_t), size);
    f.readBytes((char *)data, size);
    f.close();
    return data;
  }
  else
  {
    log_i("file not found");
    data = NULL;
    return data;
  }
};

void draw_conditions_section(int x, int y, String IconName, uint8_t forecast_part, bool IconSize)
{
  String fileName = "";
  fileName += IconName + ((IconSize == LargeIcon) ? ("L") : (""));
  fileName += ".bin";
  log_i("icon name: %s | file name: %s", IconName.c_str(), fileName.c_str());
  uint8_t *data = load_file(fileName);
  if (data != NULL)
  {
    Rect_t area = {
        .x = x,
        .y = y,
        .width = ((IconSize == LargeIcon) ? (L_SIZE) : (S_SIZE)),
        .height = ((IconSize == LargeIcon) ? (L_SIZE) : (S_SIZE))};
    epd_draw_grayscale_image(area, (uint8_t *)data);
    free(data);
  }

  if (IconSize == LargeIcon)
  {
    setFont(OpenSans12B);
    drawString(x + L_SIZE / 2, y + L_SIZE - 8, weather.fact.condition, CENTER);
  }
  else
  {
    setFont(OpenSans8B);
    drawString(x + S_SIZE / 2, y + S_SIZE - 12, weather.forecast.parts[forecast_part].condition, CENTER);
    uint8_t prec_prob = weather.forecast.parts[forecast_part].prec_prob;
    drawString(x + S_SIZE / 2, y + S_SIZE + 10, String(weather.forecast.parts[forecast_part].prec_mm, 1) + "mm", CENTER);
    drawString(x + S_SIZE / 2, y + S_SIZE + 26, String(prec_prob) + "%", CENTER);
  }
}

void display_forecast_weather()
{
  drawLine(0, 350, EPD_WIDTH, 350, Black);
  drawLine(480, 350, EPD_WIDTH / 2, EPD_HEIGHT, Black);
  int xOffSet = EPD_WIDTH / 2;
  for (uint8_t i = 0; i < 2; i++)
  {
    setFont(OpenSans10B);
    drawString(i * xOffSet + 10, 350, weather.forecast.parts[i].part_name, LEFT);
    draw_conditions_section(i * xOffSet + 10, 380, weather.forecast.parts[i].icon, i, SmallIcon);
    draw_wind_section((i * xOffSet) + (i + xOffSet - 90), 445,
                      weather.forecast.parts[i].wind_dir,
                      weather.forecast.parts[i].wind_speed,
                      weather.forecast.parts[i].wind_gust,
                      60, false);
    draw_thp_forecasr_section(i * xOffSet + 210, 380, i);
  }
}

int16_t get_wind_angle(String dir)
{
  if (dir == "nw")
    return 315;
  if (dir == "n")
    return 0;
  if (dir == "ne")
    return 45;
  if (dir == "e")
    return 90;
  if (dir == "se")
    return 135;
  if (dir == "s")
    return 180;
  if (dir == "sw")
    return 225;
  if (dir == "w")
    return 270;
  if (dir == "c")
    return -1;
}

void arrow(int x, int y, int asize, float aangle, int pwidth, int plength)
{
  float arr;
  if (aangle > 180.0)
    arr = aangle - 180.0;
  else
    arr = aangle + 180.0;

  float dx = (float)(asize - 10) * cos((arr - 90) * PI / 180) + x; // calculate X position
  float dy = (float)(asize - 10) * sin((arr - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = arr * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, Black);
}

void draw_wind_section(int x, int y, String dir, float speed, float gust, int Cradius, bool fact)
{
  if (fact)
  {
    if (dir != "c")
      arrow(x, y, Cradius - 22, get_wind_angle(dir), 16, 33);
  }
  else
  {
    if (dir != "c")
      arrow(x, y, Cradius - 10, get_wind_angle(dir), 8, 20);
  }
  setFont(OpenSans8B);
  int dxo, dyo, dxi, dyi;
  drawCircle(x, y, Cradius, Black, false);       // Draw compass circle
  drawCircle(x, y, Cradius + 1, Black, false);   // Draw compass circle
  drawCircle(x, y, Cradius + 2, Black, false);   // Draw compass circle
  drawCircle(x, y, Cradius * 0.7, Black, false); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5)
  {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)
      drawString(dxo + x + 15, dyo + y - 18, TXT_NE, CENTER);
    if (a == 135)
      drawString(dxo + x + 20, dyo + y - 2, TXT_SE, CENTER);
    if (a == 225)
      drawString(dxo + x - 20, dyo + y - 2, TXT_SW, CENTER);
    if (a == 315)
      drawString(dxo + x - 15, dyo + y - 18, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
  }
  drawString(x, y - Cradius - 20, TXT_N, CENTER);
  drawString(x, y + Cradius + 10, TXT_S, CENTER);
  drawString(x - Cradius - 15, y - 5, TXT_W, CENTER);
  drawString(x + Cradius + 10, y - 5, TXT_E, CENTER);

  if (fact)
  {
    setFont(OpenSans12B);
    String wind = dir;
    wind.toUpperCase();
    drawString(x, y - 55, wind, CENTER);
    setFont(OpenSans24B);
    drawString(x, y - 33, String(speed, 1), CENTER);
    setFont(OpenSans12B);
    drawString(x, y + 14, String(gust, 1), CENTER);
    setFont(OpenSans12B);
    drawString(x, y + 40, "m/s", CENTER);
  }
  else
  {
    setFont(OpenSans8B);
    String wind = dir;
    wind.toUpperCase();
    drawString(x, y - 35, wind, CENTER);
    setFont(OpenSans12B);
    drawString(x, y - 17, String(speed, 1), CENTER);
    setFont(OpenSans8B);
    drawString(x, y + 5, String(gust, 1), CENTER);
    setFont(OpenSans8B);
    drawString(x, y + 20, "m/s", CENTER);
  }
}

uint8_t start_WiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(param.ap_ssid.c_str(), param.ap_pass.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(500);
    WiFi.begin(param.ap_ssid.c_str(), param.ap_pass.c_str());
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    wifi_signal = WiFi.RSSI();
    log_i("WiFi connected at: %s", WiFi.localIP().toString().c_str());
  }
  else
    log_i("WiFi connection *** FAILED ***");
  return WiFi.status();
}

void stop_WiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  log_i("WiFi switched Off");
}

boolean setup_time()
{
  configTime((param.time_zone * 3600), 0, ntpServer, "time.nist.gov");
  delay(100);
  return update_local_time();
}

boolean update_local_time()
{
  struct tm timeinfo;
  char time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 5000))
  { // Wait for 5-sec for time to synchronise
    log_i("Failed to obtain time");
    return false;
  }
  currentHour = timeinfo.tm_hour;
  currentMin = timeinfo.tm_min;
  currentSec = timeinfo.tm_sec;
  sprintf(day_output, "%s, %02u %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
  strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo); // Creates: '@ 14:05:49'   and change from 30 to 8 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
  sprintf(time_output, "%s", update_time);
  return true;
}

#if PRINT_DATA
void print_weather()
{
  log_i("\nfact:");
  log_i(" condition: %s", weather.fact.condition.c_str());
  log_i(" daytime: %s", weather.fact.daytime.c_str());
  log_i(" feels_like: %d", weather.fact.feels_like);
  log_i(" humidity: %d", weather.fact.humidity);
  log_i(" icon: %s", weather.fact.icon.c_str());
  log_i(" obs_time: %s", weather.fact.obs_time);
  log_i(" polar: %d", weather.fact.polar);
  log_i(" pressure_mm: %d", weather.fact.pressure_mm);
  log_i(" pressure_pa: %d", weather.fact.pressure_pa);
  log_i(" season: %s", weather.fact.season.c_str());
  log_i(" temp: %d", weather.fact.temp);
  log_i(" temp_water: %d", weather.fact.temp_water);
  log_i(" wind_dir: %s", weather.fact.wind_dir.c_str());
  log_i(" wind_gust: %.1f", weather.fact.wind_gust);
  log_i(" wind_speed: %.1f", weather.fact.wind_speed);
  log_i("\nforecast:");
  log_i(" date: %s", weather.forecast.date.c_str());
  log_i(" date_ts: %d", weather.forecast.date_ts);
  log_i(" moon_code: %d", weather.forecast.moon_code);
  log_i(" moon_text: %s", weather.forecast.moon_text.c_str());
  for (uint8_t i = 0; i < 2; i++)
  {
    log_i("\n part[%d]:", i);
    log_i("condition: %s", weather.forecast.parts[i].condition.c_str());
    log_i("daytime: %s", weather.forecast.parts[i].daytime.c_str());
    log_i("feels_like: %d", weather.forecast.parts[i].feels_like);
    log_i("humidity: %d", weather.forecast.parts[i].humidity);
    log_i("icon: %s", weather.forecast.parts[i].icon.c_str());
    log_i("part_name: %s", weather.forecast.parts[i].part_name.c_str());
    log_i("polar: %d", weather.forecast.parts[i].polar);
    log_i("prec_mm: %.1f", weather.forecast.parts[i].prec_mm);
    log_i("prec_period: %d", weather.forecast.parts[i].prec_period);
    log_i("prec_prob: %d", weather.forecast.parts[i].prec_prob);
    log_i("pressure_mm: %d", weather.forecast.parts[i].pressure_mm);
    log_i("pressure_pa: %d", weather.forecast.parts[i].pressure_pa);
    log_i("temp_avg: %d", weather.forecast.parts[i].temp_avg);
    log_i("temp_max: %d", weather.forecast.parts[i].temp_max);
    log_i("temp_min: %d", weather.forecast.parts[i].temp_min);
    log_i("temp_water: %d", weather.forecast.parts[i].temp_water);
    log_i("wind_dir: %s", weather.forecast.parts[i].wind_dir.c_str());
    log_i("wind_gust: %.1f", weather.forecast.parts[i].wind_gust);
    log_i("wind_speed: %.1f", weather.forecast.parts[i].wind_speed);
  }
  log_i("sunrise: %s", weather.forecast.sunrise.c_str());
  log_i("sunset: %s", weather.forecast.sunset.c_str());
  log_i("week: %d", weather.forecast.week);
  log_i("\ninfo:");
  log_i("lat: %f", weather.info.lat);
  log_i("lon: %f", weather.info.lon);
  log_i("url: %s", weather.info.url.c_str());

  log_i("now: %d", weather.now);
  log_i("now_dt: %s", weather.now_dt.c_str());
}
#endif

bool decode_json(char *jsonStr, int size)
{
  log_i("weather data:");
  log_i("%s", jsonStr);
  DynamicJsonDocument jsonDoc(size);                              // allocate the JsonDocument
  DeserializationError error = deserializeJson(jsonDoc, jsonStr); // Deserialize the JSON document
  if (error)
  { // Test if parsing succeeds.
    log_i("deserializeJson() failed: %s", error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject jo = jsonDoc.as<JsonObject>();
  // read from JsonObject
  weather.fact.condition = jo["fact"]["condition"].as<char *>();
  weather.fact.daytime = jo["fact"]["daytime"].as<char *>();
  weather.fact.feels_like = jo["fact"]["feels_like"].as<int8_t>();
  weather.fact.humidity = jo["fact"]["humidity"].as<uint8_t>();
  weather.fact.icon = jo["fact"]["icon"].as<char *>();
  weather.fact.obs_time = jo["fact"]["obs_time"].as<int>();
  weather.fact.polar = jo["fact"]["polar"].as<bool>();
  weather.fact.pressure_mm = jo["fact"]["pressure_mm"].as<uint16_t>();
  weather.fact.pressure_pa = jo["fact"]["pressure_pa"].as<uint16_t>();
  weather.fact.season = jo["fact"]["season"].as<char *>();
  weather.fact.temp = jo["fact"]["temp"].as<int8_t>();
  weather.fact.temp_water = jo["fact"]["temp_water"].as<int8_t>();
  weather.fact.wind_dir = jo["fact"]["wind_dir"].as<char *>();
  weather.fact.wind_gust = jo["fact"]["wind_gust"].as<float>();
  weather.fact.wind_speed = jo["fact"]["wind_speed"].as<float>();

  weather.forecast.date = jo["forecast"]["date"].as<String>();
  weather.forecast.date_ts = jo["forecast"]["date_ts"].as<int>();
  weather.forecast.moon_code = jo["forecast"]["moon_code"].as<uint8_t>();
  weather.forecast.moon_text = jo["forecast"]["moon_text"].as<char *>();
  for (uint8_t i = 0; i < 2; i++)
  {
    weather.forecast.parts[i].condition = jo["forecast"]["parts"][i]["condition"].as<char *>();
    weather.forecast.parts[i].daytime = jo["forecast"]["parts"][i]["daytime"].as<char *>();
    weather.forecast.parts[i].feels_like = jo["forecast"]["parts"][i]["feels_like"].as<int8_t>();
    weather.forecast.parts[i].humidity = jo["forecast"]["parts"][i]["humidity"].as<uint8_t>();
    weather.forecast.parts[i].icon = jo["forecast"]["parts"][i]["icon"].as<char *>();
    weather.forecast.parts[i].part_name = jo["forecast"]["parts"][i]["part_name"].as<char *>();
    weather.forecast.parts[i].polar = jo["forecast"]["parts"][i]["polar"].as<bool>();
    weather.forecast.parts[i].prec_mm = jo["forecast"]["parts"][i]["prec_mm"].as<float>();
    weather.forecast.parts[i].prec_period = jo["forecast"]["parts"][i]["prec_period"].as<uint16_t>();
    weather.forecast.parts[i].prec_prob = jo["forecast"]["parts"][i]["prec_prob"].as<uint8_t>();
    weather.forecast.parts[i].pressure_mm = jo["forecast"]["parts"][i]["pressure_mm"].as<uint16_t>();
    weather.forecast.parts[i].pressure_pa = jo["forecast"]["parts"][i]["pressure_pa"].as<uint16_t>();
    weather.forecast.parts[i].temp_avg = jo["forecast"]["parts"][i]["temp_avg"].as<int8_t>();
    weather.forecast.parts[i].temp_max = jo["forecast"]["parts"][i]["temp_max"].as<int8_t>();
    weather.forecast.parts[i].temp_min = jo["forecast"]["parts"][i]["temp_min"].as<int8_t>();
    weather.forecast.parts[i].temp_water = jo["forecast"]["parts"][i]["temp_water"].as<uint8_t>();
    weather.forecast.parts[i].wind_dir = jo["forecast"]["parts"][i]["wind_dir"].as<char *>();
    weather.forecast.parts[i].wind_gust = jo["forecast"]["parts"][i]["wind_gust"].as<float>();
    weather.forecast.parts[i].wind_speed = jo["forecast"]["parts"][i]["wind_speed"].as<float>();
  }
  weather.forecast.sunrise = jo["forecast"]["sunrise"].as<char *>();
  weather.forecast.sunset = jo["forecast"]["sunset"].as<char *>();
  weather.forecast.week = jo["forecast"]["week"].as<uint8_t>();
  weather.info.lat = jo["info"]["lat"].as<float>();
  weather.info.lon = jo["info"]["lon"].as<float>();
  weather.info.url = jo["info"]["url"].as<char *>();
  weather.now = jo["now"].as<int>();
  weather.now_dt = jo["now_dt"].as<char *>();
#if PRINT_DATA
  print_weather();
#endif
  return true;
}

bool getWeather()
{
  int _size;
  if (param.test_data)
  {
    if (SPIFFS.exists("/test_data.json"))
    {
      File f = SPIFFS.open("/test_data.json", FILE_READ);
      _size = f.size();
      char _data[_size];
      f.readBytes(_data, _size);
      f.close();
      decode_json(_data, _size);
      return true;
    }
    else
    {
      log_i("test_data file not found");
      return false;
    }
  }
  else
  {
    // TODO Сделать запись в SPIFFS последних данных
    HTTPClient _http;
    String _host = "api.weather.yandex.ru";
    String _uri = "/v2/informers?lat=" + String(param.lat, 6) + "&lon=" + String(param.lon, 6);
    WiFiClient _client;
    _client.stop();

    _http.begin(_client, _host, 80, _uri, true);
    _http.addHeader("X-Yandex-API-Key", param.api_key);
    int _httpCode = _http.GET();

    if (_httpCode == HTTP_CODE_OK)
    {
      _size = _client.available();
      char _data[_size];
      _http.getStream().readBytes(_data, _size);
#if SAVE_LAST_DATA
      File f = SPIFFS.open("/test_data.json", FILE_WRITE);
      f.write((uint8_t *)_data, _size);
      f.close();
#endif
      decode_json(_data, _size);
      _client.stop();
      _http.end();
      return true;
    }
    else
    {
      log_i("connection failed, error[%d]: %s\n", _httpCode, _http.errorToString(_httpCode).c_str());
      _client.stop();
      _http.end();
      return false;
    }
  }
}

void drawString(int x, int y, String text, alignment align)
{
  char *data = const_cast<char *>(text.c_str());
  int x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  int w, h;
  int xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)
    x = x - w;
  if (align == CENTER)
    x = x - w / 2;
  int cursor_y = y + h;
  write_string(&currentFont, data, &x, &cursor_y, displayBuffer);
}

void fillCircle(int x, int y, int r, uint8_t color)
{
  epd_fill_circle(x, y, r, color, displayBuffer);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  epd_write_line(x0, y0, x1, y1, color, displayBuffer);
}

void drawCircle(int x0, int y0, int r, uint8_t color, bool fill)
{
  if (fill)
    epd_fill_circle(x0, y0, r, color, displayBuffer);
  else
    epd_draw_circle(x0, y0, r, color, displayBuffer);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  epd_draw_rect(x, y, w, h, color, displayBuffer);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  epd_fill_rect(x, y, w, h, color, displayBuffer);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, uint16_t color)
{
  epd_fill_triangle(x0, y0, x1, y1, x2, y2, color, displayBuffer);
}

void drawPixel(int x, int y, uint8_t color)
{
  epd_draw_pixel(x, y, color, displayBuffer);
}

void setFont(GFXfont const &font)
{
  currentFont = font;
}

void edp_update()
{
  epd_draw_grayscale_image(epd_full_screen(), displayBuffer); // Update the screen
}

void loop()
{
  vTaskDelete(NULL);
}