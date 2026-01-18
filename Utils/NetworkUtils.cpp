#include "pch.h"
#include "NetworkUtils.h"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <nlohmann/json.hpp>

namespace NetworkUtils {

	std::string ExtractIpFromHostPort(const std::string &hp) {
		if (hp.empty())
			return hp;

		// IPv6: [ip]:port
		if (hp[0] == '[') {
			auto end = hp.find(']');
			if (end != std::string::npos)
				return hp.substr(1, end - 1);
			return hp; // malformed fallback
		}

		// IPv4: ip:port
		auto pos = hp.find(':');
		if (pos == std::string::npos)
			return hp;

		return hp.substr(0, pos);
	}

	bool IsIpv4(const std::string &ip) {
		return ip.find(':') == std::string::npos;
	}

	bool IsIpv6(const std::string &ip) {
		return ip.find(':') != std::string::npos;
	}

	bool IsPrivateIpv4(const std::string &ip) {
		if (ip.rfind("10.", 0) == 0) return true;
		if (ip.rfind("192.168.", 0) == 0) return true;

		if (ip.rfind("172.", 0) == 0) {
			size_t dot2 = ip.find('.', 4);
			if (dot2 != std::string::npos) {
				int second = std::atoi(ip.c_str() + 4);
				if (second >= 16 && second <= 31)
					return true;
			}
		}

		return false;
	}

	bool IsIpv6Ula(const std::string &ip) {
		if (ip.size() < 2) return false;
		char c0 = std::tolower(ip[0]);
		char c1 = std::tolower(ip[1]);
		return (c0 == 'f' && (c1 == 'c' || c1 == 'd'));
	}

	bool IsIpv6LinkLocal(const std::string &ip) {
		return ip.rfind("fe80", 0) == 0 || ip.rfind("FE80", 0) == 0;
	}

	bool IsIpv6Gua(const std::string &ip) {
		if (ip.empty()) return false;
		char c0 = ip[0];
		return (c0 == '2' || c0 == '3'); // 2000::/3
	}

	// Higher = better
	int IpPriority(const std::string &ip) {
		if (IsIpv4(ip)) {
			if (IsPrivateIpv4(ip)) return 100; // best
			return 90;                         // public IPv4
		}

		if (IsIpv6(ip)) {
			if (IsIpv6Ula(ip)) return 80;
			if (IsIpv6Gua(ip)) return 70;
			if (IsIpv6LinkLocal(ip)) return 10;
			return 5; // other IPv6
		}

		return 0;
	}

	std::string PickBestIp(const std::vector<std::string> &ips) {
		std::string best;
		int bestScore = -1;

		for (const auto &ip : ips) {
			int score = IpPriority(ip);
			if (score > bestScore) {
				bestScore = score;
				best = ip;
			}
		}

		return best;
	}

	std::pair<std::string, int> SplitIPAddress(const std::string &address, char deliminator) {
	    std::stringstream ss(address);
	    std::string ip_address;
	    std::string port_str;

	    if (std::getline(ss, ip_address, deliminator) && std::getline(ss, port_str)) {

		    try {
			    int port = std::stoi(port_str);
			    return {ip_address, port};
		    } catch (const std::invalid_argument &e) {
			    throw std::invalid_argument("Invalid port number format");
		    } catch (const std::out_of_range &e) {
			    throw std::out_of_range("Port number out of range");
		    }
	    } else {
		    return {address, 0};
	    }
    }

	std::string GetBroadcastIP(std::string ipAddress) {
	    uint32_t subnetMask;
	    uint32_t ipAddress_int;
	    struct in_addr ip_addr;

	    auto splitIP = NetworkUtils::SplitIPAddress(ipAddress, '/');
	    if (inet_pton(AF_INET, splitIP.first.c_str(), &ip_addr) != 1) {
		    throw std::invalid_argument("The given IP address was invalid.\n");
	    }

	    if (splitIP.second > 0) {
		    struct sockaddr_in sa;
		    inet_pton(AF_INET, splitIP.first.c_str(), &(sa.sin_addr));
		    ipAddress_int = sa.sin_addr.s_addr;
		    uint32_t mask = htonl(~((1 << (32 - splitIP.second)) - 1));
		    uint32_t ip_num = ipAddress_int | ~mask;
		    sa.sin_addr.s_addr = ip_num;
		    char ip_stra[INET_ADDRSTRLEN];
		    inet_ntop(AF_INET, &(sa.sin_addr), ip_stra, INET_ADDRSTRLEN);
		    return std::string(ip_stra);
	    }

	    ipAddress_int = ntohl(ip_addr.s_addr);
	    if ((ipAddress_int & 0xF0000000) == 0x00000000) {        // Class A
		    subnetMask = 4278190080;                             // 255.0.0.0
	    } else if ((ipAddress_int & 0xE0000000) == 0x80000000) { // Class B
		    subnetMask = 4294901760;                             // 255.255.0.0
	    } else if ((ipAddress_int & 0xC0000000) == 0xC0000000) { // Class C
		    subnetMask = 4294967040;                             // 255.255.255.0
	    } else {
		    WSACleanup();
		    return "";
	    }

	    auto broadcastInt = ipAddress_int | (~subnetMask);

	    std::stringstream ss3;
	    for (int i = 3; i >= 0; --i) {
		    ss3 << ((broadcastInt >> (8 * i)) & 0xFF);
		    if (i > 0) {
			    ss3 << ".";
		    }
	    }

	    return ss3.str();
    }

}