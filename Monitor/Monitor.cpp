#include "Monitor.h"
#include "MonitorServer.h"

Monitor::Monitor() : _pid(0), injector(buildInjector()) {}

void Monitor::run(int argc, char* argv[])
{	
	if (!parseArg(argc, argv)) return;
	
	if (!injector->injectLibrary(_pid, _name)) return;

	MonitorServer server("", this->_fileName, this->_funcName);

	server.waitForClient();
	server.sendConfig();
	server.recvMessages();
}

bool Monitor::parseArg(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--pid")	_pid = std::stol(argv[++i]);
		else if (arg == "--name") {
			_name = argv[++i];
			_pid = injector->getProcessIdByName(argv[i]);
		}
		else if (arg == "--func") _funcName.push_back(argv[++i]);
		else if (arg == "--hide") _fileName.push_back(argv[++i]);
		else {
			std::cerr << "[Monitor] Unknown or incomplete argument" << std::endl;
			return false;
		}
	}
	return true;
}
