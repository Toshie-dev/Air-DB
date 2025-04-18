#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include <cstring>
#include <iostream>

using namespace std;

struct Conn {
    int fd = -1;

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    //buffered input and output
    vector<uint8_t> incoming;
    vector<uint8_t> outgoing;
};

struct pollfd {
    int fd;
    short events;   //request want to read or write
    short revents;  //is available to read or write
};

// append to the back
static void buff_append(vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data+len);
}

// remove from the front
static void buff_consume(vector<uint8_t>&buf, size_t n) {
    buf.erase(buf.begin(), buf.begin()+n);
}

static bool try_one_request(Conn* conn) {
    if(conn->incoming.size()<4)
    return 4;

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if(len > k_max_msg) {       //protocol error
        conn->want_close = true;
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
    buff_append(conn->outgoing, (const uint8_t *)&len, 4);
    buff_append(conn->outgoing, request, len);
    // remove the message from Conn:incoming
    buff_consume(conn->incoming, 4+len);

    return true;

}

static void fd_set_nb(int fd) {
    fcntl(fd, FSETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}   

static Conn *handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr* )&client_addr, &addrlen);

    if(connfd < 0) {
        return NULL;
    }
    fd_set_nb(connfd);

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
        return;
    }

    buff_append(conn->incoming, buff, (size_t)rv);
    // try to parse the accumulated buffer
    // process the parsed message
    // remove the message from Conn:incoming       

    try_one_request(conn);
}

int main()
{
    vector<Conn*> fd2conn;
    vector<struct pollfd> poll_args;

    while(true) {
        poll_args.clear();

        struct pollfd pfd = {fd, POLLIN, 0};
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
                pdf.events |= POLLOUT;
            }   
            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if(rv<0 && errno == EINTR) {
            continue;
        }
        if(rv < 0) {
            die("poll");
        }   
        
        if(poll_args[0].revents) {
            if(Conn* conn = handle_accept(fd)) {
                if(fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd+1);
                }
            }
        }

        for(size_t i = 1; i<poll_args.size(); i++) {
            uint32_t ready = poll_args[i].revents;
            Conn* conn = fd2conn[poll_args[i].fd];
            if(ready & POLLIN) {
                handle_read(conn);  //application logic
            }
            if(ready & POLLOUT) {
                handle_write(conn);    //application logic
            }
            if(ready & POLLERR || conn->want_close) {
                (void)close(conn->fd);
                fd2conn(conn->fd) = NULL;
                delete conn;
            }
        }
    }
    return 0;
}
