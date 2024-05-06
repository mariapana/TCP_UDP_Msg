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

// Maximum topic length
#define MAX_TOPIC_LEN 50

// Maximum UDP message size
#define MAX_UDP_MSG_SIZE 1551
#define BUFF 1570

// IP length
#define IP_LEN 16

// TCP message types
#define CONNECT 1
#define SUBSCRIBE 2
#define UNSUBSCRIBE 3
#define DISCONNECT 4

// UDP message types
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

// Packet structure
struct chat_packet {
  uint16_t len;
  uint16_t type;
  char source_id[MAX_ID];
  char message[MSG_MAXSIZE + 1];
};

// Subscriber structure
struct subscriber_t {
  int sockfd;
  char *id;
  bool online;
  uint16_t port;
  uint32_t ip;
};

// UDP message structure
struct udp_msg_t {
  char topic[50];
  char type;
  char msg_content[1500];
};

// Message structure
struct message_t {
  int len;
  char *message;
  char ip[IP_LEN];
  uint16_t port;
} __attribute__((packed));

#endif
