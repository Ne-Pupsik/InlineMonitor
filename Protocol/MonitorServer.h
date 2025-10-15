#pragma once
#include "boost/asio.hpp"
#include <string>
#include <vector>

using namespace std;

struct Config
{
	vector <string> fileName;
	vector <string> funkName;
};

class MonitorServer
{
public:
	MonitorServer(string mode, vector <string> fileName, vector <string> funkName);
	~MonitorServer() { closeServer(); }

	void waitForClient();
	void sendConfig();
	void recvMessages();

private:
	void closeServer();

	Config config;

	boost::asio::io_context _io;
	unsigned short _port = 55555;
	unique_ptr <boost::asio::ip::tcp::acceptor> _acceptor;
	unique_ptr <boost::asio::ip::tcp::socket> _socket;
	
};

