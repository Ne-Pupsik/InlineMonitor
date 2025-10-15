#pragma once

#include <string>
#include <memory>

class Injector {
public:
	virtual ~Injector() = default;

	virtual bool injectLibrary(unsigned long pid, std::string procName) = 0;

	virtual unsigned long getProcessIdByName(std::string procName) = 0;
};

std::unique_ptr<Injector> buildInjector();