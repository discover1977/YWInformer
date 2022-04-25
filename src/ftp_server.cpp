#include "ftp_server.h"

static FtpServer *_ftp;
static void _task(void *param);

FTP_Server::FTP_Server()
{
    _ftp = new FtpServer();
}

bool FTP_Server::begin(const String &user, const String &pass)
{
    static bool res = false;
    _ftp->begin(user, pass);
    xTaskCreatePinnedToCore(_task, T_FTPSrv_NAME, T_FTPSrv_STACK, NULL, T_FTPSrv_PRIOR, &_th, T_FTPSrv_CPU);
}

void FTP_Server::addFilesystem(String name, fs::FS *Filesystem)
{
    // _ftp->addFilesystem(name, Filesystem);
}

xTaskHandle FTP_Server::getHandle()
{
    return _th;
}

static void _task(void *param)
{
    for (;;)
    {
        _ftp->handleFTP();
    }
}