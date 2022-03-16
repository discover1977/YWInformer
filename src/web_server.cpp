#include "web_server.h"
#include "param_data.h"

static WebServer *_server;
static FS *_filesystem;
static bool loadFromFS(String path);
static void hw_WebRequests();
static void hw_Website();
static void hw_param();
static String curDataToJSONStr();
static void _task(void *param);
static xTaskHandle _th;

Web_Server::Web_Server(int port)
{
    _server = new WebServer(port);
}

void Web_Server::begin(fs::FS *Filesystem)
{
    _filesystem = Filesystem;
    // Регистрация обработчиков
    _server->on(F("/"), hw_Website);
    _server->on(F("/param"), hw_param);
    _server->onNotFound(hw_WebRequests);
    ElegantOTA.begin(_server);
    _server->begin();
    xTaskCreatePinnedToCore(_task, T_WEBSrv_NAME, T_WEBSrv_STACK, NULL, T_WEBSrv_PRIOR, &_th, T_WEBSrv_CPU);
}

static void _task(void *param)
{
    for (;;)
    {
        _server->handleClient();
    }
}

static bool loadFromFS(String _path)
{
    String dataType = F("text/plain");
    if (_path.endsWith(F("/")))
        _path += F("index.html");

    if (_path.endsWith(F(".src")))
        _path = _path.substring(0, _path.lastIndexOf(F(".")));
    else if (_path.endsWith(F(".html")))
        dataType = F("text/html");
    else if (_path.endsWith(F(".htm")))
        dataType = F("text/html");
    else if (_path.endsWith(F(".css")))
        dataType = F("text/css");
    else if (_path.endsWith(F(".js")))
        dataType = F("application/javascript");
    else if (_path.endsWith(F(".png")))
        dataType = F("image/png");
    else if (_path.endsWith(F(".gif")))
        dataType = F("image/gif");
    else if (_path.endsWith(F(".jpg")))
        dataType = F("image/jpeg");
    else if (_path.endsWith(F(".ico")))
        dataType = F("image/x-icon");
    else if (_path.endsWith(F(".xml")))
        dataType = F("text/xml");
    else if (_path.endsWith(F(".pdf")))
        dataType = F("application/pdf");
    else if (_path.endsWith(F(".zip")))
        dataType = F("application/zip");
    File dataFile = _filesystem->open(_path.c_str(), "r");
    Serial.print(F("Load File: "));
    Serial.println(dataFile.name());
    if (_server->hasArg(F("download")))
        dataType = F("application/octet-stream");
    if (_server->streamFile(dataFile, dataType) != dataFile.size())
    {
    }
    dataFile.close();
    return true;
}

static void hw_WebRequests()
{
    Serial.println("h_WebRequests");
    // if(!server.authenticate(cch_web_user, cch_web_pass)) return server.requestAuthentication();
    if (loadFromFS(_server->uri()))
        return;
    Serial.println(F("File Not Detected"));
    String message = F("File Not Detected\n\n");
    message += F("URI: ");
    message += _server->uri();
    message += F("\nMethod: ");
    message += (_server->method() == HTTP_GET) ? F("GET") : F("POST");
    message += F("\nArguments: ");
    message += _server->args();
    message += "\n";
    for (uint8_t i = 0; i < _server->args(); i++)
    {
        message += " NAME:" + _server->argName(i) + "\n VALUE:" + _server->arg(i) + "\n";
    }
    _server->send(404, F("text/plain"), message);
    Serial.println(message);
}

static void hw_Website()
{
    Serial.println("h_Website");
    // if(!_server.authenticate(cch_web_user, cch_web_pass)) return server.requestAuthentication();
    _server->sendHeader(F("Location"), F("/index.html"), true); // Redirect to our html web page
    _server->send(302, F("text/plane"), "");
}

static void hw_param()
{
    log_i("server get param");
    log_i("Server has: %d argument(s):", _server->args());
    for (int i = 0; i < _server->args(); i++)
    {
        log_i("arg name: %s, value: %s", _server->argName(i).c_str(), _server->arg(i).c_str());
    }

    param_t _param;
    _param.ap_ssid = _server->arg(F("ap_ssid"));
    _param.ap_pass = _server->arg(F("ap_pass"));
    _param.city = _server->arg(F("city"));
    _param.lat = _server->arg(F("lat")).toFloat();
    _param.lon = _server->arg(F("lon")).toFloat();
    _param.time_zone = _server->arg(F("time_zone")).toInt();
    _param.update_interval = _server->arg(F("update_interval")).toInt();
    _param.api_key = _server->arg(F("api_key"));

    if (_server->hasArg(F("test_data")))
        _param.test_data = true;
    else
        _param.test_data = false;

    log_i("\tcity: %s", _param.city.c_str());
    log_i("\tlat: %s", String(_param.lat, 6).c_str());
    log_i("\tlon: %s", String(_param.lon, 6).c_str());
    log_i("\ttest_data: %d", _param.test_data);
    log_i("\tapi_key: %s", _param.api_key.c_str());
    log_i("\tupdate_interval: %d", _param.update_interval);
    log_i("\ttime_zone: %d", _param.time_zone);
    log_i("\tap_ssid: %s", _param.ap_ssid.c_str());
    log_i("\tap_pass: %s", _param.ap_pass.c_str());

    if (SPIFFS.exists("/param.json")) // Проверили наличие файла
    {
        File f;
        f = SPIFFS.open("/param.json", FILE_READ); // Открыли для чтения
        int size = f.size();
        char str[size];
        f.readBytes(str, size);
        str[size] = 0;
        log_i("\nfile size: %d\nparam.json content:\n%s", size, str);
        DynamicJsonDocument jsonDoc(size * 2);                      // Создали JSON-документ
        DeserializationError error = deserializeJson(jsonDoc, str); // Читаем содержимое файла в JSON-документ
        f.close();                                                  // Закрыли файл
        if (error)
            log_i("deserializeJson() failed: %s", error.c_str()); // Ошибка десериализации
        else
        {
            log_i("deserializeJson() success!");      // Сериализация выполнена успешно!
            JsonObject jo = jsonDoc.as<JsonObject>(); // Получаем JSON-объект

            if (_server->arg(F("ap_ssid")) != "")
                jsonDoc["ap_ssid"] = _param.ap_ssid;
            if (_server->arg(F("ap_pass")) != "")
                jsonDoc["ap_pass"] = _param.ap_pass;
            if (_server->arg(F("city")) != "")
                jsonDoc["city"] = _param.city;
            if (_server->arg(F("lat")) != "")
                jsonDoc["lat"] = _param.lat;
            if (_server->arg(F("lon")) != "")
                jsonDoc["lon"] = _param.lon;
            if (_server->arg(F("time_zone")) != "")
                jsonDoc["time_zone"] = _param.time_zone;
            if (_server->arg(F("update_interval")) != "")
                jsonDoc["update_interval"] = _param.update_interval;
            if (_server->arg(F("api_key")) != "")
                jsonDoc["api_key"] = _param.api_key;
            jsonDoc["test_data"] = _param.test_data;

            f = SPIFFS.open("/param.json", FILE_WRITE); // Открыли для записи
            serializeJson(jsonDoc, f);                  // Сериализовали JSON-документ в файл
            f.close();                                  // Закрыли файл
        }
    }
    else
        log_i("Param file not found!");

    _server->send(200, F("text/html"), F("Setting is updated, module will be rebooting..."));

    delay(100);
    WiFi.softAPdisconnect(true);
    delay(100);

    log_d("Resetting ESP...");
    ESP.restart();
}
