#include "../include/TCP_Master.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
int Init_tcp_server(){
    int server_fd, epoll_fd;
    struct sockaddr_in addr;
    struct epoll_event ev, events[MAX_EVENTS];

    // 创建TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置地址重用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    // 绑定socket
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 设置非阻塞模式
    set_nonblocking(server_fd);

    // 开始监听
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 创建epoll实例
    if ((epoll_fd = epoll_create1(0)) < 0) {
        perror("epoll creation failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 添加服务器socket到epoll
    ev.events = EPOLLIN | EPOLLET; // 边缘触发模式
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl failed");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);

    while (1) {
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nready < 0) {
            perror("epoll_wait error");
            continue;
        }

        for (int i = 0; i < nready; i++) {
            if (events[i].data.fd == server_fd) {
                // 处理新连接
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                if (client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("accept error");
                    continue;
                }

                set_nonblocking(client_fd);
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                    perror("epoll_ctl client error");
                    close(client_fd);
                }
            } else if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                // 处理错误或断开连接
                close(events[i].data.fd);
            } else if (events[i].events & EPOLLIN) {
                // 处理可读事件
                char buffer[BUFFER_SIZE];
                int fd = events[i].data.fd;
                ssize_t bytes_read;

                while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                    // 处理接收到的数据
                    write(STDOUT_FILENO, buffer, bytes_read);
                }

                if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN)) {
                    close(fd);
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}


int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}