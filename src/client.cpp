#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include <cstring>
#include <iostream>

#define PORT 8080

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
}

static int32_t query(int fd, char* text)
{
    int32_t len = (uint32_t)strlen(text);
    if(len>k_max_msg) {
        return -1;
    }

    //send request
    char wbuf[4+k_max_msg];
    memcpy(wbuf, &len, 4);   //write len to write buffer (little endian)
    memcpy(&wbuf[4], text, len);
    if(int32_t err = write_all(fd, wbuf, 4+len)) {
        return err;
    }

    //4 bytes header
    char rbuf[4+k_max_msg];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if(err) {
        return err;
    }
    memcpy(&len, rbuf, 4);      //len of read data
    if(len>k_max_msg) {
        return -1;
    }
    err = read_full(fd, &rbuf[4], len);
    if(err) {
        return err;
    }
    //do something
    printf("server says: %.*s\n", &rbuf[4]);
    return 0;
}


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    const char *hello = "Hello from client\n";

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }
    
    int32_t err = query(sock, "hello1");
   
    err = query(sock, "hello2");
  

LDONE:
    close(sock);
    return 0;
}
