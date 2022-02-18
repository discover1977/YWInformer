#ifndef FACT_WEATHER_H_
#define FACT_WEATHER_H_

#include <Arduino.h>

typedef struct
{
    String condition;
    String daytime;
    int8_t feels_like;
    uint8_t humidity;
    String icon;
    int obs_time;
    bool polar;
    uint16_t pressure_mm;
    uint16_t pressure_pa;
    String season;
    int8_t temp;
    int8_t temp_water;
    String wind_dir;
    float wind_gust;
    float wind_speed;
} fact_weather_t;

typedef struct
{
    String condition;
    String daytime;
    int8_t feels_like;
    uint8_t humidity;
    String icon;
    String part_name;
    bool polar;
    float prec_mm;
    uint16_t prec_period;
    uint8_t prec_prob;
    uint16_t pressure_mm;
    uint16_t pressure_pa;
    int8_t temp_avg;
    int8_t temp_max;
    int8_t temp_min;
    int8_t temp_water;
    String wind_dir;
    float wind_gust;
    float wind_speed;
} forecast_part_t;

typedef struct
{
    String date;
    int date_ts;
    uint8_t moon_code;
    String moon_text;
    forecast_part_t parts[2];
    String sunrise;
    String sunset;
    uint16_t week;
} forecast_weather_t;

typedef struct
{
    float lat;
    float lon;
    String url;
} info_t;

typedef struct
{
    fact_weather_t fact;
    forecast_weather_t forecast;
    info_t info;
    int now;
    String now_dt;
} weather_t;

/* Answer example */
/*
{
    "fact": {
        "condition": "partly-cloudy",
        "daytime": "d",
        "feels_like": -17,
        "humidity": 85,
        "icon": "skc_d",
        "obs_time": 1644134400,
        "polar": false,
        "pressure_mm": 744,
        "pressure_pa": 991,
        "season": "winter",
        "temp": -12,
        "temp_water": 0,
        "wind_dir": "s",
        "wind_gust": 8.5,
        "wind_speed": 2
    },
    "forecast": {
        "date": "2022-02-06",
        "date_ts": 1644094800,
        "moon_code": 11,
        "moon_text": "moon-code-11",
        "parts": [
            {
                "condition": "light-snow",
                "daytime": "d",
                "feels_like": -17,
                "humidity": 87,
                "icon": "bkn_-sn_d",
                "part_name": "day",
                "polar": false,
                "prec_mm": 0.3,
                "prec_period": 360,
                "prec_prob": 40,
                "pressure_mm": 744,
                "pressure_pa": 991,
                "temp_avg": -11,
                "temp_max": -10,
                "temp_min": -12,
                "temp_water": 0,
                "wind_dir": "se",
                "wind_gust": 9.4,
                "wind_speed": 4.6
            },
            {
                "condition": "light-snow",
                "daytime": "n",
                "feels_like": -15,
                "humidity": 88,
                "icon": "ovc_-sn",
                "part_name": "evening",
                "polar": false,
                "prec_mm": 0.6,
                "prec_period": 360,
                "prec_prob": 40,
                "pressure_mm": 743,
                "pressure_pa": 990,
                "temp_avg": -9,
                "temp_max": -9,
                "temp_min": -10,
                "temp_water": 0,
                "wind_dir": "se",
                "wind_gust": 8.6,
                "wind_speed": 4
            }
        ],
        "sunrise": "08:27",
        "sunset": "16:57",
        "week": 5
    },
    "info": {
        "lat": 59.193938,
        "lon": 37.948628,
        "url": "https://yandex.ru/pogoda/29383?lat=59.193938&lon=37.948628"
    },
    "now": 1644137072,
    "now_dt": "2022-02-06T08:44:32.633810Z"
}
*/

#endif /* ifndef FACT_WEATHER_H_ */
