#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

// Map pentru stocarea IP-urilor si porturilor clientilor
map<int, pair<in_addr, uint16_t>> client_ip_port;

// Vector de pollfd pentru multiplexare
struct pollfd poll_fds[MAX_CONNECTIONS];

void run_server(int num_sockets, int tcpfd, int udpfd) {
  struct chat_packet received_packet;

  int rc;

  while (1) {
    // Asteptam sa primim ceva pe unul dintre cei num_sockets socketi
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == tcpfd) {
          // Am primit o cerere de conexiune pe socketul tcp (de la un
          // nou client) pe care o acceptam
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          const int newsockfd =
              accept(tcpfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

          // Retinem IP-ul si portul clientului nou
          client_ip_port[num_sockets] = {cli_addr.sin_addr, cli_addr.sin_port};

          // Adaugam noul socket intors de accept() la multimea descriptorilor
          // de citire
          poll_fds[num_sockets].fd = newsockfd;
          poll_fds[num_sockets].events = POLLIN;
          num_sockets++;

          // Conexiunea a fost efectuata, urmeaza sa primim ID-ul clientului
        } else if (poll_fds[i].fd == udpfd) {
        } else if (poll_fds[i].fd == STDIN_FILENO) {
          char buf[MSG_MAXSIZE + 1];
          fgets(buf, sizeof(buf), stdin);

          // Parsam comanda, separand la spatii
          int num_tokens = 0;
          vector<string> tokens;
          char *token = strtok(buf, " \n");
          while (token != NULL) {
            tokens.push_back(token);
            token = strtok(NULL, " \n");
            num_tokens++;
          }

          // Comanda de tip exit
          if (num_tokens++ && tokens[0] == "exit") {
            // Inchidem toate socketii
            for (int j = 1; j < num_sockets; j++) {
              close(poll_fds[j].fd);
            }

            return;
          }

        } else {
          // Am primit date pe unul din socketii de client, asa ca le
          // receptionam
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "recv");

          if (rc == 0) {
            printf("Client %d disconnected\n", i);
            close(poll_fds[i].fd);

            // Scoatem din multimea de citire socketul inchis
            for (int j = i; j < num_sockets - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];
            }

            num_sockets--;
          } else {
            // printf("S-a primit de la clientul de pe socketul %d mesajul:
            // %s\n",
            //        poll_fds[i].fd, received_packet.message);
            // // Trimite mesajul catre toti ceilalti clienti
            // for (int j = 1; j < num_sockets; j++) {
            //   if (poll_fds[j].fd != poll_fds[i].fd) {
            //     rc = send_all(poll_fds[j].fd, &received_packet,
            //                   sizeof(received_packet));
            //     DIE(rc < 0, "send");
            //   }
            // }

            // Primul mesaj primit de la un client este ID-ul acestuia
            char *client_id = received_packet.message;

            printf("New client %s connected from %s:%d\n", client_id,
                   inet_ntoa(client_ip_port[i].first),
                   ntohs(client_ip_port[i].second));
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("\n Usage: %s <PORT_SERVER>\n", argv[0]);
    return 1;
  }

  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  int rc;

  // Parsam port-ul ca un numar
  uint16_t port;
  rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru receptionarea conexiunilor
  const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcpfd < 0, "socket tcp");

  // Obtinem un socket UDP pentru receptionarea mesajelor de transmis
  const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udpfd < 0, "socket udp");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Asociem adresa serverului cu socketul creat folosind bind
  rc = bind(tcpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind tcp");

  rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind udp");

  // Adaugam socketii la multimea de descriptori
  poll_fds[0].fd = tcpfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = udpfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  int num_sockets = 3;

  // Setam socket-ul tcpfd pentru ascultare
  rc = listen(tcpfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // Rulam serverul
  run_server(num_sockets, tcpfd, udpfd);

  // Inchidem socketul de listen
  close(tcpfd);

  return 0;
}
