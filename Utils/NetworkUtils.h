#pragma once
#include <string>
#include <vector>

namespace NetworkUtils {

	std::string ExtractIpFromHostPort(const std::string &hp);

	bool IsIpv4(const std::string &ip);
	bool IsIpv6(const std::string &ip);

	bool IsPrivateIpv4(const std::string &ip);

	bool IsIpv6Ula(const std::string &ip);
	bool IsIpv6Gua(const std::string &ip);
	bool IsIpv6LinkLocal(const std::string &ip);

	int IpPriority(const std::string &ip);

	std::string PickBestIp(const std::vector<std::string> &ips);

	std::pair<std::string, int> SplitIPAddress(const std::string &address, char deliminator);
    std::string GetBroadcastIP(std::string ipAddress);

}
