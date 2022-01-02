#ifndef RESOLOVER_UTILS_H
#define RESOLOVER_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "dns.h"

#define CONFIG "resolverConfig.txt"

bool processInput(char* domainName);

bool configureResolver(char** address, int* port, char* configFile);

void outputResponse(dnsresponse* response, FILE* fp);



#endif // !RESOLOVER_UTILS_H