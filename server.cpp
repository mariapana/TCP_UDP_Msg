#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  // TCP_NODELAY
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

// Map topic: subscribers id list; unordered to preserve insertion order
unordered_map<string, vector<string>> topic_subscribers;
// Map id: subscriber
map<string, subscriber_t> id_subscriber;

// Vector for polling file descriptors
vector<pollfd> poll_fds;

void run_server(int num_sockets, int tcpfd, int udpfd) {
  struct chat_packet rcv_pkt;

  int rc;

  while (1) {
    // Wait for events on any of the sockets
    rc = poll(&poll_fds[0], num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == tcpfd) {
          // New TCP client connection
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          const int newsockfd =
              accept(tcpfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

          // Enable the socket to reuse the address
          const int enable = 1;
          rc = setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &enable,
                          sizeof(int));
          DIE(rc < 0, "setsockopt SOL_SOCKET");

          // Disable Nagle's algorithm
          rc = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable,
                          sizeof(int));
          DIE(rc < 0, "setsockopt IPPROTO_TCP");

          // Add new subscriber to the list
          struct subscriber_t new_subscriber;
          new_subscriber.sockfd = newsockfd;
          new_subscriber.ip = cli_addr.sin_addr.s_addr;
          new_subscriber.port = cli_addr.sin_port;
          new_subscriber.online = true;

          int rc = recv_all(newsockfd, &rcv_pkt, sizeof(rcv_pkt));
          DIE(rc < 0, "recv");

          char *sub_id = (char *)malloc(MAX_ID);
          strcpy(sub_id, rcv_pkt.source_id);

          if (id_subscriber.find(sub_id) != id_subscriber.end() &&
              id_subscriber[sub_id].online) {
            printf("Client %s already connected.\n", sub_id);
            close(newsockfd);
            continue;
          }

          new_subscriber.id = sub_id;

          // Add new subscriber to the list
          id_subscriber[sub_id] = new_subscriber;

          printf("New client %s connected from %s:%d\n", sub_id,
                 inet_ntoa(cli_addr.sin_addr),
                 ntohs(id_subscriber[sub_id].port));

          // Add new socket to the poll_fds vector
          poll_fds.push_back({newsockfd, POLLIN, 0});
          num_sockets++;
        } else if (poll_fds[i].fd == udpfd) {
          //  Buffer for the message
          char buffer[MAX_UDP_MSG_SIZE];
          memset(buffer, 0, sizeof(buffer));

          // Address of the UDP client
          struct sockaddr udp_cli_addr;
          socklen_t udp_cli_len = sizeof(udp_cli_addr);

          // Receive message from UDP client
          rc = recvfrom(poll_fds[i].fd, buffer, sizeof(buffer), 0,
                        &udp_cli_addr, &udp_cli_len);
          DIE(rc < 0, "recvfrom");

          // Construct message to forward
          struct message_t *msg = new struct message_t;
          msg->len = rc;
          msg->message = new char[msg->len];
          memcpy(msg->message, buffer, msg->len);
          strcpy(msg->ip,
                 inet_ntoa(((struct sockaddr_in *)&udp_cli_addr)->sin_addr));
          msg->port = ((struct sockaddr_in *)&udp_cli_addr)->sin_port;

          // Get topic
          char topic[MAX_TOPIC_LEN + 1];
          memcpy(topic, buffer, MAX_TOPIC_LEN);
          topic[MAX_TOPIC_LEN + 1] = '\0';

          // Get subscribers list for the topic
          vector<string> subscribed_clients = {};
          subscribed_clients = topic_subscribers[topic];

          for (string sub_id : subscribed_clients) {
            if (id_subscriber[sub_id].online) {
              rc = send_all(id_subscriber[sub_id].sockfd, &msg->ip,
                            sizeof(msg->ip));
              DIE(rc < 0, "send");
              rc = send_all(id_subscriber[sub_id].sockfd, &msg->port,
                            sizeof(msg->port));
              DIE(rc < 0, "send");
              rc = send_all(id_subscriber[sub_id].sockfd, &msg->len,
                            sizeof(msg->len));
              DIE(rc < 0, "send");
              rc = send_all(id_subscriber[sub_id].sockfd, msg->message,
                            msg->len);
              DIE(rc < 0, "send");
            }
          }
        } else if (poll_fds[i].fd == STDIN_FILENO) {
          char buf[MSG_MAXSIZE + 1];
          fgets(buf, sizeof(buf), stdin);

          // Parse the command, separating by spaces
          vector<string> tokens = parse_cmd(buf);

          // If the command is "exit", close all sockets
          if (tokens.size() == 1 && tokens[0] == "exit") {
            for (int j = 0; j < num_sockets; j++) {
              close(poll_fds[j].fd);
            }

            return;
          } else {
            fprintf(stderr, "Invalid command\n");
          }
        } else {
          // Received a message from a subscriber
          int rc = recv_all(poll_fds[i].fd, &rcv_pkt, sizeof(rcv_pkt));
          DIE(rc < 0, "recv");

          switch (rcv_pkt.type) {
            case SUBSCRIBE: {
              char sub_id[MAX_ID] = {0};
              strcpy(sub_id, rcv_pkt.source_id);
              char topic[MAX_TOPIC_LEN] = {0};
              strcpy(topic, rcv_pkt.message);

              // Add the subscriber to the list of subscribers for the topic
              topic_subscribers[(char *)topic].push_back(sub_id);

              break;
            }
            case UNSUBSCRIBE: {
              string sub_id = rcv_pkt.source_id;
              string topic = rcv_pkt.message;

              // Remove the subscriber from the list of subscribers for the
              // topic
              topic_subscribers[(char *)topic.c_str()].erase(
                  remove(topic_subscribers[(char *)topic.c_str()].begin(),
                         topic_subscribers[(char *)topic.c_str()].end(),
                         sub_id),
                  topic_subscribers[(char *)topic.c_str()].end());
              break;
            }
            case DISCONNECT: {
              char *sub_id = rcv_pkt.source_id;
              printf("Client %s disconnected.\n", sub_id);
              close(poll_fds[i].fd);

              // Remove fd from poll_fds
              poll_fds.erase(poll_fds.begin() + i);

              // Update the subscriber status
              id_subscriber[sub_id].online = false;

              num_sockets--;
              break;
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "\n Usage: %s <PORT_SERVER>\n", argv[0]);
    return 1;
  }

  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  int rc;

  // Parse port as number
  uint16_t port;
  rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Get TCP socket for communication with subscribers
  const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcpfd < 0, "Socket TCP failed");

  // Get UDP socket for receiving messages to send further
  const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udpfd < 0, "Socket UDP failed");

  // Save the server address, address family and port for connection
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);
  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind the server address to the sockets
  rc = bind(tcpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind tcp");
  rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind udp");

  // Add the sockets to the poll_fds vector
  poll_fds.push_back({tcpfd, POLLIN, 0});
  poll_fds.push_back({udpfd, POLLIN, 0});
  poll_fds.push_back({STDIN_FILENO, POLLIN, 0});

  int num_sockets = poll_fds.size();

  // tcpfd for listening
  rc = listen(tcpfd, INT_MAX);
  DIE(rc < 0, "listen");

  // Run the server
  run_server(num_sockets, tcpfd, udpfd);

  // Close the socket
  close(tcpfd);

  return 0;
}
