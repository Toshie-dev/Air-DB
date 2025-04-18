#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include <cstring>
#include <iostream>

#define PORT 8000
const size_t k_max_msg = 4896;


static int32_t read_full(int fd, char* buff, size_t n) {
    while(n>0) {
        ssize_t rv = read(fd, buff, n);
        if(rv<=0) {
            return -1;
        }   
        
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buff += n;
    }
    return 0;
}   

static int32_t write_all(int fd, const char* buff, size_t n) {
    while(n > 0) {
        ssize_t rv = write(fd, buff, n);

        if(rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buff += rv;
    }
    return 0;
}

static int32_t one_request(int connfd) {
    char rbuf[4+k_max_msg];     //read buffer
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if(err) {
        //msg(errno = 0?"EOF" : "read() error");
        return err;
    }
    uint32_t len = 0;
    memcpy(&len, rbuf, 4);      //write from read buffer to len
    if(len>k_max_msg) {
        //msg("too long");
        return -1;
    }

    err = read_full(connfd, &rbuf[4], len);
    if(err) {
        //msg("read() error");
        return err;
    }
//    printf("read successful\n");
    //do something
    printf("client says %.*s\n", len, rbuf[4]);

    const char reply[] = "world\n";
    char wbuf[4+sizeof(reply)];
    len = (uint32_t)strlen(reply);

    memcpy(wbuf, &len, 4);      // write len of write msg to write buffer
    memcpy(&wbuf[4], reply, len);

    return write_all(connfd, wbuf, 4+len);
}


int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while(true)
    {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);

        int connfd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if(connfd<0)
            continue;

        while(true) {
            int32_t err = one_request(connfd);
            if(err) {
                break;
            }
        }
        close(connfd);
    }

    // Close socket
    close(server_fd);
    return 0;
}
