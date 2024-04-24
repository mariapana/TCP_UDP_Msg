#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;

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

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;

  while (bytes_remaining > 0) {
    ssize_t result = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    if (result < 0) return -1;  // Error

    bytes_sent += result;
    bytes_remaining -= result;
  }

  return bytes_sent;
}
