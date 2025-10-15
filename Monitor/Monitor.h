#pragma once

#include "injector.h"
#include <iostream>
#include <vector>
#include <memory>

class Monitor {
public:
	Monitor();
	~Monitor() {}

	void run(int argc, char* argv[]);

private:
	unsigned long _pid;
	std::string _name;
	std::vector <std::string> _fileName;
	std::vector <std::string> _funcName;
	
	std::unique_ptr<Injector> injector;
	bool parseArg(int argc, char** argv);
};