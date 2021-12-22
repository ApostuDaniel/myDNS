#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "resolverUtils.h"
#include "dns.h"

int port;
char* address;
extern int errno;

int main(int argc, char* argv[]){
    int sd;
    FILE* fp;
    char request[257] = {0};
    struct sockaddr_in server;
    dnsquery query;
    dnsresponse response;
    int length=sizeof(server), msglen;
    srandom(((unsigned int)time(NULL) + (unsigned int)getpid() * 100));

//get server address and port from config file
    if(configureResolver(&address, &port, CONFIG) == false){
        perror("Error when proccesing config file, program ends");
        return errno;
    }

//setup server address structure
    server.sin_family = AF_INET;
    server.sin_port = port;
    server.sin_addr.s_addr = inet_addr(address);
    server.sin_port = htons (port);

    if ((sd = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }



    if(argc >= 2){
        if(argc > 3){
            printf("Syntax:%s [<domain>] [<output_file>]\n", argv[0]);
            free(address);
            exit(1);
        }

        strncpy(request, argv[1], 256);
        if(argc == 2) fp = stdout;
        else fp = fopen(argv[2], "w");

        if(!processInput(request)){
            printf("Invalid input, be sure that the input is a valid domain name\n");
            free(address);
            exit(1);
        }
        else{

            query = createQuery(request);
            
            if (sendto (sd, &query, sizeof(dnsquery), 0, (struct sockaddr*)&server, length) <= 0)
                {
                perror ("[client]Eroare la sendto() spre server.\n");
                return errno;
                }

            bzero(&response, sizeof(dnsresponse));

            if ( (msglen=recvfrom (sd, &response, sizeof(dnsresponse),0,(struct sockaddr*)&server, &length)) < 0)
            {
            perror ("[client]Eroare la recvfrom() de la server.\n");
            return errno;
            }

            fprintf(fp, "Query Id: %d   Response: %s\n",response.query.Header.id, response.Data);
            printf("Wrote to output\n");
        }

        if(fp != stdout) fclose(fp);
    }
    else{
        while(true){
            printf("The domain you want to search, or exit to end the client: ");
            scanf("%s", request);

            if(strcmp("exit", request) == 0) break;

            if(!processInput(request)){
                printf("Invalid input, be sure that the input is a valid domain name, please retry.\n");
            }
            else{
               query = createQuery(request);
            
                if (sendto (sd, &query, sizeof(dnsquery), 0, (struct sockaddr*)&server, length) <= 0)
                    {
                    perror ("[client]Eroare la sendto() spre server.\n");
                    return errno;
                    }

                bzero(&response, sizeof(dnsresponse));

                if ( (msglen=recvfrom (sd, &response, sizeof(dnsresponse),0,(struct sockaddr*)&server, &length)) < 0)
                {
                perror ("[client]Eroare la recvfrom() de la server.\n");
                return errno;
                }

                printf("Query Id: %d   Response: %s\n",response.query.Header.id, response.Data);
            }
        }
    }

    close(sd);
    free(address);
    return 0;
}

bool configureResolver(char** address, int* port, char* configFile){
    FILE* fp;
    char buff[256] = {0};
    *address = NULL;
    *port = 0;
    fp = fopen(configFile, "r");

    if(fp == NULL){
        perror("Cannot open config file, program ends");
        return false;
    }

    char* addr_fld = "Address";
    char* port_fld = "Port";

    while (fgets(buff, 255, fp) != NULL)
    {
        if(buff[strlen(buff) - 1] == '\n') buff[strlen(buff) - 1] = 0;

        if(strncmp(buff, addr_fld, strlen(addr_fld)) == 0){
            *address = strdup(buff + strlen(addr_fld) + 1);
        }
        else if(strncmp(buff, port_fld, strlen(port_fld)) == 0){
            *port = atoi(buff + strlen(port_fld) + 1);
        }
    }

    fclose(fp);

    if(*address == NULL || *port == 0) return false;

    return true;
}

bool processInput(char* domainName)
{
    //Check that the domainName length fits our criteria
    int len = strlen(domainName);
    if(len > 255){printf("Domain to long\n"); return false;}
    
    const char permitedChars[] = "abcdefghijklmnopqrstuvwxyz1234567890.-";
    
    //tranform each letter to uppercase
    for(int i = 0; i < len; i++){
        if(domainName[i] >= 'A' && domainName[i] <= 'Z'){
            domainName[i] = domainName[i] + 32;
        }
        //check that we only have valid charchters in the domain
        if(strchr(permitedChars, domainName[i]) == NULL) {printf("Invalid charachter %c in domain at index %d\n", domainName[i], i); return false;}
    }
    if(domainName[len - 1] == '.') domainName[len - 1] = 0;
    
    
    char delim[] = ".";
    char copy[255] = {0};
    
    strncpy(copy, domainName, strlen(domainName));
    printf("%s\n", copy);
    
    //split the domain into labels
    char *ptr = strtok(copy, delim);

    //check that each label has less than 63 charchters, and that they don't start or end with '-'
    while(ptr != NULL)
    {
        if(ptr[0] == '-' || ptr[strlen(ptr)-1] == '-') {printf("charchter - at begining or end of label\n");return false;}
        if(strlen(ptr) > 63) {printf("label longer than 63 charachters\n");return false;}
        ptr = strtok(NULL, delim);
    }
    
    return true;
}

/* 
	Function to change url to dns format
	For example: www.google.com would become:
	3www6google3com0
	size, can be used if you want to know the size of the returned pointer,
*/

unsigned char* dns_format ( char *url, int *size)
{
	int i, count = 0, replace_point = 0, len = strlen(url);
	char *buf = (char *) malloc(len+79);

    for(i = 0; i < len; ++i){
        if(url[i] == '.'){
            buf[replace_point] = count;
            replace_point = replace_point + count + 1;
            count = 0;
        }
        else{
            ++count;
            buf[replace_point + count] = url[i];
        }
    }

    buf[replace_point] = count;
    buf[replace_point + count + 1] = 0;
	
	if(size) *size = len+79;
	return buf;
}

dnsquery createQuery(char* domainName){
    dnsquery query;
    int size;
    unsigned char* dns_format_domain = dns_format(domainName, &size);

    strncpy(query.qName, dns_format_domain, 264);
    free(dns_format_domain);
    query.QClass = DNS_QCLASS_IN;
    query.QType = DNS_QTYPE_A;
    query.Header.qr = 0;
    query.Header.id = random() % 65536;
    query.Header.opcode = 0;
    query.Header.aa = 0;
    query.Header.adcount = 0;
    query.Header.ancount = 0;
    query.Header.nscount = 0;
    query.Header.qcount = 1;
    query.Header.qr = 0;
    query.Header.ra = 1;
    query.Header.rd = 1;
    query.Header.rcode = 0;
    query.Header.tc = 0;
    query.Header.zero = 0;

    return query;
}