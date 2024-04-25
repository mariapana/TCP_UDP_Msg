#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd, char *client_id) {
  // Trimitem ID-ul clientului la server, semnaland cererea de conexiune
  struct chat_packet request;
  request.len = strlen(client_id) + 1;
  strcpy(request.message, client_id);
  send_all(sockfd, &request, sizeof(request));

  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
  DIE(rc <= 0, "recv_all");

  printf("%s\n", recv_packet.message);

  /*
    Multiplexam intre citirea de la tastatura si primirea unui
    mesaj, ca sa nu mai fie impusa ordinea.
  */
  struct pollfd fds[2];

  // stdin -> input de la tastatura
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  // sockfd -> mesaj de la server
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;

  while (1) {
    int rc = poll(fds, 2, -1);

    if (rc < 0) {
      perror("poll");
      break;
    }

    // Input de la tastatura
    if (fds[0].revents & POLLIN) {
      // EOF sau space
      if (fgets(buf, sizeof(buf), stdin) == NULL || isspace(buf[0])) break;

      memset(&sent_packet, 0, sizeof(sent_packet));
      sent_packet.len = strlen(buf) + 1;
      strncpy(sent_packet.message, buf, MSG_MAXSIZE);

      // Send packet to server
      send_all(sockfd, &sent_packet, sizeof(sent_packet));
    }

    // Check for messages from server
    if (fds[1].revents & POLLIN) {
      int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
      if (rc <= 0) break;

      printf("%s\n", recv_packet.message);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("\n Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
    return 1;
  }

  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  // Parsam ID-ul clientului
  char *client_id = argv[1];

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru conectarea la server
  const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Ne conectăm la server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  run_client(sockfd, client_id);

  // Inchidem conexiunea si socketul creat
  close(sockfd);

  return 0;
}
