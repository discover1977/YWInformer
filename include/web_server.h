#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

#define T_WEBSrv_CPU 1
#define T_WEBSrv_PRIOR 0
#define T_WEBSrv_STACK 8192
#define T_WEBSrv_NAME "WEB server"

class Web_Server
{
public:
    Web_Server(int port = 80);
    void begin(fs::FS *Filesystem);
private:
};

#endif /* WEB_SERVER_H_ */