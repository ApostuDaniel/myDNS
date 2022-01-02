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
#include <sys/mman.h>
#include <time.h>
#include "dns.h"
#include "serverutils.h"

/* codul de eroare returnat de anumite apeluri */
extern int errno;

char *address;
char *database;
char *ORIGIN;
int port;

int main(int argc, char **argv)
{
  struct sockaddr_in server; // structura folosita de server
  struct sockaddr_in sender; //structura in care stocam adresa clientului sau a altui server
  int sd, nrOfClients = 10;  //descriptorul de socket
  address_queryID_pair *clients;

  /* crearea unui socket */
  if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }

  /* pregatirea structurilor de date */
  bzero(&server, sizeof(server));
  bzero(&sender, sizeof(sender));

  if (argc < 2)
  {
    printf("[server]Syntax: %s <config_file>.\n", argv[0]);
    return errno;
  }

  if (configureServer(&address, &port, &database, &ORIGIN, argv[1]) == false)
  {
    perror("Error when proccesing config file, program ends");
    return errno;
  }

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
  server.sin_family = AF_INET;
  /* acceptam orice adresa */
  server.sin_addr.s_addr = inet_addr(address);
  /* utilizam un port utilizator */
  server.sin_port = htons(port);

  /* atasam socketul */
  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server]Eroare la bind().\n");
    free(address);
    free(database);
    return errno;
  }

  clients = createSharedClientsMapping(nrOfClients);

  /* servim concurrent */
  while (1)
  {
    int msglen;
    int length = sizeof(sender);

    printf("[server]Astept la portul %d...\n", port);
    fflush(stdout);

    dnsresponse receivedQuery;
    bzero(&receivedQuery, sizeof(dnsresponse));

    /* citirea mesajului primit de la client, sau de la un alt server */
    if ((msglen = recvfrom(sd, &receivedQuery, sizeof(dnsresponse), 0, (struct sockaddr *)&sender, &length)) <= 0)
    {
      perror("[server]Eroare la recvfrom() de la client.\n");
      free(address);
      free(database);
      return errno;
    }

    printf("[server]Mesajul a fost receptionat...\n");

    switch ((fork()))
    {
    case -1:
      perror("Eroare la fork");
      break;
    case 0:;
      sqlite3 *db; //our data base
      sqlite3_stmt *stmt;
      char sqlCommand[1012] = {0};
      int dbStatus = sqlite3_open(database, &db);

      if (dbStatus != SQLITE_OK)
      {
        fprintf(stderr, "Cannot open server database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
      }

      if (receivedQuery.query.Header.qr == 1)
      {
        printf("[server] Am primit un raspuns pentru %s incerc sa-l transmit...\n", receivedQuery.name);
        struct sockaddr_in client;
        if (!addToCache(&receivedQuery, &db, &stmt, sqlCommand))
        {
          fprintf(stderr, "Failed to add data received to cache\n");
        }

        int clientIndex = lookupClientForQuery(receivedQuery.query.Header.id, &client, clients, nrOfClients);
        clients[clientIndex].query_id = RESERVED_ID;
        printf("[server] Transmitem raspunsul lui %s:%d\n",inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        if (sendto(sd, &receivedQuery, sizeof(dnsresponse), 0, (struct sockaddr *)&client, length) <= 0)
        {
          perror("[server]Eroare la sendto() catre client.\n");
          free(address);
          free(ORIGIN);
          free(database);
          sqlite3_close(db);
          return 1;
        }
        printf("[server]Sent the response received further up the chain...\n");
      }
      else
      {
        printf("[server] Am primit o intrebare pentru %s incerc sa raspund..\n", receivedQuery.query.qName);
        bool foundAnswer;
        dnsresponse queryResponse;
        bzero(&queryResponse, sizeof(queryResponse));
        queryAssign(&queryResponse.query, &receivedQuery.query);

        
        if (!(foundAnswer = getAnswerFromCache(&queryResponse, &db, &stmt, sqlCommand)))
        {
          foundAnswer = getAnswerFromRR(&queryResponse, &db, &stmt, sqlCommand);
        }

        if (foundAnswer)
        {
          if (sendto(sd, &queryResponse, sizeof(dnsresponse), 0, (struct sockaddr *)&sender, length) <= 0)
          {
            perror("[server]Eroare la sendto() catre client.\n");
          }
          else
            printf("[server]Mesajul a fost trasmis cu succes.\n");
        }
        else
        {
          struct sockaddr_in nextServer;
          bool nameServerExists = getNameServerFromRR(&receivedQuery, &nextServer, &db, &stmt, sqlCommand);
          if (nameServerExists)
          {
            int senderIndex = addClientToSenderList(receivedQuery.query.Header.id, &sender, clients, nrOfClients);

            if (sendto(sd, &receivedQuery, sizeof(dnsresponse), 0, (struct sockaddr *)&nextServer, length) <= 0)
            {
              perror("[server]Eroare la sendto() catre alt server.\n");
            }
            else
            {
              printf("[server]Mesajul a fost trasmis in jos pe arbore catre [%d].\n", ntohs(nextServer.sin_port));
            }
          }
          else
          {
            printf("[server]Domeniu de negasit, trimit NXDOMAIN...\n");
            queryResponse.query.Header.rcode = DNS_RCODE_NXDOMAIN;
            queryResponse.query.Header.ancount = 0;
            queryResponse.query.Header.qr = 1;
            char* FQDN = fullyQualifiedDomainName(queryResponse.query.qName);
            strcpy(queryResponse.name, FQDN);
            free(FQDN);
            if (sendto(sd, &queryResponse, sizeof(dnsresponse), 0, (struct sockaddr *)&sender, length) <= 0)
            {
              perror("[server]Eroare la sendto() catre client.\n");
            }
            else
            {
              printf("[server]Mesajul a fost trasmis cu succes.\n");
            }
          }
        }

        printf("[server] Acest copil a terminat...\n");
        sqlite3_close(db);
        free(address);
        free(ORIGIN);
        free(database);
        return 0;
      }
    default:
      while (waitpid(-1, NULL, WNOHANG))
        ;
      continue;
    }
  } /* while */
} /* main */

bool configureServer(char **address, int *port, char **database, char **origin, char *configFile)
{
  FILE *fp;
  char buff[256] = {0};
  *address = NULL;
  *port = 0;
  fp = fopen(configFile, "r");

  if (fp == NULL)
  {
    perror("Cannot open config file, program ends");
    return false;
  }

  while (fgets(buff, 255, fp) != NULL)
  {
    if (buff[strlen(buff) - 1] == '\n')
      buff[strlen(buff) - 1] = 0;

    if (strncmp(buff, ADDR_FLD, strlen(ADDR_FLD)) == 0)
    {
      *address = strdup(buff + strlen(ADDR_FLD) + 1);
    }
    else if (strncmp(buff, PORT_FLD, strlen(PORT_FLD)) == 0)
    {
      *port = atoi(buff + strlen(PORT_FLD) + 1);
    }
    else if (strncmp(buff, DATABASE_FLD, strlen(DATABASE_FLD)) == 0)
    {
      *database = strdup(buff + strlen(DATABASE_FLD) + 1);
    }
    else if (strncmp(buff, ORIGIN_FLD, strlen(ORIGIN_FLD)) == 0)
    {
      *origin = strdup(buff + strlen(ORIGIN_FLD) + 1);
    }
  }
  fclose(fp);

  if (*address == NULL || *port == 0 || *database == NULL || *origin == NULL)
  {
    free(*address);
    free(*database);
    free(*origin);
    return false;
  }

  return true;
}

char *fullyQualifiedDomainName(char *queryDomain)
{
  int len = strlen(queryDomain);
  char *FQDN = (char *)malloc(len);
  int start = 0;
  int count;
  int currentPos = 0;
  while (start < len)
  {
    count = queryDomain[start];
    for (size_t i = start + 1; i < start + 1 + count; i++)
    {
      FQDN[currentPos++] = queryDomain[i];
    }
    FQDN[currentPos++] = '.';
    start = start + 1 + count;
  }
  FQDN[currentPos] = 0;
  return FQDN;
}

address_queryID_pair *createSharedClientsMapping(int nrOfClients)
{
  int protection = PROT_READ | PROT_WRITE;
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  address_queryID_pair *memory = (address_queryID_pair *)mmap(NULL, nrOfClients * sizeof(address_queryID_pair), protection, visibility, -1, 0);
  bzero(memory, nrOfClients * sizeof(address_queryID_pair));

  for (size_t i = 0; i < nrOfClients; i++)
  {
    memory[i].query_id = RESERVED_ID;
  }
  
  return memory;
}

int lookupClientForQuery(uint16_t id, struct sockaddr_in *client, address_queryID_pair *clientList, int clientListSize)
{
  for (size_t i = 0; i < clientListSize; i++)
  {
    if (clientList[i].query_id == id)
    {
      *client = clientList[i].address;
      return i;
    }
  }

  return -1;
}

int addClientToSenderList(uint16_t id, struct sockaddr_in *client, address_queryID_pair *clientList, int clientListSize)
{
  while (true)
  {
    for (size_t i = 0; i < clientListSize; i++)
    {
      if (clientList[i].query_id == RESERVED_ID)
      {
        clientList[i].query_id = id;
        clientList[i].address = *client;
        return i;
      }
    }
  }
}

void fillResponse(dnsresponse *queryResponse, sqlite3_stmt **stmt, bool isFromCache)
{
  if (isFromCache)
    strcpy(queryResponse->name, sqlite3_column_text(*stmt, 0));
  else
  {
    strcat(queryResponse->name, sqlite3_column_text(*stmt, 0));
    strcat(queryResponse->name, ".");
    strcat(queryResponse->name, ORIGIN);
  }
  queryResponse->TTL = sqlite3_column_int(*stmt, 1);
  queryResponse->Class = sqlite3_column_int(*stmt, 2);
  queryResponse->Type = sqlite3_column_int(*stmt, 2);
  queryResponse->DataLength = sqlite3_column_int(*stmt, 5);
  strncpy(queryResponse->Data, sqlite3_column_text(*stmt, 4), queryResponse->DataLength);
  queryResponse->query.Header.qr = 1;
  queryResponse->query.Header.ancount = 1;
  queryResponse->query.Header.rcode = DNS_RCODE_NOERROR;
}

bool addToCache(dnsresponse *data, sqlite3 **db, sqlite3_stmt **stmt, char *buff)
{
  bool successState = false;
  time_t now = time(0);
  sprintf(buff, "INSERT INTO Cache VALUES('%s', %d, %d, %d, '%s', %d, %ld)", data->name, data->TTL, data->Class, data->Type, data->Data, data->DataLength, now);
  int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
  if (dbStatus != SQLITE_OK)
  {
    fprintf(stderr, "Failed statement preapration: %s\n", sqlite3_errmsg(*db));
    return successState;
  }

  int step = sqlite3_step(*stmt);
  if (step == SQLITE_DONE)
  {
    successState = true;
    printf("[server]Raspuns adaugat in cache...\n");
  }

  sqlite3_finalize(*stmt);

  refreshCache(db, stmt, buff);

  return successState;
}

bool refreshCache(sqlite3 **db, sqlite3_stmt **stmt, char *buff)
{
  bool successState = false;
  time_t now = time(0);
  sprintf(buff, "DELETE FROM Cache WHERE %ld > (TTL + DateAdded)", now);
  int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
  if (dbStatus != SQLITE_OK)
  {
    fprintf(stderr, "Failed statement preapration: %s\n", sqlite3_errmsg(*db));
    return successState;
  }

  int step = sqlite3_step(*stmt);
  if (step == SQLITE_DONE)
  {
    successState = true;
    printf("[server]Cache refresed...\n");
  }

  sqlite3_finalize(*stmt);

  return successState;
}

bool getAnswerFromCache(dnsresponse *data, sqlite3 **db, sqlite3_stmt **stmt, char *buff)
{
  bool successState = false;
  char *FQDN = fullyQualifiedDomainName(data->query.qName);
  sprintf(buff, "SELECT * FROM Cache WHERE Name = '%s' AND Type = %d AND Class = %d", FQDN, data->query.QType, data->query.QClass);
  free(FQDN);
  int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
  if (dbStatus != SQLITE_OK)
  {
    fprintf(stderr, "Failed statement preapration: %s\n", sqlite3_errmsg(*db));
    return successState;
  }

  int step = sqlite3_step(*stmt);
  if (step == SQLITE_ROW)
  {
    successState = true;
    fillResponse(data, stmt, true);
    printf("[server]Am gasit raspunsul in cache...\n");
  }
  else printf("[server]Raspunsul nu e in cache...\n");

  sqlite3_finalize(*stmt);

  if (successState)
  {
    time_t now = time(0);
    sprintf(buff, "UPDATE Cache SET DateAdded = %ld WHERE Name = '%s' AND Type = %d AND Class = %d", now, data->name, data->Type, data->Class);

    int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
    if (dbStatus != SQLITE_OK)
    {
      fprintf(stderr, "Failed statement preapration: %s\n", sqlite3_errmsg(*db));
    }
    else
    {
      step = sqlite3_step(*stmt);
      if (step == SQLITE_DONE)
      {
        printf("[server]Cache updated successfuly...\n");
      }
      else  printf("[server]Could not update cache...\n");
    }
  }

  refreshCache(db, stmt, buff);

  return successState;
}

bool getAnswerFromRR(dnsresponse *data, sqlite3 **db, sqlite3_stmt **stmt, char *buff)
{
  bool successState = false;
  char *FQDN = fullyQualifiedDomainName(data->query.qName);
  sprintf(buff, "SELECT * FROM RR WHERE Name || '.' || '%s' = '%s' AND Type = %d AND Class = %d", ORIGIN, FQDN, data->query.QType, data->query.QClass);
  free(FQDN);
  int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
  if (dbStatus != SQLITE_OK)
  {
    fprintf(stderr, "Failed statement preapration: %s\n", sqlite3_errmsg(*db));
    return successState;
  }

  int step = sqlite3_step(*stmt);
  if (step == SQLITE_ROW)
  {
    successState = true;
    fillResponse(data, stmt, false);
    printf("[server]Am gasit raspunsul in resource records...\n");
  } else printf("[server]Nu am gasit raspunsul in resource records...\n");

  sqlite3_finalize(*stmt);

  if (successState)
  {
    addToCache(data, db, stmt, buff);
  }
  return successState;
}

bool getNameServerFromRR(dnsresponse *data, struct sockaddr_in *next, sqlite3 **db, sqlite3_stmt **stmt, char *buff)
{
  bool successState = false;
  char *FQDN = fullyQualifiedDomainName(data->query.qName);
  sprintf(buff, "SELECT * FROM RR WHERE Type = 2 AND Class = 1 AND  instr('%s', Name) - 1 + length(Name) = %d", FQDN, (int)strlen(FQDN));
  free(FQDN);
  int dbStatus = sqlite3_prepare_v2(*db, buff, -1, stmt, 0);
  if (dbStatus != SQLITE_OK)
  {
    fprintf(stderr, "Failed statement preparation: %s\n", sqlite3_errmsg(*db));
    return successState;
  }
  printf("Interogating for name server\n");
  int step = sqlite3_step(*stmt);
  if (step == SQLITE_ROW)
  {
    successState = true;
    fillAddress(next, stmt);
    printf("[server]am gasit un name server...\n");
  } else printf("[server] nu am gasit nici un name server...\n");

  sqlite3_finalize(*stmt);

  return successState;
}

void fillAddress(struct sockaddr_in *next, sqlite3_stmt **stmt)
{
  char NSData[75] = {0};
  char address[50] = {0};
  char port[25] = {0};
  strncpy(NSData, sqlite3_column_text(*stmt, 4), sqlite3_column_int(*stmt, 5));
  char* portIndex = strchr(NSData, ':');
  strncpy(address, NSData, (int)(portIndex - NSData));
  strncpy(port, portIndex + 1, strlen(portIndex + 1));
  next->sin_family = AF_INET;
  next->sin_addr.s_addr = inet_addr(address);
  next->sin_port = htons(atoi(port));
}