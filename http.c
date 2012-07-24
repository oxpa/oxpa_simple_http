#include <sys/stat.h>
#include <fcntl.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

extern int errno;

#include "client.h"

#include <sys/epoll.h>

#define EXPECTED_CLIENTS 5
#define EPOLL_TIMEOUT 1000
#define CLIENT_TIMEOUT 60
#define MAX_REQUEST_SIZE 8192
#define METHOD_NOT_ALLOWED 405
#define BAD_REQUEST 400
 
void help() {
    printf("port num is the first argument\n");
    printf("message is the second argument\n");
    printf("Here goes some help\n");
}

int parse_options(){
    printf("here will be options handling\n");
    return EXIT_SUCCESS;
}

int client_timeout (void * ptr){
    struct client * my_client = (struct client *)ptr;
    return my_client->cl_first_seen - time(NULL) > CLIENT_TIMEOUT ? EXIT_FAILURE:EXIT_SUCCESS;
};
int read_all_request_data(void * ptr){
#define BUFFER_SIZE 8192    //just a magic number. FIXME: should be a page size or so
    struct client * my_client = (struct client *)ptr;
    
    my_client->cl_rbuffer= (char *) malloc(BUFFER_SIZE); 
    memset(my_client->cl_rbuffer,0,BUFFER_SIZE);
    my_client->cl_bytes_read=read(my_client->cl_fd, my_client->cl_rbuffer, BUFFER_SIZE);
    if (my_client->cl_bytes_read == -1) {
        //an error or a EAGAIN condition
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK )) {
            //FIXME: should just close connection.
            perror("client read error");
            exit(EXIT_FAILURE);
        };
        //EAGAIN: all available data is read.
        //HOW THIS COULD HAPPEN?!
        assert(my_client->cl_bytes_read > -1);

    } else if (my_client->cl_bytes_read < BUFFER_SIZE) {
        //read all available data
        return my_client->cl_bytes_read;

    } else if (my_client->cl_bytes_read == BUFFER_SIZE) {
        // some data still may be available. 
        // if it is so, the request is malformed.
        int count;
        char test_char;
        count=read(my_client->cl_fd, &test_char, 1);
        if ((count == -1)&&(errno!=EAGAIN)&&(errno!=EWOULDBLOCK)) {
            //FIXME: should just close connection.
            perror("client read error");
            exit(EXIT_FAILURE);
        }else if (count > 0) {
            //read more data. malformed GET/HEAD request
            return -400;
        }else if (count == 0) {
            return my_client->cl_bytes_read;
        }else {
            assert(count == 0);
            return EXIT_FAILURE;
        }
        
    }else {
        assert(my_client->cl_bytes_read <= BUFFER_SIZE);
        return EXIT_FAILURE;
    }
assert(my_client->cl_bytes_read >= -1);
return EXIT_FAILURE;
}; 

        //char content_type[]= "HTTP/1.0 200 OK\n\rContent-Type: text/html\r\nConnection: Close\r\nContent-Length: 127\r\n\r\n";
int parse_request(events[n].data.ptr);


/*
 * Simple helper function: sets fd to non blocking IO.
 */
int set_nonblocking(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("Could not get flags for fd");
        exit(EXIT_FAILURE);
    };
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv, char **arge) {

    openlog(argv[0],0,LOG_DAEMON);
    syslog(LOG_NOTICE, "starting up");

    if (argc < 2 ) {
        help();
        return EXIT_SUCCESS;
    }
    else {
        parse_options();
    };

    struct sockaddr_in dest;
    struct sockaddr_in serv;
    int mysocket;
    unsigned int socksize = sizeof(struct sockaddr_in);
    struct epoll_event ev, events[EXPECTED_CLIENTS];
    int epollfd, nfds, connection_socket;
    struct client clients[EXPECTED_CLIENTS];
    memset(&clients,0,EXPECTED_CLIENTS*sizeof(struct client));

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("192.168.1.47"); // INADDR_ANY for any address
    serv.sin_port = htons(atoi(argv[1]));

    mysocket = socket(AF_INET, SOCK_STREAM, 0);
    bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr));
    listen(mysocket, EXPECTED_CLIENTS);
    
    //create epoll queue
    epollfd = epoll_create(EXPECTED_CLIENTS);
    if (epollfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = mysocket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mysocket, &ev) == -1) {
        perror("error adding mysocket to epoll");
        exit(EXIT_FAILURE);
    }

    do {
        nfds = epoll_wait(epollfd, events, EXPECTED_CLIENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            perror("error waiting epoll");
            exit(EXIT_FAILURE);
        }
        int n;
        for (n = 0; n < nfds; n++) {
            if (events[n].data.fd == mysocket) {
                //new connection arrived
                 connection_socket = accept(mysocket,(struct sockaddr *) &dest, &socksize);
                if (connection_socket == -1) {
                    perror("accept failed");
                    exit(EXIT_FAILURE);
                }
                set_nonblocking(connection_socket);
                // find first free client struct and populate it with new client
                // buffers should be allocated later, if data arrives
                int i;
                void * my_client;
                for(i=0;i<EXPECTED_CLIENTS;i++) {
                    if (clients[i].cl_fd == 0) {
                        clients[i].cl_fd = connection_socket;
                        clients[i].cl_client = dest;
                        clients[i].cl_first_seen = time(NULL);
                        my_client=&clients[i];
                        break;
                    }
                }
                ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
                ev.data.ptr = my_client;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connection_socket, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            }else{  //process existing connection

                //check if read or write ready
                if (events[n].events & EPOLLIN) {
                    //read data until EAGAIN, check if last line is empty. 
                    //if not - wait more until timeout.
                    if (read_all_request_data(events[n].data.ptr)){
                        parse_request(events[n].data.ptr);
                        //prepare_answer(events[n].data.ptr);
                        ev.events = EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET;
                        ev.data.ptr=events[n].data.ptr;
                        struct client * my_client= (struct client *) events[n].data.ptr;
                        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, my_client->cl_fd, &ev) == -1) {
                            perror("epoll_ctl: conn_sock");
                            exit(EXIT_FAILURE);
                        }     
                    } 
                    
                    //need more input. check if timeout.
                    if (client_timeout(events[n].data.ptr)) {
                        //remove_client(events[n].data.ptr);
                    }
                } else if (events[n].events & EPOLLOUT){
                    //send output
                } else if (events[n].events & EPOLLHUP) {
                    //remove client and close fd
                }
                

            }
        }
    }while(1);
    close(mysocket);
    return EXIT_SUCCESS;
};
