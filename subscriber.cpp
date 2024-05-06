#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  // TCP_NODELAY
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

string get_int_msg(char *buffer, char *topic) {
  // Move to the next byte after reading the sign
  int8_t sign = *buffer++;

  // Convert from network to host byte order and assign
  int64_t number = ntohl(*(uint32_t *)buffer);

  // Apply sign
  if (sign) number = -number;

  string msg = "";
  msg.append(topic);
  msg.append(" - INT - ");
  msg.append(to_string(number));
  msg.append("\n");

  return msg;
}

string get_short_real_msg(char *buffer, char *topic) {
  // Convert, scale, and round to two decimal places
  float number = roundf(ntohs(*(uint16_t *)buffer) * 100.0f) / 10000.0f;

  std::ostringstream msg_stream;
  msg_stream << topic << " - SHORT_REAL - " << std::fixed
             << std::setprecision(2) << number << "\n";

  return msg_stream.str();
}

string get_float_msg(char *buffer, char *topic) {
  // Read sign and move buffer forward
  int8_t sign = *buffer++;

  // Convert the next 4 bytes and adjust pointer
  float number = ntohl(*(uint32_t *)buffer);
  buffer += sizeof(uint32_t);

  // Apply sign
  if (sign) number = -number;

  // Read the exponent for scaling
  int8_t exponent = *buffer;
  number = number / pow(10, exponent);

  string msg = "";
  msg.append(topic);
  msg.append(" - FLOAT - ");
  msg.append(to_string(number));
  msg.append("\n");

  return msg;
}

string get_string_msg(char *buffer, char *topic) {
  string msg = "";
  msg.append(topic);
  msg.append(" - STRING - ");
  msg.append(buffer);
  msg.append("\n");

  return msg;
}

void run_subscriber(int sockfd, char *sub_id) {
  // Send the subscriber ID to the server
  struct chat_packet request;
  request.len = 0;
  strcpy(request.source_id, sub_id);
  request.type = CONNECT;
  int rc = send_all(sockfd, &request, sizeof(request));
  DIE(rc <= 0, "send_all");

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

  while (1) {
    rc = poll(fds, 2, -1);

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
        rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
        DIE(rc <= 0, "send_all");
        return;
      }

      if (tokens[0] == "subscribe") {
        // If the command is "subscribe", send a subscribe message to the server
        sent_packet.len = tokens[1].size();
        sent_packet.type = SUBSCRIBE;
        strcpy(sent_packet.source_id, sub_id);
        strcpy(sent_packet.message, tokens[1].c_str());
        rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
        DIE(rc <= 0, "send_all");
        printf("Subscribed to topic %s\n", tokens[1].c_str());
      } else if (tokens[0] == "unsubscribe") {
        // If the command is "unsubscribe", send an unsubscribe message to the
        sent_packet.len = tokens[1].size();
        sent_packet.type = UNSUBSCRIBE;
        strcpy(sent_packet.source_id, sub_id);
        strcpy(sent_packet.message, tokens[1].c_str());
        rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
        DIE(rc <= 0, "send_all");
        printf("Unsubscribed from topic %s\n", tokens[1].c_str());
      } else {
        fprintf(stderr, "Invalid command\n");
      }
    }

    // Check for messages from server
    if (fds[1].revents & POLLIN) {
      //  Read IP and port
      char ip[IP_LEN];
      uint16_t port;

      int rc = recv_all(sockfd, &ip, sizeof(ip));
      DIE(rc <= 0, "recv_all");

      rc = recv_all(sockfd, &port, sizeof(port));
      DIE(rc <= 0, "recv_all");

      printf("%s:%d - ", ip, port);

      // Read message length and message
      int len;
      char buffer[BUFF];
      memset(buffer, 0, sizeof(buffer));

      rc = recv_all(sockfd, &len, sizeof(len));
      if (!rc) return;

      recv_all(sockfd, buffer, len);

      // Get topic
      char topic[MAX_TOPIC_LEN + 1];
      memset(topic, 0, sizeof(topic));
      memcpy(topic, buffer, MAX_TOPIC_LEN);
      char *buff = buffer;
      buff += MAX_TOPIC_LEN;

      // Get type
      char type = *buff;
      buff++;

      switch (type) {
        case INT:
          cout << get_int_msg(buff, topic);
          break;
        case SHORT_REAL:
          cout << get_short_real_msg(buff, topic);
          break;
        case FLOAT:
          cout << get_float_msg(buff, topic);
          break;
        case STRING:
          cout << get_string_msg(buff, topic);
          break;
        default:
          break;
      }
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

  // Enable the socket to reuse the address
  const int enable = 1;
  rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  DIE(rc < 0, "setsockopt SOL_SOCKET");

  // Disable Nagle's algorithm
  rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  DIE(rc < 0, "setsockopt IPPROTO_TCP");

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
