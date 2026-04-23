#include <windows.h>
#include <shellapi.h>
#include "pch.h"
#include "debug.h"
#include "utils.h"
#include <cstdarg>
#include <cstdio>
#include <strsafe.h>
#include <cassert>
#include <string>
#include <format>

#ifdef SSA_BETA

void logDebug(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vlog(fmt, args);
	va_end(args);
}

void flushLog(HANDLE log)
{
	FlushFileBuffers(log);
}

#else

void logDebug(const char* fmt, ...)
{
	return;
}

void flushLog(HANDLE log)
{
	return;
}

#endif



DWORD WINAPI ExecFixLogFilePermissions(LPVOID lpParam) {
	LPCTSTR logPath = (LPCTSTR)lpParam;
	std::wstring cmdLine = std::format(
		L"/C echo Fixing permissions on log file. Normal log should appear on next game run.>\"{0}\" && "
		L"icacls \"{0}\" /grant *S-1-5-11:M"
	, logPath);

	DWORD exitCode = TryExecuteCmd(cmdLine.c_str(), true);
	
	if (exitCode == EXIT_SUCCESS) {
		std::wstring msg = std::format(
			L"Permissions have been updated on:\n{}",
			logPath
		);
		MessageBox(NULL, msg.c_str(), L"SirenSetting_Limit_Adjuster.asi", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
	}
	else {
		std::wstring msg = std::format(
			L"Unable to fix permissions for:\n{}",
			logPath
		);
		MessageBox(NULL, msg.c_str(), L"SirenSetting_Limit_Adjuster.asi", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
	}
	
	return 0;
}

HANDLE file = INVALID_HANDLE_VALUE;
bool setup_attempted = false;

bool setup_log()
{
	if (setup_attempted) {
		return (file != INVALID_HANDLE_VALUE);
	}
	setup_attempted = true;
	
	TCHAR fullPath[MAX_PATH] = { 0 };
	DWORD pathLen = GetFullPathName(TEXT("SirenSettings.log"), MAX_PATH, fullPath, NULL);
	
	file = CreateFile(fullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		// Get error message and set a default if FormatMessage fails
		LPTSTR errorMessage = NULL;
		DWORD errorCode = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, 0, (LPTSTR)&errorMessage, 0, NULL);
		TCHAR defaultErrorMessage[] = TEXT("Unknown error.");
		bool usedDefaultErrorMessage = false;
		if (errorMessage == NULL) {
			errorMessage = defaultErrorMessage;
			usedDefaultErrorMessage = true;
		}

		std::wstring msg = std::format(
			L"SirenSetting_Limit_Adjuster log creation failed.\n\n"
			L"Error: {}\n\n"
			L"To attempt to fix log permissions automatically, click Yes. "
			L"This will request admin permissions and changes will take effect on the next game start. "
			L"To continue loading the game with logging disabled, click No. "
			L"To exit click Cancel.\n\n"
			L"Log file path: {}"
		, errorMessage, fullPath);

		int result = MessageBox(
			NULL,
			msg.c_str(),
			TEXT("SirenSetting_Limit_Adjuster.asi"),
			MB_ICONWARNING | MB_YESNOCANCEL | MB_SYSTEMMODAL
		);

		if (result == IDYES) {
			size_t allocChars = pathLen + 1;
			LPTSTR pathCopy = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, allocChars * sizeof(TCHAR));
			if (pathCopy) {
				StringCchCopy(pathCopy, allocChars, fullPath);
				CreateThread(NULL, 0, ExecFixLogFilePermissions, pathCopy, 0, NULL);
			}
		}
		else if (result == IDCANCEL) {
			ExitProcess(1);
		}

		if (!usedDefaultErrorMessage && errorMessage) LocalFree(errorMessage);
		return false;
	}
	return true;
}

void log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vlog(fmt, args);
	va_end(args);
}

void vlog(const char* fmt, va_list args)
{
	if (file == INVALID_HANDLE_VALUE || file == 0)
		if (!setup_log())
			return;
	if (file == INVALID_HANDLE_VALUE || file == 0)
		return;
	char outputString[128] = { 0 };
	char* outputStringLong = outputString;
	bool outputStrIsLong = false;
	va_list argcopy;
	va_copy(argcopy, args);
	SIZE_T outputLen = vsnprintf(outputString, 128, fmt, argcopy);
	va_end(argcopy);
	if (outputLen > 128)
	{
		outputStringLong = (char*)LocalAlloc(LMEM_ZEROINIT, outputLen + 1);
		outputStrIsLong = true;
		if (outputStringLong == NULL)
			return;
		outputLen++;
		vsnprintf(outputStringLong, outputLen, fmt, args);
	}
	WriteFile(file, outputStringLong, outputLen, NULL, NULL);
	if (outputStrIsLong)
		LocalFree(outputStringLong);
	flushLog(file);
}

void cleanup_log()
{
	if (file != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(file);
		CloseHandle(file);
	}
	return;
}