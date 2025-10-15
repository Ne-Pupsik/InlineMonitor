#include "MonitorClient.h"
#include "IFileMonitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

#ifdef _WIN32
#include "WinFileMonitor.h"
#include <windows.h>
#else
#include "LinFileMonitor.h"
#endif

class Worker {
private:
    std::unique_ptr<IFileMonitor> observer;
    MonitorClient client;
    std::atomic<bool> running{ false };

public:
    Worker() {
#ifdef _WIN32
        observer = std::make_unique<WindowsFileMonitor>();
#else
        observer = std::make_unique<LinuxFileMonitor>();
#endif
    }

    void run() {
        running = true;

        // Даем время инициализироваться основному процессу
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        client.connect();

        Config config = client.getConfig();
        observer->configure(config, &client);

        if (!observer->installHooks()) {
            std::string msg = "[HOOK] Failed to install hooks.";
            client.sendMsg(msg);
        }
        else {
            std::string msg = "[HOOK] hooks installed.";
            client.sendMsg(msg);
        }
                
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stop() {
        running = false;
    }
};

// Глобальный указатель для управления временем жизни
std::unique_ptr<Worker> g_worker;
std::unique_ptr<std::thread> g_workerThread;

// Функция инициализации библиотеки
void init_library() {
    // Создаем объекты глобально
    g_worker = std::make_unique<Worker>();
    g_workerThread = std::make_unique<std::thread>([]() {
        g_worker->run();
        });
}

// Функция деинициализации
void cleanup_library() {
    if (g_worker) {
        g_worker->stop();
    }
    if (g_workerThread && g_workerThread->joinable()) {
        g_workerThread->join();
    }
    g_worker.reset();
    g_workerThread.reset();
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // Отключаем вызовы DLL_THREAD_ATTACH/DETACH для оптимизации
        DisableThreadLibraryCalls(hinstDLL);

        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            init_library();
            }).detach();
        break;

    case DLL_PROCESS_DETACH:
        cleanup_library();
        break;
    }
    return TRUE;
}
#else
__attribute__((constructor))
void init_library_wrapper() {
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        init_library();
        }).detach();
}

__attribute__((destructor))
void cleanup_library_wrapper() {
    cleanup_library();
}
#endif