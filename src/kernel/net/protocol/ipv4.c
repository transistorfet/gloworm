
#include <endian.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../protocol.h"

#include "ipv4.h"


int ipv4_init();
int ipv4_decode_header(struct protocol *proto, struct packet *pack, uint16_t offset);
int ipv4_forward_packet(struct protocol *proto, struct packet *pack);


struct protocol_ops ipv4_protocol_ops = {
	ipv4_init,
	ipv4_decode_header,
	ipv4_forward_packet,
	NULL,
};

struct protocol ipv4_protocol = {
	&ipv4_protocol_ops,
	NULL,
	PF_INET,
	SOCK_RAW,
	IPPROTO_IP,
};


struct ipv4_header {
	uint8_t version: 4;
	uint8_t ihl: 4;
	uint8_t dscp: 6;
	uint8_t ecn: 2;
	uint16_t length;
	uint16_t id;
	uint16_t flags: 3;
	uint16_t frag_offset: 13;

	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;

	uint32_t src;
	uint32_t dest;
};

int ipv4_init()
{
	net_register_protocol(&ipv4_protocol);
	return 0;
}

int ipv4_decode_header(struct protocol *proto, struct packet *pack, uint16_t offset)
{
	int error;
	uint16_t checksum;
	struct protocol *next;
	struct ipv4_header *hdr;
	struct ipv4_custom_data *custom;

	pack->network_offset = offset;
	hdr = (struct ipv4_header *) &pack->data[pack->network_offset];

	if (pack->length - offset < sizeof(struct ipv4_header))
		return -1;
	if (hdr->version != 4 || hdr->ihl < 5 || hdr->ihl > 9)
		return -2;

	hdr->length = be16toh(hdr->length);
	hdr->checksum = be16toh(hdr->checksum);
	hdr->src = be32toh(hdr->src);
	hdr->dest = be32toh(hdr->dest);

	checksum = hdr->checksum;
	hdr->checksum = 0;
	if (checksum != ipv4_calculate_checksum(hdr, sizeof(struct ipv4_header), 0))
		return -3;
	hdr->checksum = checksum;

	custom = (struct ipv4_custom_data *) &pack->custom_data;
	custom->src.addr = hdr->src;
	custom->src.port = 0;
	custom->dest.addr = hdr->dest;
	custom->dest.port = 0;

	next = net_get_protocol(PF_INET, 0, hdr->protocol);
	if (!next)
		return -4;
	pack->proto = next;

	pack->transport_offset = offset + (((uint16_t) hdr->ihl) << 2);
	error = next->ops->decode_header(next, pack, pack->transport_offset);
	if (error)
		return error;
	return 0;
}

int ipv4_forward_packet(struct protocol *proto, struct packet *pack)
{
	if (!pack->proto || pack->proto == proto)
		return PACKET_DROPPED;
	return pack->proto->ops->forward_packet(pack->proto, pack);
}

int ipv4_encode_header(struct packet *pack, const struct ipv4_address *src, const struct ipv4_address *dest, const unsigned char *data, int length)
{
	struct ipv4_header *hdr;
	struct ipv4_custom_data *custom;

	if (!pack->proto)
		return -1;

	if (pack->capacity < pack->length + sizeof(struct ipv4_header) + length)
		return -2;
	pack->network_offset = pack->length;
	pack->length += sizeof(struct ipv4_header);

	hdr = (struct ipv4_header *) &pack->data[pack->network_offset];
	hdr->version = 4;
	hdr->ihl = 5;
	hdr->dscp = 0;
	hdr->ecn = 0;
	hdr->length = htobe16(sizeof(struct ipv4_header) + length);
	hdr->id = htobe16(0);
	hdr->flags = 2;
	hdr->frag_offset = htobe16(0);
	hdr->ttl = 0x40;
	hdr->protocol = pack->proto->protocol;
	hdr->checksum = htobe16(0);
	hdr->src = htobe32(src->addr);
	hdr->dest = htobe32(dest->addr);

	hdr->checksum = ipv4_calculate_checksum(hdr, sizeof(struct ipv4_header), 0);

	custom = (struct ipv4_custom_data *) &pack->custom_data;
	custom->src.addr = src->addr;
	custom->src.port = src->port;
	custom->dest.addr = dest->addr;
	custom->dest.port = dest->port;

	pack->transport_offset = pack->length;
	return 0;
}

uint16_t ipv4_calculate_checksum(void *data, int len, uint32_t start)
{
	uint16_t carry;
	uint32_t checksum = start;

	for (short i = 0; i < (len >> 1); i++)
		checksum += ((uint16_t *) data)[i];

	if (len & 0x01)
		checksum += (((uint8_t *) data)[len - 1] << 8);

	carry = checksum >> 16;
	checksum &= 0xFFFF;
	checksum += carry;
	if (checksum >> 16)
		checksum += 1;
	checksum = ~checksum & 0xFFFF;
	return checksum;
}


