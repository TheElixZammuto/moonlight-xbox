#include "pch.h"
#include <mdns.h>
#include <Utils.hpp>

using namespace moonlight_xbox_dx;
using namespace Windows::Networking::Connectivity;

static char addrbuffer[64];
static char entrybuffer[256];
static char namebuffer[256];
static char sendbuffer[1024];
static int  mdns_buffer[4096];
static mdns_record_txt_t txtbuffer[128];

static mdns_string_t
ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr,
	size_t addrlen) {
	char host[NI_MAXHOST] = { 0 };
	char service[NI_MAXSERV] = { 0 };
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	int len = 0;
	if (ret == 0) {
		if (addr->sin_port != 0)
			len = snprintf(buffer, capacity, "%s:%s", host, service);
		else
			len = snprintf(buffer, capacity, "%s", host);
	}
	if (len >= (int)capacity)
		len = (int)capacity - 1;
	mdns_string_t str;
	str.str = buffer;
	str.length = len;
	return str;
}

static mdns_string_t
ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr,
	size_t addrlen) {
	char host[NI_MAXHOST] = { 0 };
	char service[NI_MAXSERV] = { 0 };
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	int len = 0;
	if (ret == 0) {
		if (addr->sin6_port != 0)
			len = snprintf(buffer, capacity, "[%s]:%s", host, service);
		else
			len = snprintf(buffer, capacity, "%s", host);
	}
	if (len >= (int)capacity)
		len = (int)capacity - 1;
	mdns_string_t str;
	str.str = buffer;
	str.length = len;
	return str;
}

static mdns_string_t
ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen) {
	if (addr->sa_family == AF_INET6)
		return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6*)addr, addrlen);
	return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in*)addr, addrlen);
}


// Callback handling parsing answers to queries sent
static int
query_callback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
	uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
	size_t size, size_t name_offset, size_t name_length, size_t record_offset,
	size_t record_length, void* user_data) {
	(void)sizeof(sock);
	(void)sizeof(query_id);
	(void)sizeof(name_length);
	(void)sizeof(user_data);
	mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);
	const char* entrytype = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer" : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
	mdns_string_t entrystr = mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));
	if (rtype == MDNS_RECORDTYPE_SRV) {
		mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length,
			namebuffer, sizeof(namebuffer));
		auto hostname = std::string(srv.name.str);
		hostname.pop_back();
		auto hostnamePlusPort = Utils::StringFromStdString(hostname) + ":" + srv.port;
		GetApplicationState()->AddHost(hostnamePlusPort);
	}
	/*else if (rtype == MDNS_RECORDTYPE_A) {
		struct sockaddr_in addr;
		mdns_record_parse_a(data, size, record_offset, record_length, &addr);
		mdns_string_t addrstr =
			ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
		printf("%.*s : %s %.*s A %.*s\n", MDNS_STRING_FORMAT(fromaddrstr), entrytype,
			MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(addrstr));
		
	}
	else if (rtype == MDNS_RECORDTYPE_AAAA) {
		struct sockaddr_in6 addr;
		mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
		mdns_string_t addrstr =
			ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
		printf("%.*s : %s %.*s AAAA %.*s\n", MDNS_STRING_FORMAT(fromaddrstr), entrytype,
			MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(addrstr));
	}
	else {
		printf("%.*s : %s %.*s type %u rclass 0x%x ttl %u length %d\n",
			MDNS_STRING_FORMAT(fromaddrstr), entrytype, MDNS_STRING_FORMAT(entrystr), rtype,
			rclass, ttl, (int)record_length);
	}*/
	return 0;
}


int init_mdns() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	for(auto h : NetworkInformation::GetHostNames()) {
		if (h->IPInformation == nullptr || h->Type != Windows::Networking::HostNameType::Ipv4)continue;
		auto ip = h->ToString();
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = MDNS_PORT;
		inet_pton(AF_INET, Utils::PlatformStringToStdString(ip).c_str(), &addr.sin_addr.s_addr);
		int socket = mdns_socket_open_ipv4(&addr);
		auto str = "_nvstream._tcp.local";
		mdns_query_send(socket, MDNS_RECORDTYPE_PTR, str, strlen(str), &mdns_buffer, 4096, 0);
		return socket;
	}
}

int query_mdns(int socket) {
	return mdns_query_recv(socket, &mdns_buffer, 4096, query_callback, NULL, 0);
}