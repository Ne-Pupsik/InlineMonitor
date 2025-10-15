// windowsfilemonitor.cpp

#include "IFileMonitor.h"
#include "inlineWinhook.h"
#include <stringapiset.h>
#include <algorithm>  
#include <chrono>

#if defined(_WIN32) || defined(_WIN64)

// Windows-специфичный монитор
class WindowsFileMonitor : public IFileMonitor {
private:
    // Сигнатуры Windows функций
    using CloseHandleSig = BOOL(WINAPI*)(HANDLE);
    using CreateFileSig = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    using FindFirstFileSig = HANDLE(WINAPI*)(LPCWSTR, LPWIN32_FIND_DATAW);
    using FindNextFileSig = BOOL(WINAPI*)(HANDLE, LPWIN32_FIND_DATAW);


    // Хуки
    std::unique_ptr<InlineHook<CloseHandleSig>> closeHook;
    std::unique_ptr<InlineHook<CreateFileSig>> createHook;
    std::unique_ptr<InlineHook<FindFirstFileSig>> findFirstHook;
    std::unique_ptr<InlineHook<FindNextFileSig>> findNextHook;

    // Режимы и данные
    bool logMode = false;
    bool hideMode = false;
    std::string logFunk;
    std::vector<std::string> hiddenFiles;
    MonitorClient* client = nullptr;

    typedef int (WINAPI* WideCharToMultiBytes_t)(
        UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

    static std::string to_utf8(const wchar_t* ws) {
        if (!ws) return "[null]";

        // Быстрая конвертация только для ASCII символов
        std::string result;
        for (const wchar_t* ptr = ws; *ptr; ++ptr) {
            wchar_t ch = *ptr;
            if (ch < 128) {
                result += static_cast<char>(ch);
            }
            else {
                // Для не-ASCII символов используем замену
                result += '?';
            }
        }

        return result.empty() ? "[empty]" : result;
    }

    // Проверка, является ли файл скрытым
    bool isHidden(const std::string& fileName) {
        return std::find(hiddenFiles.begin(), hiddenFiles.end(), fileName) != hiddenFiles.end();
    }

    static std::string get_simple_time() {
        static std::atomic<uint64_t> counter{ 0 };
        uint64_t count = counter.fetch_add(1, std::memory_order_relaxed);

        // Используем только системные вызовы
        SYSTEMTIME st;
        GetLocalTime(&st);

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d#%04llu]",
            st.wHour, st.wMinute, st.wSecond, count % 10000);
        return buffer;
    }

    static BOOL WINAPI MyCloseHandleHook(HANDLE hObject) {
        std::string timeStr = get_simple_time();
        
        instance->client->sendMsg("[LOG] " + timeStr + " CloseHandle: " + std::to_string((uintptr_t)hObject));

        auto original = instance->closeHook->GetOriginal();
        return original(hObject);
    }

    static HANDLE WINAPI MyCreateFileHook(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
        std::string fileStr = to_utf8(lpFileName);
        
        std::string timeStr = get_simple_time();

        if (instance->logMode) {
           instance->client->sendMsg("[LOG]" + timeStr + "CreateFileA: " + fileStr);
        }
        if (instance->hideMode && instance->isHidden(fileStr)) {
            instance->client->sendMsg("[HIDE] " + timeStr + " CreateFileW, hiding worked.");
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        auto original = instance->createHook->GetOriginal();
        return original(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    static HANDLE WINAPI MyFindFirstFileHook(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
        std::string pattern = to_utf8(lpFileName);

        std::string timeStr = get_simple_time();

        if (instance->logMode) {
            instance->client->sendMsg("[LOG] " + timeStr + " FindFirstFile: " + pattern);
        }
        auto original = instance->findFirstHook->GetOriginal();
        HANDLE hFind = original(lpFileName, lpFindFileData);
        if (instance->hideMode && hFind != INVALID_HANDLE_VALUE) {
            while (instance->isHidden(to_utf8(lpFindFileData->cFileName))) {
                if (!FindNextFileW(hFind, lpFindFileData)) {
                    FindClose(hFind);
                    instance->client->sendMsg("[HIDE] " + timeStr + " FindFirstFile, hiding worked.");
                    return INVALID_HANDLE_VALUE;
                }
            }
        }
        return hFind;
    }

    static BOOL WINAPI MyFindNextFileHook(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
        std::string timeStr = get_simple_time();
        
        if (instance->logMode) {
            instance->client->sendMsg("[LOG]" + timeStr + "FindNextFile: " + std::to_string((uintptr_t)hFindFile));
        }
        auto original = instance->findNextHook->GetOriginal();
        BOOL result;
        do {
            result = original(hFindFile, lpFindFileData);
        } while (result && instance->hideMode && instance->isHidden(to_utf8(lpFindFileData->cFileName)));
        return result;
    }

    // Статический указатель на экземпляр (для доступа в статических хуках)
    static WindowsFileMonitor* instance;

public:
    WindowsFileMonitor() {
        instance = this;
    }

    void configure(const Config& config, MonitorClient* cl) override {
        client = cl;
        if (!config.funkName.empty()) {
            logMode = true;
            logFunk = config.funkName[0];  // Только одна функция
        }
        else if (!config.fileName.empty()) {
            hideMode = true;
            hiddenFiles = config.fileName;
        }
    }

    bool installHooks() override {
        HMODULE kernel32 = GetModuleHandleA("kernelbase.dll");
        if (!kernel32) return false;

        auto closeHandle = reinterpret_cast<void*>(GetProcAddress(kernel32, "CloseHandle"));
        auto createFile = reinterpret_cast<void*>(GetProcAddress(kernel32, "CreateFileW"));
        auto findFirst = reinterpret_cast<void*>(GetProcAddress(kernel32, "FindFirstFileW"));
        auto findNext = reinterpret_cast<void*>(GetProcAddress(kernel32, "FindNextFileW"));

        closeHook = std::make_unique<InlineHook<CloseHandleSig>>(closeHandle, MyCloseHandleHook);
        createHook = std::make_unique<InlineHook<CreateFileSig>>(createFile, MyCreateFileHook);
        findFirstHook = std::make_unique<InlineHook<FindFirstFileSig>>(findFirst, MyFindFirstFileHook);
        findNextHook = std::make_unique<InlineHook<FindNextFileSig>>(findNext, MyFindNextFileHook);

        if (logMode) {
            if (logFunk == "CloseHandle") return(closeHook->Install());
            else if (logFunk == "CreateFile") return(createHook->Install());
            else if (logFunk == "FindFirstFile") return(findFirstHook->Install());
            else if (logFunk == "FindNextFile") return(findNextHook->Install());
        }
        else if (hideMode) {
            return createHook->Install() && findFirstHook->Install() && findNextHook->Install();
        }
    }

    bool uninstallHooks() override {
        return closeHook->Uninstall() && createHook->Uninstall() && findFirstHook->Uninstall() && findNextHook->Uninstall();
    }
};

WindowsFileMonitor* WindowsFileMonitor::instance = nullptr;

#endif