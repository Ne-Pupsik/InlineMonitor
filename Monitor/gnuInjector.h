#include "injector.h"
#include <string>
#include <memory>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <fstream>
#include <errno.h>

class GnuInjector : public Injector {
public:
	GnuInjector();

	bool injectLibrary(unsigned long pid, std::string procName) override;
	unsigned long getProcessIdByName(std::string procName) override;

private:
	const std::string _soPath;

	bool isLibraryLoaded(pid_t pid, const std::string& soPath);
};