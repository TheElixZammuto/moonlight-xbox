#include "pch.h"
#include <Utils.hpp>
#include <mdns.h>

using namespace moonlight_xbox_dx;
using namespace Windows::Networking::Connectivity;

static char addrbuffer[64];
static char entrybuffer[256];
static char namebuffer[256];
static uint8_t mdns_buffer[4096];
static int sockets[8] = {0};

const char *service = "_nvstream._tcp.local.";

static std::map<std::string, std::string> instanceToHost;
static std::map<std::string, uint16_t> instanceToPort;
static std::map<std::string, std::vector<std::string>> hostToIps;

// Normalize DNS names for consistent matching (e.g. strip trailing '.')
static std::string normalize_name(const std::string &name) {
	if (!name.empty() && name.back() == '.')
		return name.substr(0, name.size() - 1);
	return name;
}

static mdns_string_t ipv4_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in *addr, size_t addrlen) {
	char host[NI_MAXHOST] = { 0 };
	char service[NI_MAXSERV] = { 0 };
	int ret = getnameinfo(
	    (const struct sockaddr *)addr, (socklen_t)addrlen,
	    host, NI_MAXHOST,
	    service, NI_MAXSERV,
	    NI_NUMERICSERV | NI_NUMERICHOST);

	int len = 0;
	if (ret == 0) {
		if (addr->sin_port != 0) {
			len = snprintf(buffer, capacity, "%s:%s", host, service);
		} else {
			len = snprintf(buffer, capacity, "%s", host);
		}
	}

	if (len >= (int)capacity) {
		len = (int)capacity - 1;
	}

	mdns_string_t str;
	str.str = buffer;
	str.length = len;
	return str;
}

static mdns_string_t ip_address_to_string(char *buffer, size_t capacity, const struct sockaddr *addr, size_t addrlen) {

	if (addr->sa_family == AF_INET6) {
		char host[NI_MAXHOST] = {0};
		int ret = getnameinfo(
		    addr, (socklen_t)addrlen,
		    host, NI_MAXHOST,
		    nullptr, 0,
		    NI_NUMERICHOST);

		int len = 0;
		if (ret == 0) {
			len = snprintf(buffer, capacity, "%s", host);
		}

		if (len >= (int)capacity) {
			len = (int)capacity - 1;
		}

		mdns_string_t s;
		s.str = buffer;
		s.length = len;
		return s;
	}


	return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in *)addr, addrlen);
}

static size_t dns_read_name(const uint8_t *data, size_t size, size_t offset, char *out, size_t out_capacity) {

	if (!data || !out || out_capacity == 0 || offset >= size) {

		if (out_capacity > 0) {
			out[0] = '\0';
		}

		return offset;
	}

	size_t out_len = 0;
	bool jumped = false;
	size_t jump_offset = 0;

	while (offset < size) {
		uint8_t len = data[offset];

		// end of name
		if (len == 0) {
			offset++;
			break;
		}

		// compression pointer
		if ((len & 0xC0) == 0xC0) {
			if (offset + 1 >= size) break;
			uint8_t b2 = data[offset + 1];
			uint16_t ptr = ((len & 0x3F) << 8) | b2;
			if (!jumped) {
				jump_offset = offset + 2;
				jumped = true;
			}
			offset = ptr;
			continue;
		}

		// normal label
		offset++;
		if (offset + len > size) break;

		if (out_len > 0 && out_len + 1 < out_capacity) {
			out[out_len++] = '.';
		}

		for (uint8_t i = 0; i < len && out_len + 1 < out_capacity; ++i) {
			out[out_len++] = (char)data[offset + i];
		}

		offset += len;
	}

	if (out_capacity > 0) {

		if (out_len >= out_capacity) {
			out_len = out_capacity - 1;
		}

		out[out_len] = '\0';
	}

	if (jumped) {
		return jump_offset;
	}

	return offset;
}

static bool is_ipv4(const std::string &ip) {
	return ip.find(':') == std::string::npos;
}

static bool is_private_ipv4(const std::string &ip) {
	if (ip.rfind("10.", 0) == 0) return true;
	if (ip.rfind("192.168.", 0) == 0) return true;
	if (ip.rfind("172.", 0) == 0) {
		size_t dot2 = ip.find('.', 4);
		if (dot2 != std::string::npos) {
			int second = std::atoi(ip.c_str() + 4);
			if (second >= 16 && second <= 31) return true;
		}
	}
	return false;
}

static bool is_ipv6(const std::string &ip) {
	return ip.find(':') != std::string::npos;
}

static bool is_ipv6_ula(const std::string &ip) {
	if (ip.size() < 2) return false;
	char c0 = std::tolower(ip[0]);
	char c1 = std::tolower(ip[1]);
	return (c0 == 'f' && (c1 == 'c' || c1 == 'd'));
}

static bool is_ipv6_link_local(const std::string &ip) {
	return ip.rfind("fe80", 0) == 0 || ip.rfind("FE80", 0) == 0;
}

static bool is_ipv6_gua(const std::string &ip) {
	char c0 = ip.empty() ? 0 : ip[0];
	return (c0 == '2' || c0 == '3');
}

static int ip_priority(const std::string &ip) {
	if (is_ipv4(ip)) {
		if (is_private_ipv4(ip)) return 100;
		return 90;
	}
	if (is_ipv6(ip)) {
		if (is_ipv6_ula(ip)) return 80;
		if (is_ipv6_gua(ip)) return 70;
		if (is_ipv6_link_local(ip)) return 10;
		return 5;
	}
	return 0;
}

static std::string pick_best_ip(const std::vector<std::string> &ips) {
	std::string best;
	int bestScore = -1;

	for (auto &ip : ips) {
		int score = ip_priority(ip);
		if (score > bestScore) {
			bestScore = score;
			best = ip;
		}
	}

	return best;
}

static int query_callback(
    int sock, const struct sockaddr *from, size_t addrlen,
    mdns_entry_type_t entry, uint16_t query_id,
    uint16_t rtype, uint16_t rclass, uint32_t ttl,
    const void *data, size_t size,
    size_t name_offset, size_t name_length,
    size_t record_offset, size_t record_length,
    void *user_data) {

	(void)sock;
	(void)entry;
	(void)query_id;
	(void)rclass;
	(void)ttl;
	(void)name_length;
	(void)user_data;
	(void)from;
	(void)addrlen;

	mdns_string_t entrystr =
	    mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));

	std::string key(entrystr.str, entrystr.length);
	std::string keyNorm = normalize_name(key);

	// PTR: _nvstream._tcp.local. -> instance name
	if (rtype == MDNS_RECORDTYPE_PTR) {
		mdns_string_t ptrname =
		    mdns_record_parse_ptr(data, size, record_offset, record_length,
		                          namebuffer, sizeof(namebuffer));

		std::string instance(ptrname.str, ptrname.length);
		std::string instanceNorm = normalize_name(instance);

		if (!instanceToHost.count(instanceNorm)) {
			instanceToHost[instanceNorm] = "";
		}
	}
	// SRV: instance (via key) + port + target hostname
	else if (rtype == MDNS_RECORDTYPE_SRV) {
		mdns_record_srv_t srv =
		    mdns_record_parse_srv(data, size, record_offset, record_length,
		                          namebuffer, sizeof(namebuffer));

		// srv.name is not reliable as the instance; use owner name (keyNorm)
		std::string instance = keyNorm;

		// RDATA layout: priority(2) + weight(2) + port(2) + target(DNS name)
		size_t target_offset = record_offset + 6;
		char hostbuf[256];
		dns_read_name((const uint8_t *)data, size, target_offset, hostbuf, sizeof(hostbuf));

		std::string hostnameRaw(hostbuf);
		std::string hostname = normalize_name(hostnameRaw);

		instanceToHost[instance] = hostname;
		instanceToPort[instance] = srv.port;
	}
	// A: hostname -> IPv4 address
	else if (rtype == MDNS_RECORDTYPE_A) {
		struct sockaddr_in addr;
		mdns_record_parse_a(data, size, record_offset, record_length, &addr);

		mdns_string_t addrstr =
		    ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

		// A record owner name is the hostname
		std::string hostnameRaw(key);
		std::string hostname = normalize_name(hostnameRaw);
		std::string ip(addrstr.str, addrstr.length);

		auto &vec = hostToIps[hostname];
		if (std::find(vec.begin(), vec.end(), ip) == vec.end()) {
			vec.push_back(ip);
		}
	}
	// AAAA: hostname -> IPv6 address
	else if (rtype == MDNS_RECORDTYPE_AAAA) {
		struct sockaddr_in6 addr6;
		mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr6);

		mdns_string_t addrstr =
		    ip_address_to_string(namebuffer, sizeof(namebuffer),
		                         (const struct sockaddr *)&addr6, sizeof(addr6));

		std::string hostnameRaw(key);
		std::string hostname = normalize_name(hostnameRaw);
		std::string ip(addrstr.str, addrstr.length);

		auto &vec = hostToIps[hostname];
		if (std::find(vec.begin(), vec.end(), ip) == vec.end()) {
			vec.push_back(ip);
		}
	}

	// Try to resolve any complete instance → hostname → ip → port chain
	for (auto it = instanceToHost.begin(); it != instanceToHost.end();) {
		const std::string instance = it->first;
		const std::string hostname = it->second;

		bool hasHostname = !hostname.empty();
		bool hasPort = instanceToPort.count(instance) > 0;
		bool hasIp = hostToIps.count(hostname) > 0;

		if (!hasHostname || !hasPort || !hasIp) {
			++it;
			continue;
		}

		auto hostIpIt = hostToIps.find(hostname);
		if (hostIpIt == hostToIps.end() || hostIpIt->second.empty()) {
			++it;
			continue;
		}

		const std::vector<std::string> &ips = hostIpIt->second;
		std::string ip = pick_best_ip(ips);
		uint16_t port = instanceToPort[instance];

		std::string fullHostname;
		if (ip.find(':') != std::string::npos) {
			// IPv6 → wrap in brackets
			fullHostname = "[" + ip + "]:" + std::to_string(port);
		} else {
			// IPv4
			fullHostname = ip + ":" + std::to_string(port);
		}

		auto appState = GetApplicationState();

		// First try silent upgrade of an existing host keyed by mDNS instance name
		bool updated = appState->UpdateHostAddressForInstance(
		    Utils::StringFromStdString(instance),
		    Utils::StringFromStdString(fullHostname));

		if (!updated) {
			// No existing host: treat as a new host
			appState->AddHost(
			    Utils::StringFromStdString(fullHostname),
			    Utils::StringFromStdString(instance)); // store mDNS instance name
		}

		// Clean up discovery state
		instanceToPort.erase(instance);
		hostToIps.erase(hostname);
		it = instanceToHost.erase(it);

	}

	return 0;
}

void init_mdns() {

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	int k = 0;

	for (auto h : NetworkInformation::GetHostNames()) {
		if (h->IPInformation == nullptr ||
		    h->Type != Windows::Networking::HostNameType::Ipv4)
			continue;

		auto ip = h->ToString();
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		inet_pton(AF_INET, Utils::PlatformStringToStdString(ip).c_str(), &addr.sin_addr.s_addr);

		int sock = mdns_socket_open_ipv4(&addr);
		if (sock <= 0)
			continue;

		int loopback = 1;
		setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopback, sizeof(loopback));

		sockets[k++] = sock;
		if (k >= 8)
			break;
	}

	size_t service_length = strlen(service);

	for (int i = 0; i < 8; ++i) {
		if (sockets[i] == 0)
			break;

		mdns_query_send(
		    sockets[i],
		    MDNS_RECORDTYPE_PTR,
		    service,
		    service_length,
		    mdns_buffer,
		    sizeof(mdns_buffer),
		    0);
	}
}

int query_mdns() {

	size_t service_length = strlen(service);

	for (int i = 0; i < 8; i++) {
		if (sockets[i] == 0)
			break;

		int sendret = mdns_query_send(
			sockets[i],
			MDNS_RECORDTYPE_PTR,
			service,
			service_length,
			mdns_buffer,
			sizeof(mdns_buffer),
			0);

		if (sendret < 0) {
			Utils::Logf("[mdns] mdns_query_send failed on socket %d\n", sockets[i]);
			continue;
		}

		// Wait briefly for responses and drain them. Use select to allow for a small window.
		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(sockets[i], &readset);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200000; // 200ms

		int rv = select(sockets[i] + 1, &readset, NULL, NULL, &tv);
		if (rv > 0) {
			// Drain as many responses as available
			while (true) {
				size_t parsed = mdns_query_recv(
					sockets[i],
					mdns_buffer,
					sizeof(mdns_buffer),
					query_callback,
					nullptr,
					0);
				if (parsed == 0) break;
			}
		}
		else if (rv < 0) {
			int err = WSAGetLastError();
			Utils::Logf("[mdns] select error on socket %d: %d\n", sockets[i], err);
			// Try to close and mark socket for reinit
			mdns_socket_close(sockets[i]);
			sockets[i] = 0;
		}
		// else rv == 0 => timeout, no responses
	}

	return 0;
}

void close_mdns() {
	Utils::Log("[mdns] close_mdns called\n");
	for (int i = 0; i < 8; ++i) {
		if (sockets[i] != 0) {
			mdns_socket_close(sockets[i]);
			sockets[i] = 0;
		}
	}
	WSACleanup();
}

void reinit_mdns() {
	Utils::Log("[mdns] reinit_mdns called\n");
	close_mdns();
	instanceToHost.clear();
	instanceToPort.clear();
	hostToIps.clear();
	init_mdns();
}
