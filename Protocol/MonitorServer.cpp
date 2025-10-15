#include "MonitorServer.h"
#include <iostream>

using namespace std;
using boost::asio::ip::tcp;

MonitorServer::MonitorServer(string mode, vector<string> fileName, vector<string> funkName)
{
	this->config.fileName = fileName;
	this->config.funkName = funkName;
	
	tcp::endpoint ep(boost::asio::ip::address_v4::loopback(), _port);
	_acceptor = make_unique<tcp::acceptor>(_io, ep);

	boost::system::error_code ec;
	_acceptor->set_option(boost::asio::socket_base::reuse_address(true), ec);

	_socket = make_unique<tcp::socket>(_io);
	cout << "[monitor] Listen on 127.0.0.1:55555" << endl;
}

void MonitorServer::waitForClient()
{
	cout << "[monitor] Waiting for client ..." << endl;
	_acceptor->accept(*_socket);
	cout << "[monitor] Client connected." << endl;
}

void MonitorServer::sendConfig()
{
	// Формат строки:
	// N<fileLine>M<funkLine>
	// N и M - длины строк fileLine и funkLine
	using boost::asio::buffer;
	using boost::asio::write;

	string fileLine;
	for (size_t i = 0; i < config.fileName.size(); i++) {
		if (i) fileLine.push_back(' ');
		fileLine += config.fileName[i];
	}

	string funkLine;
	for (size_t i = 0; i < config.funkName.size(); i++) {
		if (i) funkLine.push_back(' ');
		funkLine += config.funkName[i];
	}

	string msg = to_string(fileLine.size()) + ' ' + fileLine + ' ' + to_string(funkLine.size()) + ' ' + funkLine + '\n';
	write(*_socket, buffer(msg.data(), msg.size()));
}

void MonitorServer::recvMessages()
{
	try {
		boost::asio::streambuf buf;
		while (true) {
			size_t n = boost::asio::read_until(*_socket, buf, '\n'); // блокируется до '\n'
			istream is(&buf);
			string line;
			getline(is, line); // убирает '\n'
			cout << line << endl;
		}
	}
	catch (const exception& e) {
		cout << "[monitor] recv loop end: " << e.what() << endl;
	}
}

void MonitorServer::closeServer()
{
    boost::system::error_code ec;
    if (_socket && _socket->is_open()) _socket->close(ec);
    if (_acceptor && _acceptor->is_open()) _acceptor->close(ec);
}