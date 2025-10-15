#include "winInjector.h"
#include <TlHelp32.h>
#include <iostream>

using namespace std;

std::unique_ptr<Injector> buildInjector()
{
	return std::make_unique<WinInjector>();
}

WinInjector::WinInjector() : _pid(0), _hProcess(nullptr), _dllPath(L"C:\\MyProgs\\HMonitor\\out\\build\\Observer.dll") {}

WinInjector::~WinInjector()
{
	if (_hProcess) CloseHandle(_hProcess);
}

bool WinInjector::injectLibrary(unsigned long pid, string procName)
{
	_pid = pid;

	if (!openTargetProcess())
		return false;

	LPVOID remoteBuf = allocateAndWriteMemory();
	if (!remoteBuf)	
		return false;

	bool result = createRemoteThread(remoteBuf);
	if (remoteBuf) VirtualFreeEx(_hProcess, remoteBuf, 0, MEM_RELEASE);
	
	return result;
}

unsigned long WinInjector::getProcessIdByName(string procName)
{
	unsigned long pid = 0;
	wstring procNameW(procName.begin(), procName.end());

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		cerr << "Failed to create process snapshot." << endl;
		return 0;
	}

	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(PROCESSENTRY32W);
	if (Process32FirstW(hSnapshot, &pe))
	{
		do
		{
			if (procNameW == pe.szExeFile)
			{
				pid = pe.th32ProcessID;
				break;
			}
		} while (Process32NextW(hSnapshot, &pe));
	}
	else
	{
		cerr << "Failed to get first process." << endl;
	}
	CloseHandle(hSnapshot);
	return pid;
}

bool WinInjector::openTargetProcess()
{
	_hProcess = OpenProcess(
		PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
		FALSE, _pid
	);

	if (!_hProcess) {
		cerr << "[!] OpenProcess failed." << endl;
		return false;
	}
	return true;
}

LPVOID WinInjector::allocateAndWriteMemory()
{
	SIZE_T size = (_dllPath.size() + 1) * sizeof(wchar_t);
	LPVOID remoteBuf = VirtualAllocEx(_hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (!remoteBuf) {
		cerr << "[!] VirtualAllocEx failed, error " << GetLastError() << endl;
		return nullptr;
	}

	if (!WriteProcessMemory(_hProcess, remoteBuf, _dllPath.c_str(), size, nullptr)) {
		cerr << "[!] WriteProcessMemory failed, error " << GetLastError() << endl;
		VirtualFreeEx(_hProcess, remoteBuf, 0, MEM_RELEASE);
		return nullptr;
	}

	return remoteBuf;
}

bool WinInjector::createRemoteThread(LPVOID remoteBuf)
{
	LPTHREAD_START_ROUTINE pLoadLibraryW =
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

	if (!pLoadLibraryW) {
		cerr << "[monitor] GetProcAddress failed" << endl;
		return false;
	}

	HANDLE hThread = CreateRemoteThread(_hProcess, nullptr, 0, pLoadLibraryW, remoteBuf, 0, nullptr);

	if (!hThread) {
		cerr << "[monitor] CreateRemoteThread failed, error " << GetLastError() << endl;
		return false;
	}

	WaitForSingleObject(hThread, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);

	if (exitCode == 0) {
		cerr << "[monitor] LoadLibraryW failed in target (return=0).\n";
		CloseHandle(hThread);
		return false;
	}

	cout << "[monitor] DLL injected, HMODULE=0x" << hex << exitCode << endl;
	CloseHandle(hThread);
	return true;
}
