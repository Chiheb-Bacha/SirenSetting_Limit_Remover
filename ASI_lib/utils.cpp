#include <windows.h>
#include "pch.h"
#include "debug.h"
#include "utils.h"
#include <string>
#include <format>


DWORD WINAPI ExecElevRemoveCompatMode(LPVOID lpParam) {
    std::wstring exePath = GetExePath();
    
    bool success = true;
    
    for (HKEY scope : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        HKEY hKey = nullptr;

        // Registry key does not exist
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        // Check if there is a value with the name equal to the absolute path of the EXE
        DWORD type = 0, size = 0;
        bool found = RegQueryValueExW(hKey, exePath.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);
        RegCloseKey(hKey);

        if (!found) 
            continue;

        // Attempt to open key for modification, elevate if fails
        HKEY hKeyWrite = nullptr;
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_WRITE, &hKeyWrite) != ERROR_SUCCESS) {
            success = false;
            break;
        }

        // If user is in administrator group but exe is not currently elevated, the first check
        // above can succeed but actually deleting the value can fail
        LSTATUS status = RegDeleteValueW(hKeyWrite, exePath.c_str());
        RegCloseKey(hKeyWrite);
        if (status != ERROR_SUCCESS) {
            success = false;
            break;
        }
    }

    if (!success) {
        // Likely needs elevation to complete the operation
        // Need to launch an external process to elevate

        std::wstring cmdLine = std::format(
            L"/C echo Disabling compatibility mode for \"{0}\" & "
            L"reg DELETE \"HKLM\\{1}\" /v \"{0}\" /f & "
            L"reg DELETE \"HKCU\\{1}\" /v \"{0}\" /f"
            , exePath, COMPAT_REG_PATH);

        ShellExecute(NULL, TEXT("runas"), TEXT("cmd.exe"), cmdLine.c_str(), NULL, SW_SHOWMINNOACTIVE);
    }

    int result = MessageBox(
        NULL,
        TEXT(
            "Removed compatibility mode. Click OK to exit game, or Cancel to continue loading without SSLA. "
            "Re-launch game for changes to take effect."
        ),
        TEXT("SirenSetting_Limit_Adjuster.asi"),
        MB_ICONINFORMATION | MB_OKCANCEL | MB_SYSTEMMODAL
    );

    if (result == IDOK) {
        ExitProcess(1);
    }
    
    return 0;
}

bool CheckAndRemoveCompatibilityMode()
{
    std::wstring exePath = GetExePath();
    bool foundAny = false;
    // bool needsElevation = false;

    for (HKEY scope : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        HKEY hKey = nullptr;

        // Registry key does not exist, so compatibility mode can't be set
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        // Check if there is a value with the name equal to the absolute path of the EXE
        DWORD type = 0, size = 0;
        bool found = RegQueryValueExW(hKey, exePath.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);
        RegCloseKey(hKey);

        if (!found) continue;
        foundAny = true;
    }

    // Compatibility mode not set for user or machine, OK to continue
    if (!foundAny) return true;

    std::wstring msg = std::format(
        L"Windows compatibility mode is enabled for \"{}\".\n\n"
        L"SirenSetting_Limit_Adjuster.asi is not compatible with Compatibility Mode.\n\n"
        L"Do you want to automatically disable Compatibility Mode? Click Yes to disable "
        L"(may request admin permissions), click No to continue loading the game with "
        L"SSLA disabled, or click Cancel to exit."
        , exePath);

    int result = MessageBox(
        NULL,
        msg.c_str(),
        TEXT("SirenSetting_Limit_Adjuster.asi"),
        MB_ICONWARNING | MB_YESNOCANCEL | MB_SYSTEMMODAL
    );

    if (result == IDYES) {
        CreateThread(NULL, 0, ExecElevRemoveCompatMode, NULL, 0, NULL);
    }
    else if (result == IDCANCEL) {
        ExitProcess(1);
    }

    return false;
}