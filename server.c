#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <wait.h>
#include <stdbool.h>
#include <sqlite3.h>
#include "serverutils.h"
#include "dns.h"

int port;
char* address;
char* database;

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int main(int argc, char** argv)
{
  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in client;    //structura in care stocam adresa clientului
  struct sockaddr_in subdomain_server;  //structura pe care o folosim pentru a interoga un subdomeniu
  int sd;			//descriptorul de socket 

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }

  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&client, sizeof (client));
  bzero (&subdomain_server, sizeof (subdomain_server));

  if(argc < 2){
      printf ("[server]Syntax: %s <config_file>.\n", argv[0]);
      return errno;
  }

  if(configureServer(&address, &port, &database, argv[1]) == false){
        perror("Error when proccesing config file, program ends");
        return errno;
    }
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = inet_addr(address);
  /* utilizam un port utilizator */
    server.sin_port = htons (port);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  
  /* servim concurrent */
  while (1)
    {
      int msglen;
      int length = sizeof (client);

      printf ("[server]Astept la portul %d...\n",port);
      fflush (stdout);

      dnsquery receivedQuery;
      bzero(&receivedQuery, sizeof(dnsquery));
      
      /* citirea mesajului primit de la client */
      if ((msglen = recvfrom(sd, &receivedQuery, sizeof(dnsquery), 0,(struct sockaddr*) &client, &length)) <= 0)
      
	{
	  perror ("[server]Eroare la recvfrom() de la client.\n");
	  return errno;
	}

      printf ("[server]Mesajul a fost receptionat...\n");

        switch ((fork())) {
            case -1:
                perror("Eroare la fork");
                break;
            case 0:
                /*pregatim mesajul de raspuns */
                // bzero(msgrasp, 500);
                // strcat(msgrasp, receivedQuery.qName );
                // strcat(msgrasp, " - primit de la ");
                // strcat(msgrasp, inet_ntoa(client.sin_addr));
                // strcat(msgrasp, " port ");
                // char text[20];
                // sprintf(text, "%d", client.sin_port);
                // strcat(msgrasp, text);
                ;
                dnsresponse queryResponse;
                bzero(&queryResponse, sizeof(queryResponse));
                queryAssign(&queryResponse.query, &receivedQuery);
                queryResponse.Class = DNS_QCLASS_IN;
                strncpy(queryResponse.name, queryResponse.query.qName, strlen(queryResponse.query.qName));
                queryResponse.TTL = 50;
                strcpy(queryResponse.Data, "8.8.8.8 A google.com");

                printf("[server]Trimitem mesajul inapoi...\n");

                /* returnam mesajul clientului */
                if (sendto(sd, &queryResponse, sizeof(dnsresponse), 0, (struct sockaddr*) &client, length) <= 0)
                {
                    perror ("[server]Eroare la sendto() catre client.\n");
                    continue;		/* continuam sa ascultam */
                }
                else
                    printf ("[server]Mesajul a fost trasmis cu succes.\n");
                return 0;
            default:
                while(waitpid(-1, NULL, WNOHANG));
                continue;
        }
    }				/* while */
}				/* main */

bool configureServer(char** address, int* port, char** database, char* configFile){
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
    char* database_fld = "DB";

    while (fgets(buff, 255, fp) != NULL)
    {
        if(buff[strlen(buff) - 1] == '\n') buff[strlen(buff) - 1] = 0;

        if(strncmp(buff, addr_fld, strlen(addr_fld)) == 0){
            *address = strdup(buff + strlen(addr_fld) + 1);
        }
        else if(strncmp(buff, port_fld, strlen(port_fld)) == 0){
            *port = atoi(buff + strlen(port_fld) + 1);
        }
        else if(strncmp(buff, database_fld, strlen(database_fld)) == 0){
            *database = strdup(buff + strlen(database_fld) + 1);
        }
    }
    fclose(fp);

    if(*address == NULL || *port == 0) return false;

    return true;
}