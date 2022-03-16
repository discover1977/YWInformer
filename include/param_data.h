#ifndef PARAM_DATA_H_
#define PARAM_DATA_H_

#include <Arduino.h>

typedef struct
{
  String city;
  float lat;
  float lon;
  bool test_data;
  String api_key;
  uint8_t update_interval;
  int8_t time_zone;
  String ap_ssid;
  String ap_pass;
} param_t;

#endif /* ifndef PARAM_DATA_H_ */
