#ifndef __COMMON_H__
#define __COMMON_H__

#include <bits/stdc++.h>
#include <stddef.h>
#include <stdint.h>

using namespace std;

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);
vector<string> parse_cmd(char *buf);

// Maximum message size
#define MSG_MAXSIZE 1024

// Maximum ID size
#define MAX_ID 10

// Message types
#define CONNECT 1
#define SUBSCRIBE 2
#define UNSUBSCRIBE 3
#define DISCONNECT 4

// Packet structure
struct chat_packet {
  uint16_t len;
  uint16_t type;
  char source_id[MAX_ID];
  char message[MSG_MAXSIZE + 1];
};

// subscriber structure
struct subscriber_t {
  int sockfd;
  char *id;
  bool online;
  uint16_t port;
  uint32_t ip;
};

#endif
