#include <arpa/inet.h>
#include <bits/stdc++.h>
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

using namespace std;

#include "common.h"
#include "helpers.h"

void run_subscriber(int sockfd, char *sub_id) {
  // Send the subscriber ID to the server
  struct chat_packet request;
  request.len = 0;
  strcpy(request.source_id, sub_id);
  request.type = CONNECT;
  send_all(sockfd, &request, sizeof(request));

  // Multiplexing input from stdin and messages from server
  struct pollfd fds[2];

  // stdin -> keyboard input
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  // sockfd -> server message
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;

  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  while (1) {
    int rc = poll(fds, 2, -1);

    if (rc < 0) {
      perror("poll");
      break;
    }

    // Check for input from stdin
    if (fds[0].revents & POLLIN) {
      fgets(buf, sizeof(buf), stdin);

      // Parse the command, separating by spaces
      vector<string> tokens = parse_cmd(buf);

      // If the command is "exit", send a disconnect message to the server
      if (tokens[0] == "exit") {
        sent_packet.len = 0;
        memset(sent_packet.message, 0, MSG_MAXSIZE);
        sent_packet.type = DISCONNECT;
        strcpy(sent_packet.source_id, sub_id);
        send_all(sockfd, &sent_packet, sizeof(sent_packet));
        return;
      }

      if (tokens[0] == "subscribe") {
        // If the command is "subscribe", send a subscribe message to the server
        sent_packet.len = tokens[1].size();
        sent_packet.type = SUBSCRIBE;
        strcpy(sent_packet.source_id, sub_id);
        strcpy(sent_packet.message, tokens[1].c_str());
        send_all(sockfd, &sent_packet, sizeof(sent_packet));
        printf("Subscribed to topic %s\n", tokens[1].c_str());
      } else if (tokens[0] == "unsubscribe") {
        // If the command is "unsubscribe", send an unsubscribe message to the
        sent_packet.len = tokens[1].size();
        sent_packet.type = UNSUBSCRIBE;
        strcpy(sent_packet.source_id, sub_id);
        strcpy(sent_packet.message, tokens[1].c_str());
        send_all(sockfd, &sent_packet, sizeof(sent_packet));
        printf("Unsubscribed from topic %s\n", tokens[1].c_str());
      } else {
        fprintf(stderr, "Invalid command\n");
      }
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

  // Parse subscriber ID
  char *sub_id = argv[1];

  // Parse port as number
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Get the socket to communicate with the server
  const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // Save the server address, address family and port for connection
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);
  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Connect to the server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  // Run the subscriber subscriber
  run_subscriber(sockfd, sub_id);

  // Close the socket
  close(sockfd);

  return 0;
}
