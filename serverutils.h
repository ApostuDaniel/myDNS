#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "dns.h"

#define ADDR_FLD "Address"
#define PORT_FLD "Port"
#define DATABASE_FLD "DB"
#define ORIGIN_FLD "ORIGIN"

#define RESERVED_ID 65535

typedef struct{
    struct sockaddr_in address;
    uint16_t query_id;
} address_queryID_pair;

bool configureServer(char** address, int* port, char** database, char** origin, char* configFile);
char* fullyQualifiedDomainName(char* queryDomain);
void fillResponse(dnsresponse* queryResponse, sqlite3_stmt** stmt, bool isFromCache);
address_queryID_pair* createSharedClientsMapping(int nrOfClients);
int lookupClientForQuery(uint16_t id, struct sockaddr_in* client, address_queryID_pair* clientList, int clientListSize);
bool addToCache(dnsresponse* data, sqlite3** db, sqlite3_stmt** stmt, char* buff);
bool refreshCache(sqlite3** db, sqlite3_stmt** stmt, char* buff);
bool getAnswerFromCache(dnsresponse* data, sqlite3** db, sqlite3_stmt** stmt, char* buff);
bool getAnswerFromRR(dnsresponse* data, sqlite3** db, sqlite3_stmt** stmt, char* buff);
bool getNameServerFromRR(dnsresponse* data, struct sockaddr_in* next, sqlite3** db, sqlite3_stmt** stmt, char* buff);
int addClientToSenderList(uint16_t id, struct sockaddr_in* client, address_queryID_pair* clientList, int clientListSize);
void fillAddress(struct sockaddr_in* next, sqlite3_stmt** stmt);

#endif // !SERVER_UTILS_H