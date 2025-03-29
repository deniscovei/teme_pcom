#include "lib.h"
#include "protocols.h"
#include <arpa/inet.h> 
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std;

#define MAX_ENTRIES 	1e5

#define HARDWARE_TYPE_ETHERNET	1

#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806

#define ARP_REQUEST		0x0001
#define ARP_REPLY		0x0002

#define ICMP_ECHO_REPLY					0
#define ICMP_DESTINATION_UNREACHABLE	3
#define ICMP_ECHO_REQEST				8
#define ICMP_TIME_EXCEEDED				11

#define MAC_LEN	6
#define IP_LEN	4

struct mask_entry {
	uint32_t mask;
	vector<route_table_entry*> entries;
};

vector<mask_entry> mask_entries;

/* Routing table */
route_table_entry* rtable;
int rtable_len;

/* Arp table */
arp_table_entry* arp_table;
int arp_table_len;

struct packet_t {
	uint32_t ip_next_hop;
	char buf[MAX_PACKET_LEN];
	size_t buf_len;

	packet_t(uint32_t ip_next_hop, char *buf, size_t buf_len)
	{
		this->ip_next_hop = ip_next_hop;
		memcpy(this->buf, buf, buf_len);
		this->buf_len = buf_len;
	}
};

queue<packet_t> packets_queue;

route_table_entry* search_by_prefix(vector<route_table_entry*>& entries, uint32_t ip_dest, uint32_t mask)
{
	route_table_entry* best_entry = nullptr;

	uint32_t prefix = (ip_dest & mask);
	int left = 0, right = entries.size() - 1;

	while (left <= right) {
		int mid = (left + right) / 2;

		if (prefix == entries[mid]->prefix) {
			best_entry = entries[mid];
			left = mid + 1;
		} else if (prefix < entries[mid]->prefix) {
			right = mid - 1;
		} else {
			left = mid + 1;
		}
	}

	return best_entry;
}

void print_ip(uint32_t ip)
{
	uint32_t addr = ntohl(ip);
	printf("%d.%d.%d.%d\n",
		(addr >> 24) & 0xff,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		addr & 0xff);
}

void print_mac(uint8_t* mac)
{
	for (int i = 0; i < MAC_LEN - 1; i++) {
		printf("%02x:", mac[i]);
	}
	printf("%02x\n", mac[MAC_LEN - 1]);
}

route_table_entry* get_best_route(vector<mask_entry>& mask_entries, uint32_t ip_dest)
{
	for (auto& [mask, entries] : mask_entries) {
		route_table_entry* best_entry = search_by_prefix(entries, ip_dest, mask);

		if (best_entry != nullptr) {
			return best_entry;
		}
	}

	return nullptr;
}

arp_table_entry* get_arp_entry(uint32_t given_ip)
{
	for (int i = 0; i < arp_table_len; i++) {
		if (arp_table[i].ip == given_ip) {
			return &arp_table[i];
		}
	}

	return nullptr;
}

vector <mask_entry> build_mask_entries(route_table_entry* rtable, int rtable_len)
{
	sort(rtable, rtable + rtable_len, [](const route_table_entry& r1, const route_table_entry& r2) {
		if (r1.mask == r2.mask) {
			return r1.prefix < r2.prefix;
		}
		return r1.mask > r2.mask;
		});

	vector<mask_entry> mask_entries;

	for (int i = 0; i < rtable_len; i++) {
		if (mask_entries.empty() || rtable[i].mask != mask_entries[mask_entries.size() - 1].mask) {
			mask_entries.push_back({ rtable[i].mask, {&rtable[i]} });
		} else {
			mask_entries[mask_entries.size() - 1].entries.push_back(&rtable[i]);
		}
	}

	return mask_entries;
}

void icmp_reply(uint8_t type, uint8_t code, int interface, char* buf)
{
	ether_header* eth_hdr = (ether_header*)buf;
	iphdr* ip_hdr = (iphdr*)(buf + sizeof(ether_header));
	icmphdr* icmp_hdr = (icmphdr*)(buf + sizeof(ether_header) + sizeof(iphdr));

	// populate icmp header
	icmp_hdr->type = type;
	icmp_hdr->code = code;

	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = htons(checksum((uint16_t*)icmp_hdr, sizeof(icmp_hdr)));

	// assign information of the old packet
	memcpy(icmp_hdr + sizeof(icmphdr), ip_hdr, sizeof(iphdr));
	memcpy(icmp_hdr + sizeof(icmphdr) + sizeof(iphdr), buf, 64);

	// populate eth header
	get_interface_mac(interface, eth_hdr->ether_shost);

	// populate ip header
	ip_hdr->tot_len = htons(sizeof(iphdr) + sizeof(icmphdr));
	ip_hdr->ihl = 5;
	ip_hdr->version = 4;
	ip_hdr->tos = 0;

	ip_hdr->id = 1;
	ip_hdr->frag_off = 0;
	ip_hdr->protocol = IPPROTO_ICMP;

	ip_hdr->ttl = 64;

	swap(ip_hdr->saddr, ip_hdr->daddr);

	ip_hdr->check = 0;
	ip_hdr->check = htons(checksum((uint16_t*)ip_hdr, sizeof(iphdr)));

	// send reply
	send_to_link(interface, buf, sizeof(ether_header) + 2 * sizeof(iphdr) + sizeof(icmphdr) + 64);
}

void send_arp_request(int interface_next_hop, uint32_t ip_next_hop, char* buf)
{
	ether_header* eth_hdr = (ether_header*)buf;
	arp_header* arp_hdr = (arp_header*)(buf + sizeof(ether_header));

	// populate eth header
	get_interface_mac(interface_next_hop, eth_hdr->ether_shost);
	memset(eth_hdr->ether_dhost, 0xff, MAC_LEN); // broadcast MAC address
	eth_hdr->ether_type = htons(ETHERTYPE_ARP);

	// populate arp header
	arp_hdr->htype = htons(HARDWARE_TYPE_ETHERNET);
	arp_hdr->ptype = htons(ETHERTYPE_IP);
	arp_hdr->hlen = MAC_LEN;
	arp_hdr->plen = IP_LEN;
	arp_hdr->op = htons(ARP_REQUEST);

	get_interface_mac(interface_next_hop, arp_hdr->sha); // sender MAC address
	arp_hdr->spa = inet_addr(get_interface_ip(interface_next_hop)); // sender IP address

	memset(arp_hdr->tha, 0, MAC_LEN); // target MAC address
	arp_hdr->tpa = ip_next_hop; // target IP address

	send_to_link(interface_next_hop, buf, sizeof(ether_header) + sizeof(arp_header));
}

void handle_ip_packet(int interface, char* buf, size_t buf_len)
{
	ether_header* eth_hdr = (ether_header*)buf;
	iphdr* ip_hdr = (iphdr*)(buf + sizeof(ether_header));
	icmphdr* icmp_hdr = (icmphdr*)(buf + sizeof(ether_header) + sizeof(iphdr));

	printf("Source: ");
	print_ip(ip_hdr->saddr);

	printf("Destination: ");
	print_ip(ip_hdr->daddr);

	if (inet_addr(get_interface_ip(interface)) == ip_hdr->daddr && icmp_hdr->type == ICMP_ECHO_REQEST) {
		icmp_reply(ICMP_ECHO_REPLY, 0, interface, buf);
		return;
	}

	uint16_t received_checksum = ntohs(ip_hdr->check);
	ip_hdr->check = 0;
	uint16_t calculated_checksum = checksum((uint16_t*)ip_hdr, sizeof(iphdr));

	if (calculated_checksum != received_checksum) {
		printf("Wrong checksum\n");
		return;
	}

	route_table_entry* best_route = get_best_route(mask_entries, ip_hdr->daddr);

	if (best_route == nullptr) {
		printf("No route found\n");
		icmp_reply(ICMP_DESTINATION_UNREACHABLE, 0, interface, buf);
		return;
	}

	printf("Next hop found at ip: ");
	print_ip(best_route->next_hop);

	if (ip_hdr->ttl <= 1) {
		printf("Time exceeded\n");
		icmp_reply(ICMP_TIME_EXCEEDED, 0, interface, buf);
		return;
	}

	ip_hdr->ttl--;
	ip_hdr->check = htons(checksum((uint16_t*)ip_hdr, sizeof(iphdr)));

	arp_table_entry* next_hop = get_arp_entry(best_route->next_hop);

	if (next_hop == nullptr) {
		printf("No MAC address found for next hop, enqueuing packet\n");
		packets_queue.push(packet_t(best_route->next_hop, buf, buf_len));
		send_arp_request(best_route->interface, best_route->next_hop, buf);
		return;
	}

	printf("We succesfully send a packet\n");

	memcpy(eth_hdr->ether_dhost, next_hop->mac, MAC_LEN);
	get_interface_mac(best_route->interface, eth_hdr->ether_shost);

	send_to_link(best_route->interface, buf, buf_len);
}

void send_queued_packets(int interface, uint32_t ip)
{
	printf("Checking queue\n");

	size_t packets_to_check = packets_queue.size();

	while (!packets_queue.empty() && packets_to_check > 0)
	{
		packet_t packet = packets_queue.front();
		packets_queue.pop();
		packets_to_check--;

		printf("Packet found in queue from ip: ");
		print_ip(packet.ip_next_hop);

		// check if the packet's next hop ip corresponds with the arp reply found one
		if (packet.ip_next_hop == ip) {
			ether_header* eth_hdr = (ether_header*)packet.buf;

			// update next hop MAC address
			memcpy(eth_hdr->ether_dhost, get_arp_entry(ip)->mac, MAC_LEN);

			// send queued packet
			send_to_link(interface, packet.buf, packet.buf_len);
		} else {
			packets_queue.push(packet);
		}
	}
}

void send_arp_reply(int interface, char* buf)
{
	ether_header* eth_hdr = (ether_header*)buf;
	arp_header* arp_hdr = (arp_header*)(buf + sizeof(ether_header));

	// populate eth header
	get_interface_mac(interface, eth_hdr->ether_shost);
	memcpy(eth_hdr->ether_dhost, arp_hdr->sha, MAC_LEN);

	// populate arp header (considering already populated fields)
	arp_hdr->op = htons(ARP_REPLY);

	memcpy(arp_hdr->tha, arp_hdr->sha, MAC_LEN); // target MAC address
	arp_hdr->tpa = arp_hdr->spa; // target IP address

	memcpy(arp_hdr->sha, eth_hdr->ether_shost, MAC_LEN); // sender MAC address
	arp_hdr->spa = inet_addr(get_interface_ip(interface)); // sender IP address

	send_to_link(interface, buf, sizeof(ether_header) + sizeof(arp_header));
}

void update_arp_table(int interface, uint32_t ip, uint8_t* mac)
{
	bool mac_cached = false;

	for (int i = 0; i < arp_table_len; i++) {
		if (arp_table[i].ip == ip) {
			memcpy(arp_table[i].mac, mac, MAC_LEN);
			mac_cached = true;
			break;
		}
	}

	if (!mac_cached) {
		arp_table[arp_table_len].ip = ip;
		memcpy(arp_table[arp_table_len++].mac, mac, MAC_LEN);
	}
}

void handle_arp_packet(int interface, char* buf)
{
	arp_header* arp_hdr = (arp_header*)(buf + sizeof(ether_header));

	if (ntohs(arp_hdr->op) == ARP_REQUEST) {
		printf("ARP request\n");
		send_arp_reply(interface, buf);
	} else if (ntohs(arp_hdr->op) == ARP_REPLY) {
		printf("ARP reply\n");

		printf("Arp source ip: ");
		print_ip(arp_hdr->spa);
		printf("Arp source mac: ");
		print_mac(arp_hdr->sha);
		
		update_arp_table(interface, arp_hdr->spa, arp_hdr->sha);
		send_queued_packets(interface, arp_hdr->spa);
	}
}

int main(int argc, char* argv[])
{
	int interface;
	char buf[MAX_PACKET_LEN];
	size_t buf_len;

	init(argc - 2, argv + 2);

	rtable = (route_table_entry*)malloc(sizeof(route_table_entry) * MAX_ENTRIES);
	DIE(rtable == nullptr, "memory");

	arp_table = (arp_table_entry*)malloc(sizeof(arp_table_entry) * MAX_ENTRIES);
	DIE(arp_table == nullptr, "memory");

	rtable_len = read_rtable(argv[1], rtable);
	arp_table_len = 0;

	mask_entries = build_mask_entries(rtable, rtable_len);

	while (1) {
		interface = recv_from_any_link(buf, &buf_len);
		DIE(interface < 0, "recv_from_any_links");
		printf("\nPackage received on interface: %d\n", interface);

		ether_header* eth_hdr = (ether_header*)buf;

		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
			printf("Packet type: IP\n");
			handle_ip_packet(interface, buf, buf_len);
		} else if (ntohs(eth_hdr->ether_type) == ETHERTYPE_ARP) {
			printf("Packet type: ARP\n");
			handle_arp_packet(interface, buf);
		}
	}
}

