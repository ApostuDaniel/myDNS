#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#define ADDR_FLD "Address"
#define PORT_FLD "Port"
#define DATABASE_FLD "DB"
#define ORIGIN_FLD "ORIGIN"

bool configureServer(char** address, int* port, char** database, char** origin, char* configFile);
char* fullyQualifiedDomainName(char* queryDomain);