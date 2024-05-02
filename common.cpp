#include "common.h"

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace std;

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while (bytes_remaining > 0) {
    size_t result = recv(sockfd, buff + bytes_received, bytes_remaining, 0);

    if (result == 0)
      break;  // Connection closed
    else if (result < 0)
      return -1;  // Error

    bytes_received += result;
    bytes_remaining -= result;
  }

  return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while (bytes_remaining > 0) {
    ssize_t result = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    if (result < 0) return -1;  // Error

    bytes_sent += result;
    bytes_remaining -= result;
  }

  return bytes_sent;
}

vector<string> parse_cmd(char *buf) {
  vector<string> tokens;
  char *token = strtok(buf, " \n");
  while (token != NULL) {
    tokens.push_back(token);
    token = strtok(NULL, " \n");
  }
  return tokens;
}
