#ifndef FTP_SERVER_H_
#define FTP_SERVER_H_

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPIFFS.h>
#include <ESP-FTP-Server-Lib.h>

#define T_FTPSrv_CPU        0
#define T_FTPSrv_PRIOR      0
#define T_FTPSrv_STACK      16384
#define T_FTPSrv_NAME       "FTP server"

#define DEF_USER            "esp32"
#define DEF_PASS            "esp32"

class FTP_Server
{
public:
    FTP_Server();    
    bool begin(const String &user = DEF_USER, const String &pass = DEF_PASS);
    void addFilesystem(String name, fs::FS *Filesystem);
    xTaskHandle getHandle();

private:    
    xTaskHandle _th;
};

#endif /* FTP_SERVER_H_ */