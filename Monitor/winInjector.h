#pragma once

#include "injector.h"
#include <windows.h>
#include <string>

class WinInjector : public Injector {
public:
	WinInjector();
	~WinInjector();

	bool injectLibrary(unsigned long pid, std::string procName) override;
	unsigned long getProcessIdByName(std::string procName) override;

private:
	bool openTargetProcess();
	LPVOID allocateAndWriteMemory();
	bool createRemoteThread(LPVOID remoteBuf);

	const std::wstring _dllPath;
	unsigned long _pid;
	HANDLE _hProcess;
};