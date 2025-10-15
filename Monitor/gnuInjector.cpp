#include "gnuInjector.h"
#include <cstdlib>     // Для system
#include <cstdio>      // Для popen, pclose
#include <fstream>     // Для std::ifstream
#include <iostream>    // Для std::cerr
#include <string>
#include <thread>      // Для sleep

GnuInjector::GnuInjector() : _soPath("/home/user/.vs/HMonitor/out/build/libObserver.so") {}

bool GnuInjector::injectLibrary(unsigned long pid, std::string procName) {
    
    std::string cmd = "LD_PRELOAD=" + _soPath + " " + procName + " &";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[ERROR] system failed: " << ret << std::endl;
        return false;
    }

    //sleep(1);

    /*pid_t child = static_cast<pid_t>(getProcessIdByName(procName));
    if (child == 0) {
        std::cerr << "[ERROR] Cannot find new process PID." << std::endl;
        return false;
    }*/

    /*if (!isLibraryLoaded(child, _soPath)) {
        std::cerr << "[ERROR] Library " << _soPath << " not loaded in PID: " << child << std::endl;
        return false;
    }*/

    std::cout << "[INFO] SO injected in PID: "  << std::endl;
    return true;
}

unsigned long GnuInjector::getProcessIdByName(std::string procName) {
    std::string cmd = "pgrep -f " + procName;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return 0;

    unsigned long pid = 0;
    if (fscanf(fp, "%lu", &pid) != 1) pid = 0;
    pclose(fp);
    return pid;
}

bool GnuInjector::isLibraryLoaded(pid_t pid, const std::string& soPath) {
    std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream mapsFile(mapsPath);
    if (!mapsFile.is_open()) {
        std::cerr << "[ERROR] Cannot open " << mapsPath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(mapsFile, line)) {
        if (line.find(soPath) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<Injector> buildInjector() {
    return std::make_unique<GnuInjector>();
}