#include "MonitorClient.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;
using boost::asio::ip::tcp;

MonitorClient::MonitorClient()
{
	_socket = make_unique<tcp::socket>(_io);
}

MonitorClient::~MonitorClient()
{
	if (_socket->is_open())
		_socket->close();
}

void MonitorClient::connect()
{
	tcp::resolver resolver(_io);
	auto endpoints = resolver.resolve("127.0.0.1", to_string(_port));
	
    boost::system::error_code ec;
    for (int i = 0; i < 20; ++i) {                // до ~2 секунд суммарно
        boost::asio::connect(*_socket, endpoints, ec);
        if (!ec) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    throw std::runtime_error(std::string("connect failed: ") + ec.message());
}

void MonitorClient::sendMsg(string& msg)
{
	string line = msg;
	if (line.empty() || line.back() != '\n') line.push_back('\n');
	boost::asio::write(*_socket, boost::asio::buffer(line.data(), line.size()));
}

static vector<string> split_ws(const string& s) {
    istringstream iss(s);
    vector<string> out;
    string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

Config MonitorClient::getConfig()
{
    if (!_socket || !_socket->is_open())
        throw runtime_error("socket is not connected");

    // 1) читаем ВЕСЬ конфиг до '\n' в одну строку confMsg
    boost::asio::streambuf buf;
    boost::asio::read_until(*_socket, buf, '\n');
    string confMsg;
    {
        istream is(&buf);
        getline(is, confMsg); // без '\n'
    }

    // 2) парсим confMsg формата: "<N> <fileLine> <M> <funkLine>"
    size_t i = 0, n = confMsg.size();

    auto expect_space = [&]() {
        if (i >= n || confMsg[i] != ' ')
            throw runtime_error("format error: expected space");
        ++i;
        };

    auto parse_len = [&]() -> size_t {
        if (i >= n || !isdigit(static_cast<unsigned char>(confMsg[i])))
            throw runtime_error("format error: expected length");
        size_t val = 0;
        while (i < n && isdigit(static_cast<unsigned char>(confMsg[i]))) {
            val = val * 10 + (confMsg[i] - '0');
            ++i;
        }
        if (i >= n) throw runtime_error("format error: unexpected end after length");
        return val;
        };

    // N
    size_t N = parse_len();
    expect_space();

    // fileLine
    if (i + N > n) throw runtime_error("format error: N out of range");
    string fileLine = confMsg.substr(i, N);
    i += N;

    expect_space();

    // M
    size_t M = parse_len();
    expect_space();

    // funkLine
    if (i + M > n) throw runtime_error("format error: M out of range");
    string funkLine = confMsg.substr(i, M);
    i += M;

    // 3) раскладываем в _config
    _config.fileName = split_ws(fileLine);
    _config.funkName = split_ws(funkLine);

    return _config;
}

