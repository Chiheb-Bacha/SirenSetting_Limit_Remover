#pragma once
#include "pch.h"
#pragma comment(lib, "Version.lib")
#include "debug.h"
#include <cstdint>
#include <vector>
#include <string>

#pragma comment(lib, "shell32.lib")

struct FileVersion
{
    uint16_t Major = 0;
    uint16_t Minor = 0;
    uint16_t Build = 0;
    uint16_t Revision = 0;

    constexpr uint64_t ToComparable() const noexcept
    {
        return (uint64_t(Major) << 48) |
            (uint64_t(Minor) << 32) |
            (uint64_t(Build) << 16) |
            uint64_t(Revision);
    }

    friend constexpr bool operator<(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() < b.ToComparable();
    }

    friend constexpr bool operator>=(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() >= b.ToComparable();
    }
};

inline bool ReadCurrentProcessFileVersion(FileVersion& outVersion)
{
    wchar_t path[MAX_PATH]{};

    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return false;

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &dummy);
    if (size == 0)
        return false;

    std::vector<std::byte> buffer(size);

    if (!GetFileVersionInfoW(path, 0, size, buffer.data()))
        return false;

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;

    if (!VerQueryValueW(
        buffer.data(),
        L"\\",
        reinterpret_cast<LPVOID*>(&info),
        &infoSize))
    {
        return false;
    }

    outVersion.Major = HIWORD(info->dwFileVersionMS);
    outVersion.Minor = LOWORD(info->dwFileVersionMS);
    outVersion.Build = HIWORD(info->dwFileVersionLS);
    outVersion.Revision = LOWORD(info->dwFileVersionLS);

    return true;
}

inline const FileVersion& GetGameVersion()
{
    static const FileVersion& cachedVersion = []
        {
            FileVersion v{};
            if (ReadCurrentProcessFileVersion(v)) {
                return v;
            }
            log("Could not determine the game version. Terminating process...");
            ExitProcess(1);
        }();

    return cachedVersion;
}

// Will be used soon ... 
inline bool IsEnhanced() {
    static const bool isEnhanced = []() -> bool 
        {
            char path[MAX_PATH];
            GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);

            const char* filename = strrchr(path, '\\');
            filename = filename ? filename + 1 : path;

            return (_stricmp(filename, "GTA5_Enhanced.exe") == 0);
        }();
    return isEnhanced;
}

DWORD TryExecuteCmd(const wchar_t* cmdLine, bool elevate);

#pragma region Compatibility mode stuff

static const wchar_t* COMPAT_REG_PATH = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

inline std::wstring GetExePath()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

bool CheckAndRemoveCompatibilityMode();


#pragma endregion