#ifndef TCP_MASTER_H
#define TCP_MASTER_H



#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define PORT 8080

int Init_tcp_server();
int set_nonblocking(int fd);

#endif