#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include <cstring>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <poll.h>

using namespace std;
uint32_t k_max_msg = 4096;

struct Conn {
    int fd = -1;

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    //buffered input and output
    vector<uint8_t> incoming;
    vector<uint8_t> outgoing;
};

// append to the back
static void buff_append(vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data+len);
}

// remove from the front
static void buff_consume(vector<uint8_t>&buf, size_t n) {
    buf.erase(buf.begin(), buf.begin()+n);
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size()>0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if(rv < 0 && errno == EAGAIN) {
        return;
    }

    if(rv < 0) {
        conn->want_close = true;
        printf("write failed to connection\n");
        return;
    }
    //remove written data from Conn:outgoing
    buff_consume(conn->outgoing, (size_t)rv);
    if(conn->outgoing.size()==0) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

static bool try_one_request(Conn* conn) {
    if(conn->incoming.size()<4)
    return 4;

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if(len > k_max_msg) {       //protocol error
        conn->want_close = true;
        printf("protocol exceeded length\n");
        return false;
    }
    // protocol:message body
    if(4+len>conn->incoming.size()) {
        return false;
    }
    const uint8_t *request = &conn->incoming[4];
    // process the parsed message
    // ...
    // generate the response
    printf("client says:%.*s\n", len, (char*)request);

    buff_append(conn->outgoing, (const uint8_t *)&len, 4);
    buff_append(conn->outgoing, request, len);
    // remove the message from Conn:incoming
    buff_consume(conn->incoming, 4+len);

    return true;

}

static void fd_set_nb(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}   

static Conn *handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr* )&client_addr, &addrlen);

    if(connfd < 0) {
        return NULL;
    }

    fd_set_nb(connfd);
    printf("New connection established\n");
    Conn* conn = new Conn;
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

static void handle_read(Conn* conn) {
    uint8_t buf[64*1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if(rv <= 0) {
        conn->want_close = true;
        printf("unable to read\n");
        return;
    }
   // printf("read successful\n");

    buff_append(conn->incoming, buf, (size_t)rv);
    // try to parse the accumulated buffer
    // process the parsed message
    // remove the message from Conn:incoming       
    try_one_request(conn);
        
    
    if(conn->outgoing.size()>0) {
        conn->want_write = true;
        conn->want_read = false;
    }
}

int main()
{
    vector<Conn*> fd2conn;
    vector<struct pollfd> poll_args;
    
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
    address.sin_port = htons(8000);

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
    fd_set_nb(server_fd);

    while(true) {
       // printf("while loop running\n");
        poll_args.clear();

        struct pollfd pfd = {server_fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for(Conn* conn: fd2conn) {
            if(!conn) {
                continue;
            }

            struct pollfd pfd = {conn->fd, POLLERR, 0};

            if (conn->want_read) {
                pfd.events |= POLLIN;
            }
            if(conn->want_write) {
                pfd.events |= POLLOUT;
            }   
            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if(rv<0 && errno == EINTR) {
            printf("error: cannot poll\n");
            continue;
        }
        if(rv < 0) {
          //  die("poll");
        }   
        
        if(poll_args[0].revents) {
            if(Conn* conn = handle_accept(server_fd)) {
                if(fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd+1);
                    fd2conn[conn->fd] = conn;
                }
            }
        }

        for(size_t i = 1; i<poll_args.size(); i++) {
            uint32_t ready = poll_args[i].revents;
            Conn* conn = fd2conn[poll_args[i].fd];
            if(ready & POLLIN) {
               // printf("read available\n");
                handle_read(conn);      //application logic
            }
            if(ready & POLLOUT) {
               // printf("write available\n");
                handle_write(conn);    //application logic
            }
            if(ready & POLLERR || conn->want_close) {
                printf("client connection closed\n");
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }
    return 0;
}
