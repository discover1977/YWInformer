#include "ftp_server.h"

static FTPServer *_ftp;
static void _task(void *param);

FTP_Server::FTP_Server()
{
    _ftp = new FTPServer();
}

bool FTP_Server::begin(const String &user, const String &pass)
{
    static bool res = false;
    _ftp->addUser(user, pass);
    res = _ftp->begin();
    if (res)
    {
        xTaskCreatePinnedToCore(_task, T_FTPSrv_NAME, T_FTPSrv_STACK, NULL, T_FTPSrv_PRIOR, &_th, T_FTPSrv_CPU);
    }
    return res;
}

void FTP_Server::addFilesystem(String name, fs::FS *Filesystem)
{
    _ftp->addFilesystem(name, Filesystem);
}

xTaskHandle FTP_Server::getHandle()
{
    return _th;
}

static void _task(void *param)
{
    for (;;)
    {
        _ftp->handle();
    }
}