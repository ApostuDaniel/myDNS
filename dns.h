/*
    DNS Header for packet forging
    Copyright (C) 2016 unh0lys0da

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DNS_H
#define DNS_H



#define DNS_OPCODE_QUERY	0
#define DNS_OPCODE_IQUERY	1
#define DNS_OPCODE_STATUS	2
#define DNS_OPCODE_NOTIFY	4
#define DNS_OPCODE_UPGRADE	5

#define DNS_RCODE_NOERROR	0
#define DNS_RCODE_FORMERR	1
#define DNS_RCODE_SERVFAIL	2
#define DNS_RCODE_NXDOMAIN	3
#define DNS_RCODE_NOTIMP	4
#define DNS_RCODE_REFUSED	5
#define DNS_RCODE_YXDOMAIN	6
#define DNS_RCODE_YXRRSET	7
#define DNS_RCODE_NXRRSET	8
#define DNS_RCODE_NOTAUTH	9
#define DNS_RCODE_NOTZONE	10
#define DNS_RCODE_BADVERS	16
#define DNS_RCODE_BADSIG	16
#define DNS_RCODE_BADKEY	17
#define DNS_RCODE_BADTIME	18
#define DNS_RCODE_BADMODE	19
#define DNS_RCODE_BADNAME	20
#define DNS_RCODE_BADALG	21
#define DNS_RCODE_BADTRUNC	22
#define DNS_RCODE_BADCOOKIE	23

#include <endian.h>
#include <stdint.h>
#include <strings.h>
typedef struct {
	uint16_t id;
# if __BYTE_ORDER == __BIG_ENDIAN
	uint16_t qr:1;
	uint16_t opcode:4;
	uint16_t aa:1;
	uint16_t tc:1;
	uint16_t rd:1;
	uint16_t ra:1;
	uint16_t zero:3;
	uint16_t rcode:4;
# elif __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t rd:1;  /*Recursion desired bit*/
	uint16_t tc:1;  /*Response Truncated bit*/
	uint16_t aa:1;  /*Authorative Answer bit*/
	uint16_t opcode:4;  /*Operation code, values above*/
	uint16_t qr:1;  /*unset if message is a query, set if it is a response*/
	uint16_t rcode:4;  /*Response code, values above*/
	uint16_t zero:3;  /*rezerved for future use*/
	uint16_t ra:1;  /*Recursion available*/
# else
#  error "Adjust your <bits/endian.h> defines"
# endif
	uint16_t qcount;	/* question count */
	uint16_t ancount;	/* Answer record count */
	uint16_t nscount;	/* Name Server (Autority Record) Count */ 
	uint16_t adcount;	/* Additional Record Count */
} dnshdr;



/* DNS QTYPES */
#define DNS_QTYPE_A		1
#define DNS_QTYPE_NS		2
#define DNS_QTYPE_CNAME		5
#define DNS_QTYPE_SOA		6
#define DNS_QTYPE_PTR		12
#define DNS_QTYPE_MX		15
#define DNS_QTYPE_TXT		16
#define DNS_QTYPE_RP		17
#define DNS_QTYPE_AFSDB		18
#define DNS_QTYPE_SIG		24
#define DNS_QTYPE_KEY		25
#define DNS_QTYPE_AAAA		28
#define DNS_QTYPE_LOC		29
#define DNS_QTYPE_SRV		33
#define DNS_QTYPE_NAPTR		35
#define DNS_QTYPE_KX		36
#define DNS_QTYPE_CERT		37
#define DNS_QTYPE_DNAME		39
#define DNS_QTYPE_OPT		41
#define DNS_QTYPE_APL		42
#define DNS_QTYPE_DS		43
#define DNS_QTYPE_SSHFP		44
#define DNS_QTYPE_IPSECKEY	45
#define DNS_QTYPE_RRSIG		46
#define DNS_QTYPE_NSEC		47
#define DNS_QTYPE_DNSKEY	48
#define DNS_QTYPE_DHCID		49
#define DNS_QTYPE_NSEC3		50
#define DNS_QTYPE_NSEC3PARAM	51
#define DNS_QTYPE_TLSA		52
#define DNS_QTYPE_HIP		55
#define DNS_QTYPE_CDS		59
#define DNS_QTYPE_CDNSKEY	60
#define DNS_QTYPE_TKEY		249
#define DNS_QTYPE_TSIG		250
#define DNS_QTYPE_IXFR		251
#define DNS_QTYPE_AXFR		252
#define DNS_QTYPE_ALL		255 /* AKA: * QTYPE */
#define DNS_QTYPE_URI		256
#define DNS_QTYPE_CAA		257
#define DNS_QTYPE_TA		32768
#define DNS_QTYPE_DLV		32769

/* DNS QCLASS */
#define DNS_QCLASS_RESERVED	0
#define DNS_QCLASS_IN		1
#define DNS_QCLASS_CH		3
#define DNS_QCLASS_HS		4
#define DNS_QCLASS_NONE		254
#define DNS_QCLASS_ANY		255

typedef struct {
    dnshdr Header;
    unsigned char qName[264];
    uint16_t QType;
    uint16_t QClass;
} dnsquery;

typedef struct {
    dnsquery query;
    unsigned char name[264];
    uint16_t Type;
    uint16_t Class;
    uint32_t TTL;
    uint32_t DataLength;
    char Data[512];
} dnsresponse;

dnsquery createQuery(char* domainName);

unsigned char* dns_format ( char *url, int *size);

void queryAssign(dnsquery* dest, dnsquery* src){
    dest->Header = src->Header;
    dest->QClass = src->QClass;
    dest->QType = src->QType;
    strncpy(dest->qName, src->qName, strlen(src->qName));
}

void responseAssign(dnsresponse* dest, dnsresponse* src){
    queryAssign(&dest->query, &src->query);
    dest->Class = src->Class;
    strncpy(dest->name, src->name, strlen(src->name));
    dest->DataLength = src->DataLength;
    dest->Type = src->Type;
    dest->TTL = src->TTL;
    strncpy(dest->Data, src->Data, src->DataLength);
}
#endif // !DNS_H