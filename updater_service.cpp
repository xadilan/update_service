#include "updater_service.h"
#include "namepipe_server.h"
#include "utils.h"
#include "xadl_utils.h"

#include <Shlobj.h>
#include <Windows.h>
#include <WtsApi32.h>
#include <fstream>
using tofstream = std::wofstream;


HANDLE gUpdaterProcess = NULL;
const TCHAR* kUpdateName = _T("AuditUpdate.exe");


CString GetFormattedTime() {
    SYSTEMTIME sysTime = { 0 };
    ::GetLocalTime(&sysTime);

    static CString message;
    message.Format(_T("%04d-%02d-%02d %2d:%2d:%2d "),
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

    return message;
}



void RunUpdater(std::wstring& installPath, tofstream& m_logFile) {
    std::wstring cmd = installPath;
    cmd += _T("\\");
    cmd += kUpdateName;
    if (!PathFileExists(cmd.c_str())) {
        m_logFile << GetFormattedTime().GetString() << cmd << " not exists" << std::endl;
        return;
    }

    // check update every 6 hours
    cmd += _T(" --interval_check 6");

    cmd += _T(" --log_path ");
    cmd += installPath;
    cmd += _T("\\update_log.log");

    cmd += _T(" --install_root ");
    cmd += installPath;

    LPTSTR szUpdaterCmdline = _tcsdup(cmd.c_str());
    m_logFile << GetFormattedTime().GetString() << "cmdline " << szUpdaterCmdline << std::endl;

    RunNewProcess(szUpdaterCmdline, installPath.c_str());
    free(szUpdaterCmdline);
}


DWORD WINAPI CheckAppUpdateEverySixHours(tofstream& m_logFile) {
    std::wstring installPath = GetAppInstallPath();
    if (!PathFileExists(installPath.c_str())) {
        m_logFile << GetFormattedTime().GetString() << "install path not exists" << std::endl;
        return false;
    }

    RunUpdater(installPath, m_logFile);

    return true;
}

void UpdaterService::OnStart(DWORD /*argc*/, TCHAR** /*argv[]*/) {
    m_logFile.close();
    TCHAR szPath[MAX_PATH] = { 0 };
    SHGetFolderPath(NULL, 
                    CSIDL_COMMON_APPDATA | CSIDL_FLAG_CREATE,
                    NULL,
                    0,
                    szPath);
    PathAppend(szPath, _T(PRODUCT_NAME "\\logs"));
    SHCreateDirectoryEx(NULL, szPath, NULL);
    CString logPath;
    logPath.Format(_T("%s\\update_service.log"), szPath);
    m_logFile.open(logPath, std::ios_base::app);

    if (!m_logFile.is_open()) {
        CString msg = _T("Can't open log file ");
        msg += logPath;
        WriteToEventLog(msg, EVENTLOG_ERROR_TYPE);
    }
    else {
        m_logFile << GetFormattedTime().GetString() << "started" << std::endl;
    }

     CreateThread(NULL, 0, StartNamePipeServer, NULL, 0, NULL);
    _tprintf(_T("started named pipe"));
     CheckAppUpdateEverySixHours(m_logFile);
#ifdef _DEBUG
    printf("service started\n");
#endif // _DEBUG
}

void UpdaterService::OnStop() {
    // Doesn't matter if it's open.
    if (m_logFile.is_open()) {
        m_logFile << GetFormattedTime().GetString() << "stopped" << std::endl;
    }
    m_logFile.close();
    if (gUpdaterProcess != NULL) {
        TerminateProcess(gUpdaterProcess, 0);
        m_logFile << GetFormattedTime().GetString() << " terminate " << std::endl;
    }
}