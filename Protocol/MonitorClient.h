#pragma once
#include "boost/asio.hpp"
#include "MonitorServer.h"

using namespace std;

class MonitorClient
{
public:
	MonitorClient();
	~MonitorClient();

	void connect();
	void sendMsg(string& msg);
	Config getConfig();

private:
	Config _config;
	
	boost::asio::io_context _io;
	unsigned short _port = 55555;
	unique_ptr <boost::asio::ip::tcp::socket> _socket;
};

