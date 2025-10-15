#ifndef LINFILEMONITOR_H
#define LINFILEMONITOR_H

#include "IFileMonitor.h"
#include <algorithm>  // Для std::find
#include <atomic>
#include <chrono>
#include <ctime>
#include <dlfcn.h>
#include <dirent.h>
#include <cerrno>
#include <iomanip>
#include <sstream>
#include <thread>  // Для thread_local
#include <string>  // Для std::string::find

#if defined(__linux__)

// Linux-специфичный монитор
class LinuxFileMonitor : public IFileMonitor {
private:
    // Оригинальные функции
    static DIR* (*original_opendir)(const char* name);
    static struct dirent* (*original_readdir)(DIR* dirp);

    // Режимы и данные
    bool logMode = false;
    bool hideMode = false;
    std::string logFunk;
    std::vector<std::string> hiddenFiles;
    MonitorClient* client = nullptr;

    // Защита от рекурсии
    static thread_local bool in_hook;

    // Проверка, является ли файл скрытым
    bool isHidden(const std::string& fileName) {
        return std::find(hiddenFiles.begin(), hiddenFiles.end(), fileName) != hiddenFiles.end();
    }

    static std::string get_simple_time() {
        static std::atomic<uint64_t> counter{ 0 };
        uint64_t count = counter.fetch_add(1, std::memory_order_relaxed);

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm lt{};
        localtime_r(&t, &lt);  // Thread-safe

        std::ostringstream oss;
        oss << "[" << std::setfill('0') << std::setw(2) << lt.tm_hour << ":"
            << std::setw(2) << lt.tm_min << ":" << std::setw(2) << lt.tm_sec
            << "#" << std::setw(4) << (count % 10000) << "]";
        return oss.str();
    }

    // Проверка на GTK-специфические пути (для исключения)
    bool isGtkStylePath(const std::string& path) {
        return path.find("gtksourceview") != std::string::npos || path.find("styles") != std::string::npos;
    }

    // Статический указатель на экземпляр
    static LinuxFileMonitor* instance;

public:
    LinuxFileMonitor() {
        instance = this;
    }

    // Хук-функции
    static DIR* MyOpendirHook(const char* name);
    static struct dirent* MyReaddirHook(DIR* dirp);

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
        // Всегда получаем оригиналы для переопределённых функций (чтобы избежать рекурсии)
        original_opendir = (DIR * (*)(const char*)) dlsym(RTLD_NEXT, "opendir");
        original_readdir = (struct dirent* (*)(DIR*)) dlsym(RTLD_NEXT, "readdir");

        if (!original_opendir || !original_readdir) {
            if (client) {
                std::string msg = "[ERROR] Failed to get original functions via dlsym";
                client->sendMsg(msg);
            }
            return false;
        }
        return true;
    }

    bool uninstallHooks() override {
        // На Linux с LD_PRELOAD снятие хуков не требуется
        return true;
    }
};

// Инициализация
LinuxFileMonitor* LinuxFileMonitor::instance = nullptr;
DIR* (*LinuxFileMonitor::original_opendir)(const char*) = nullptr;
struct dirent* (*LinuxFileMonitor::original_readdir)(DIR*) = nullptr;
thread_local bool LinuxFileMonitor::in_hook = false;

// Определения хук-функций
DIR* LinuxFileMonitor::MyOpendirHook(const char* name) {
    if (!instance || in_hook) {
        return original_opendir ? original_opendir(name) : nullptr;
    }

    std::string dirStr = name ? name : "[null]";
    if (dirStr.empty()) dirStr = "[empty]";

    // Исключение для GTK-путей
    if (instance->isGtkStylePath(dirStr)) {
        return original_opendir(name);
    }

    // Проверка необходимости хука (только для логирования opendir)
    bool need_log = instance->logMode && instance->logFunk == "opendir";
    if (!need_log) {
        return original_opendir(name);
    }

    in_hook = true;
    std::string timeStr = get_simple_time();
    std::ostringstream oss;
    oss << "[LOG] " << timeStr << " opendir: " << dirStr;
    std::string msg = oss.str();
    instance->client->sendMsg(msg);

    DIR* result = original_opendir(name);
    in_hook = false;
    return result;
}

struct dirent* LinuxFileMonitor::MyReaddirHook(DIR* dirp) {
    if (!instance || in_hook) {
        return original_readdir ? original_readdir(dirp) : nullptr;
    }

    // Проверка необходимости хука (логирование или скрытие)
    bool need_hide = instance->hideMode;
    bool need_log = instance->logMode && instance->logFunk == "readdir";
    if (!need_hide && !need_log) {
        return original_readdir(dirp);
    }

    in_hook = true;
    std::string timeStr = get_simple_time();
    struct dirent* ent;

    do {
        ent = original_readdir(dirp);
        if (ent && need_hide && instance->isHidden(ent->d_name)) {
            std::ostringstream oss;
            oss << "[HIDE] " << timeStr << " readdir, hiding: " << std::string(ent->d_name);
            std::string msg = oss.str();
            instance->client->sendMsg(msg);
        }
    } while (ent && need_hide && instance->isHidden(ent->d_name));

    if (need_log && ent) {
        std::ostringstream oss;
        oss << "[LOG] " << timeStr << " readdir: " << std::string(ent->d_name);
        std::string msg = oss.str();
        instance->client->sendMsg(msg);
    }

    in_hook = false;
    return ent;
}

// Для LD_PRELOAD: Переопределение функций
extern "C" {
    DIR* opendir(const char* name) {
        return LinuxFileMonitor::MyOpendirHook(name);
    }

    struct dirent* readdir(DIR* dirp) {
        return LinuxFileMonitor::MyReaddirHook(dirp);
    }
}

#endif

#endif // LINFILEMONITOR_H