##### Copyright Maria Pana 325CA <maria.pana4@gmail.com> 2023-2024

# TCP & UDP Client-Server App for Managing Messages

## Description
The aim of the project is to implement a client-server application that leverages TCP and UDP protocols in order to manage message transmission. The server acts as a broker between subscribers (TCP Clients) and content sources (UDP Clients). This is done by managing subscriptions and broadcasting messages to online clients based on their subscriptions.

It is important to note that since we want the commands and messages sent by the application to be executed/received immediately, the program disables Nagle's algorithm. If there are multiple messages to be sent in a short period of time, they will be joined into a singular large message (`send_all`, `recv_all` in `common.cpp`).

Note: The project is based on the implementation used in the 7th laboratory.    

## Structures

* `subscriber_t`: models the structure of a subscriber, containing the file descriptor it uses, its ID, the connection status (online/offline) and its port and IP address.

* `chat_packet`: used for interactions between the TCP Client and the Server (e.g. connect, subscribe, unsubscribe, exit).

* `udp_message_t` and `message_t`: used for interactions between the UDP Client and the Server i.e. forwarding the message from the UDP Client to the TCP Client via the Server.

## Components

### Server
The server is responsible for managing the messages (related either to the topics or the actions of the subscribers). It opens two sockets, one for TCP (used for the subscribers) and one for UDP (the source of the topic messages which will be forwarded to the subscribed clients), and listens for incoming connections. The server can handle multiple clients at the same time.

#### Commands

To start the server:
```bash
./server <PORT>
```

To stop the server (including all connected clients):
```
exit
```

#### Flow
1. *Initialization*: Start by initializing server-specific variables and data structures: socket file descriptors, a vector for polling file descriptors, a map that retains the ID's of the subscribed clients for each topic, and a map that retains the actual client structures, based on their ID.
2. *Configuration*: Set up the server by parsing command-line arguments, creating TCP and UDP sockets, and binding them to the specified port and address. Add the TCP socket, UDP socket and STDIN to the polling vector.
3. *Run the server loop*: Wait for events on the sockets using `poll()`. There are several scenarios to handle:
    - *New TCP client* (TCP Socket event): Accept the connection, set socket options (SO_REUSEADDR, TCP_NODELAY), receive client identification, manage new or duplicate connections and register the new client to the map and polling vector.
    - *New UDP message*: Receive the message, parse the topic from the message, check subscribers for the topic and relay the message to each, ensuring each subscriber is online and notified only once (`notified_subscribers`).
    - *TCP client message*: Handle data received from subscribers (subscribe/unsubscribe requests, disconnection notices).
    - *STDIN command*: Read the command and process it accordingly (exit).
4. *Cleanup and exit*: After exiting the loop on receiving the "exit" command, close all sockets and clean up resources.

#### Wildcard topic
The program allows the client to subscribe to a multiple topics in a much more efficient manner, by using wildcards. The server uses regex to match the topic with the wildcard, and if the topic matches the wildcard, the message is sent to the subscriber. There are two types of wildcards: `+` and `*`, which determine the "range" of the topics that will be matched, without the need to subscribe to each one individually.

`+` - replaces one level in the topic hierarchy
`*` - replaces any number of levels in the topic hierarchy

When a client wishes to subscribe to a topic using wildcards, the server uses the `topic_matches()` function, which constructs a regex based on the topic. It transforms wildcards with the corresponding regular expressions (`+` becomes `[^/]+` and `*` becomes `.*`), adding the necessary delimiters (`^` and `$`) to ensure the topic is matched exactly.

### Subscriber
Implemented as a TCP client, the subscriber connects to the server and sends messages to subscribe/unsubscribe to topics, as well as to disconnect from the server. The subscriber can also receive messages from the server, which are forwarded from the UDP client.

#### Commands
To subscribe or unsubscribe to a topic:
```bash
subscribe <TOPIC>
unsubscribe <TOPIC>
```

To disconnect from the server:
```bash
exit
```

#### Flow
1. *Initialization*: Start by initializing the environment: parse command-line arguments to retrieve the subscriber's ID, server IP address and port.
2. *Socket setup*: Create a TCP socket, set socket options (SO_REUSEADDR, TCP_NODELAY), and connect to the server.
3. *Send connection request*: Send the subscriber's ID to the server to indicate that it's online and ready to communicate.
4. *Run the subscriber loop*: Wait for events on the sockets using `poll()`. There are several scenarios to handle:
- *STDIN command* (Handle user commands):
    - *Subscribe/Unsubscribe*: Send the topic-specific request to the server.
    - *Exit*: Send the disconnection notice to the server and close the socket.
- *Server message*: Receive the message from the server, read IP, port and content. Based on the type of the message (integer, short real, float or string), process and display the message.
5. *Cleanup and exit*: After exiting the loop on receiving the "exit" command, close the socket and clean up resources


### Application / Content source
Implemented as a UDP client, its functionalities are already provided as part of the project skeleton. The UDP client sends messages to the server, which will forward them to the subscribers based on their subscriptions.
