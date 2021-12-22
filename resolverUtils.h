#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define CONFIG "resolverConfig.txt"

bool processInput(char* domainName);

bool configureResolver(char** address, int* port, char* configFile);