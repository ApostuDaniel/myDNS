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
char *address;
char *database;
char *ORIGIN;

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int main(int argc, char **argv)
{
  struct sockaddr_in server; // structura folosita de server
  struct sockaddr_in sender; //structura in care stocam adresa clientului sau a altui server
  int sd;                    //descriptorul de socket

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

  /* servim concurrent */
  while (1)
  {
    int msglen;
    int length = sizeof(sender);

    printf("[server]Astept la portul %d...\n", port);
    fflush(stdout);

    dnsresponse receivedQuery;
    bzero(&receivedQuery, sizeof(dnsquery));

    /* citirea mesajului primit de la client, sau de la un alt server */
    if ((msglen = recvfrom(sd, &receivedQuery, sizeof(dnsquery), 0, (struct sockaddr *)&sender, &length)) <= 0)
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
    case 0:
      ;
      sqlite3 *db; //our data base
      sqlite3_stmt *stmt;
      char sqlCommand[1012] = {0};
      char *FQDN = fullyQualifiedDomainName(receivedQuery.query.qName);
      sprintf(sqlCommand, "SELECT * FROM RR WHERE Type='%s' AND Name = '%s'", "NS", FQDN);
      free(FQDN);
      printf("Executing statement: %s", sqlCommand);

      int dbStatus = sqlite3_open(database, &db);

      if (dbStatus != SQLITE_OK)
      {
        fprintf(stderr, "Cannot open server database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
      }

      dbStatus = sqlite3_prepare_v2(db, sqlCommand, -1, &stmt, 0);
      if(dbStatus != SQLITE_OK){
        fprintf(stderr, "Failed to execute binding statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
      }
      // if (dbStatus == SQLITE_OK)
      // {
      //   int index = sqlite3_bind_parameter_index(stmt, "@name");
      //   char *FQDN = fullyQualifiedDomainName(receivedQuery.qName);
      //   printf("Searching for: %s\n", FQDN);
      //   sqlite3_bind_text(stmt, index, FQDN, -1, free);
      // }
      // else
      // {
      //   fprintf(stderr, "Failed to execute binding statement: %s\n", sqlite3_errmsg(db));
      //   sqlite3_close(db);
      //   return 1;
      // }

      dnsresponse queryResponse;
      bzero(&queryResponse, sizeof(queryResponse));
      queryAssign(&queryResponse.query, &receivedQuery.query);

      int step = sqlite3_step(stmt);

      if (step == SQLITE_ROW)
      {
        printf("Writing a response.\n");
        strcpy(queryResponse.name, sqlite3_column_text(stmt, 0));
        queryResponse.TTL = atoi(sqlite3_column_text(stmt, 1));
        queryResponse.Class = strcmp(sqlite3_column_text(stmt, 2), "IN") == 0 ? DNS_QCLASS_IN : DNS_QCLASS_NONE;
        queryResponse.Type = strcmp(sqlite3_column_text(stmt, 3), "NS") == 0 ? DNS_QTYPE_NS : DNS_QTYPE_NAPTR;
        queryResponse.DataLength = atoi(sqlite3_column_text(stmt, 5));
        strncpy(queryResponse.Data, sqlite3_column_text(stmt, 4), queryResponse.DataLength);
      }

      sqlite3_finalize(stmt);
      sqlite3_close(db);
      printf("[server]Trimitem mesajul inapoi...\n");

      /* returnam mesajul clientului */
      if (sendto(sd, &queryResponse, sizeof(dnsresponse), 0, (struct sockaddr *)&sender, length) <= 0)
      {
        perror("[server]Eroare la sendto() catre client.\n");
        continue; /* continuam sa ascultam */
      }
      else
        printf("[server]Mesajul a fost trasmis cu succes.\n");
      return 0;
    default:
      while (waitpid(-1, NULL, WNOHANG))
        ;
      continue;
    }
  } /* while */
} /* main */

bool configureServer(char **address, int *port, char **database, char** origin, char *configFile)
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