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
#include "request.h"
#include "urldecode.h"

#include <sys/epoll.h>

#define EXPECTED_CLIENTS 5
#define EPOLL_TIMEOUT 1000
#define CLIENT_TIMEOUT 60
#define MAX_REQUEST_SIZE 8192
//#define MAX_REQUEST_SIZE 20
#define METHOD_NOT_ALLOWED 405
#define BAD_REQUEST -400
#define GOOD_REQUEST -200
#define FORBIDDEN -403
#define NOT_FOUND -404
#define INTERNAL_SERVER_ERROR -503
#define MAX_METHOD_LENGTH 4
#define NUM_OF_METHODS 4

#define RETURN_GOOD 1
#define RETURN_BAD 0
#define NOT_MALLOCCED 0
#define IS_MALLOCCED 1
 
void help() {
    printf("port num is the first argument\n");
    printf("message is the second argument\n");
    printf("Here goes some help\n");
}

int parse_options(){
    printf("here will be options handling\n");
    return EXIT_SUCCESS;
}

char * get_content_root() {
    //FIXME: content root should be read from options or config
    return get_current_dir_name();

}
int client_timeout (void * ptr){
    struct client * my_client = (struct client *)ptr;
    return my_client->cl_first_seen - time(NULL) > CLIENT_TIMEOUT ? EXIT_FAILURE:EXIT_SUCCESS;
};
int read_all_request_data(void * ptr){
#define BUFFER_SIZE MAX_REQUEST_SIZE    //just a magic number. FIXME: should be a page size or so
    struct client * my_client = (struct client *)ptr;
    
    my_client->cl_rbuffer= (char *) malloc(BUFFER_SIZE); 
    memset(my_client->cl_rbuffer,0,BUFFER_SIZE);
    my_client->cl_request= (struct request *) malloc (sizeof(struct request));
    memset(my_client->cl_request,0,sizeof(struct request));
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
            return BAD_REQUEST;
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
}

void return_code(void * ptr, int code) {
    struct client * my_client=(struct client *)ptr;
    my_client->cl_request->rq_status=code;
    my_client->cl_request->rq_object_mime = strdup("text/html");
    char * buffer, *page;
    switch (code) {
        case 400 : 
                    buffer=strdup("400 Bad Request");
                    page=strdup("400.html");
                    break;
        case 403 :
                    buffer=strdup("403 Forbidden");
                    page=strdup("403.html");
                    break;
        case 404 :   
                    buffer=strdup("404 Not Found");
                    page=strdup("404.html");
                    break;
        case 503 :
        default :
                    buffer=strdup("503 Internal Server Error");
                    page=strdup("503.html");
                    break;
    }
    //FIXME: fill in **rq_headers, not just a string! prepare_answer_buffer() should do this!
    char * format = "HTTP/1.0 %s\r\nContent-Type: text/html\r\nConnection: Close\r\nContent-Length: ";
    my_client->cl_sbuffer = (char *) malloc ( (strlen(buffer)+strlen(format)-2/*%s*/+1/*\0*/)*sizeof(char) );
    memset(my_client->cl_sbuffer,0,(strlen(buffer)+strlen(format)-2/*%s*/+1/*\0*/)*sizeof(char));
    sprintf(my_client->cl_sbuffer, format, buffer);
    free(buffer);
    if (my_client->cl_request->rq_object_requested != NULL) {
        free(my_client->cl_request->rq_object_requested);
    }
    my_client->cl_request->rq_object_requested=page;

    struct stat *file_stats;
    file_stats=(struct stat *) malloc(sizeof(struct stat));

    lstat(my_client->cl_request->rq_object_requested, file_stats);
    if (S_ISREG(file_stats->st_mode)){
        my_client->cl_request->rq_size=file_stats->st_size;
    } else { 
        my_client->cl_request->rq_size = 0;
    };
    free(file_stats);
}

void return_400(void * ptr) {
    return_code(ptr,400);
}
void return_403(void * ptr) {
    return_code(ptr,403);
}
void return_404(void * ptr) {
    return_code(ptr,404);
}
void return_503(void * ptr) {
    return_code(ptr,503);
}

int parse_request(void * ptr) {
    // request have two spaces: after method and after object. Find them.
    // find out the method and object.
    //FIXME: omg! use strtok here for kittens sake!
    struct client * my_client=(struct client *)ptr;
    char * all_methods[NUM_OF_METHODS+1];
    all_methods[RQ_GET]="GET ";
    all_methods[RQ_HEAD]="HEAD ";
    all_methods[RQ_PUT]="PUT ";
    all_methods[RQ_OPTIONS]="OPTIONS ";
    all_methods[4]=NULL;
    int i;
    for(i=0;i<NUM_OF_METHODS;i++) {
        if (strstr(my_client->cl_rbuffer,all_methods[i]) == my_client->cl_rbuffer ) {
            break;
        }
    }
    //we got all_methods[i] request
    
    if (all_methods[i] == NULL) {
        return_400(ptr);
        return BAD_REQUEST;
    }else{
        my_client->cl_request->rq_method=i;
    }
    char * second_space=memchr(my_client->cl_rbuffer + strlen(all_methods[i]),' ',my_client->cl_bytes_read - strlen(all_methods[i]));
    if (second_space == 0) {
        return_400(ptr);
        return BAD_REQUEST;
    }else{
        char * all_http_versions[3];
        all_http_versions[0]="HTTP/0.9\r\n";
        all_http_versions[1]="HTTP/1.0\r\n";
        all_http_versions[2]="HTTP/1.1\r\n";
        int j;
        for(j=0;j<3;j++){
            if (strstr(second_space,all_http_versions[j]) == second_space + 1 ){
                break;
            }
        }
        if (j==3){
            return_400(ptr);
            return BAD_REQUEST;
        }
        //here I have "METHOD SPACE SOMEBYTES HTTP_VERSION" string for sure. 
        //need to get object from bytes.
        //FIXME: object is not only a file!
        //
        char * k;
        while( k=strstr(my_client->cl_rbuffer,"//")){
            second_space--;
            while(k<my_client->cl_rbuffer+my_client->cl_bytes_read){
             *k=*(k+1);
             k++;
            }
        }
        int my_shift = * (my_client->cl_rbuffer + strlen(all_methods[i])) == '/' ? strlen(all_methods[i]) + 1 : strlen(all_methods[i]);
        char * raw_object_request=strndup(my_client->cl_rbuffer + my_shift, second_space - my_client->cl_rbuffer - my_shift);
        if (strlen(raw_object_request)<1) {
            free(raw_object_request);
            raw_object_request=strdup("index.html");
        }
        printf("%s \n", raw_object_request);
        char * decoded_object_request = url_decode(raw_object_request);
        free(raw_object_request);
        my_client->cl_request->rq_object_requested = realpath(decoded_object_request, NULL);
        if (my_client->cl_request->rq_object_requested == NULL) {
            switch (errno){
                case EACCES      : return_403(ptr); return FORBIDDEN;

                case EIO         :
                case ELOOP       : return_503(ptr); return INTERNAL_SERVER_ERROR;

                case ENOENT      :
                case ENAMETOOLONG:
                case ENOTDIR     : return_404(ptr); return NOT_FOUND;
                default     : perror("errno case in parse request"); exit(EXIT_FAILURE);
            }
            return EXIT_FAILURE;
        }
        free(decoded_object_request);
        char * content_root=get_content_root();
        if (strstr(my_client->cl_request->rq_object_requested, content_root) != my_client->cl_request->rq_object_requested) {
            return_403(ptr);
            free(content_root);
            return FORBIDDEN;
        }
        free(content_root);

        return GOOD_REQUEST;
    };
    
};


void free_client(struct client * my_client, int mallocced){
    //close fd and check for any memory allocations done;
    close(my_client->cl_fd);
    free(my_client->cl_rbuffer);
    free(my_client->cl_sbuffer);
    if (my_client->cl_request){
        free (my_client->cl_request->rq_request_line);
        free(my_client->cl_request->rq_object_requested);
        free(my_client->cl_request->rq_object_mime);
        int i=0;
        for(i=0;i<my_client->cl_request->rq_num_of_headers;i++){
            free(my_client->cl_request->rq_headers[i]);
        }
        for(i=0;i<my_client->cl_request->rq_num_of_params;i++){
            free(my_client->cl_request->rq_parameters[i]);
        }
        //free(my_client->cl_request->rq_headers);
        //free(my_client->cl_request->rq_parameters);
        free(my_client->cl_request);

    }
    if (mallocced==1) {
            free(my_client);
    }else{
        memset(my_client,0,sizeof(struct client));
    };
}
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
int send_headers(void * ptr){
    struct client * my_client=(struct client *)ptr;
    size_t bytes_sent=send(my_client->cl_fd,my_client->cl_sbuffer + my_client->cl_bytes_sent,strlen(my_client->cl_sbuffer),0);
    if (bytes_sent == -1) {
        switch(errno){
            case EAGAIN:
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
                    // smth strange: we could write, but can't now. Just wait till timeout
                    break;
            default:
                    // an error have occured. remove client
                    free_client(my_client,NOT_MALLOCCED);
        }
    }else if (bytes_sent < strlen(my_client->cl_sbuffer)) {
        //sent a part of headers. continue sending it.
        my_client->cl_last_seen=time(NULL);
        my_client->cl_bytes_sent += bytes_sent;
    }else if (bytes_sent == strlen(my_client->cl_sbuffer)) {
        //headers are sent. zero cl_bytes_sent and start object transfer right now! 
        //There would be no "next iteration" if the buffer is not filled up;
        my_client->cl_header_sent=1;
        my_client->cl_bytes_sent=0;
        if (my_client->cl_request != RQ_HEAD) {
            send_data(ptr);
        }
    }
}
int send_data(void * ptr){
    struct client * my_client=(struct client *)ptr;
    //send file until EAGAIN or EWOULDBLOCK or other error
    do {
        int bytes_sent=sendfile(my_client->cl_fd, open(my_client->cl_request->rq_object_requested,O_RDONLY),&my_client->cl_bytes_sent,my_client->cl_request->rq_size);
        if(bytes_sent >=0) {
            //do nothing, just continue buffer filling
            my_client->cl_last_seen=time(NULL);
            if ((bytes_sent==my_client->cl_request->rq_size)||(bytes_sent + my_client->cl_bytes_sent==my_client->cl_request->rq_size)) {
                free_client(my_client,NOT_MALLOCCED);
                return RETURN_GOOD;
            };
        }else if(bytes_sent==-1){
            switch (errno){
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                            //buffer is full, need to wait till writing is allowed
                            return RETURN_GOOD;
                          break;
                default:
                          //on any other error should close the socket and free client memory
                          free_client(my_client,NOT_MALLOCCED);
                          return RETURN_BAD;
            };
        };
    } while(1);
}
int prepare_answer_buffer(void * ptr){
    struct client * my_client=(struct client *)ptr;

    if (my_client->cl_sbuffer ==0) {
        //there was no return_code call previously. prepare full answer;
        struct stat *file_stats;
        file_stats=(struct stat *) malloc(sizeof(struct stat));
        lstat(my_client->cl_request->rq_object_requested, file_stats);
        if (S_ISREG(file_stats->st_mode)){
            my_client->cl_request->rq_size=file_stats->st_size;

            my_client->cl_request->rq_status=200;
            char * dot = memrchr(my_client->cl_request->rq_object_requested,'.',strlen(my_client->cl_request->rq_object_requested));
            my_client->cl_request->rq_object_mime = strdup("text/html");
            if (dot != NULL) {
                //try do some mime magic
                //FIXME: use something like system("file") can do;
                char * images[4];
                images[0]=".gif";
                images[1]=".jpeg";
                images[2]=".png";
                images[3]=NULL;
                int i;
                for(i=0;i<3;i++){
                    if(strcasestr(dot,images[i])==dot) {break;};
                }
                printf("i is %i, dot is %i\n",i,dot);
                if (images[i]!=NULL) {
                    free(my_client->cl_request->rq_object_mime);
                    my_client->cl_request->rq_object_mime = (char *) malloc((strlen("image/;charset=binary")+strlen(images[i]))*sizeof(char));
                    memset(my_client->cl_request->rq_object_mime,0,(strlen("image/;charset=binary")+strlen(images[i]))*sizeof(char));
                    if (my_client->cl_request->rq_object_mime == NULL) {
                        perror("malloc rq_object_mime");
                        exit(EXIT_FAILURE);
                    };
                    sprintf(my_client->cl_request->rq_object_mime,"image/%s;charset=binary",images[i]+1);
                }
            }

            char format[]="HTTP/1.0 200 OK\r\nContent-Type: %s\r\nConnection: Close\r\nContent-Length: ";
            my_client->cl_sbuffer = (char *) malloc((strlen(format)+strlen(my_client->cl_request->rq_object_mime))*sizeof(char));
            memset(my_client->cl_sbuffer,0,(strlen(format)+strlen(my_client->cl_request->rq_object_mime))*sizeof(char));
            sprintf(my_client->cl_sbuffer,format,my_client->cl_request->rq_object_mime);

            free(file_stats);
        }
    }

    char size[18];
    snprintf(size,18,"%i\r\n",my_client->cl_request->rq_size);
    char * new_buffer=realloc(my_client->cl_sbuffer,(strlen(my_client->cl_sbuffer)+strlen(size)+2+1)*sizeof(char));
    if (new_buffer == NULL) {
        //FIXME: no mo memory is not such a big problem, actually. 
        perror("can't realloc memory for buffer!");
        exit(EXIT_FAILURE);
    }
    my_client->cl_sbuffer=new_buffer;
    snprintf(my_client->cl_sbuffer+strlen(my_client->cl_sbuffer),strlen(size)+2+1/*\0 from snprintf*/,"%s\r\n",size);
    return EXIT_SUCCESS;
};

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
                        prepare_answer_buffer(events[n].data.ptr);
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
                    struct client * my_client= (struct client *) events[n].data.ptr;
                    if ((my_client->cl_bytes_sent == my_client->cl_request->rq_size)&&(my_client->cl_header_sent ==1)) {
                        //sent all data. close socket to the client and free all allocated memory.
                        free_client(my_client,NOT_MALLOCCED);
                    }else{
                        //some data need to be sent. do it.
                        if ((my_client->cl_bytes_sent < strlen(my_client->cl_sbuffer))&&(my_client->cl_header_sent !=1)) {
                            send_headers(my_client);
                        }else if(my_client->cl_header_sent ==1) {
                            //send file until EAGAIN or EWOULDBLOCK or other error
                            send_data(my_client);
                        }else {
                            assert(((my_client->cl_bytes_sent != strlen(my_client->cl_sbuffer))&&(my_client->cl_header_sent ==1)));
                            exit(EXIT_FAILURE); 
                        }
                    }
                } else if (events[n].events & EPOLLHUP) {
                    //remove client and close fd
                }
                

            }
        }
    }while(1);
    close(mysocket);
    return EXIT_SUCCESS;
};
