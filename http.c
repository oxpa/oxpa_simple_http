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
extern int errno;

#define MAX_REQUEST_SIZE 8192
#define METHOD_NOT_ALLOWED 405
#define BAD_REQUEST 400
 
void help(){
    printf("port num is the first argument\n");
    printf("message is the second argument\n");
    printf("Here goes some help\n");
}

int parse_options(){
    printf("here will be options handling\n");
    return EXIT_SUCCESS;
}

int parse_request(char * buffer, int buffer_size) {
    if ((buffer != strstr(buffer, "GET"))&&
        (buffer != strstr(buffer,"HEAD"))){
        syslog(LOG_NOTICE, "wrong method");
        return METHOD_NOT_ALLOWED;
    }
    //FIXME: replace with "findoutmethod" which will get list of methods and move pointer accordingly
    if (buffer[0]='G') {
        //have "GET" in the begginning
        buffer+=3;
            //have "HEAD" 
    } else {
        buffer+=4;
    }

    if (buffer[0]==' ') {
        buffer++;
    }else {
        syslog(LOG_NOTICE, "BAD REQUEST");
        return BAD_REQUEST;
    }

    // at this point only " path HTTP/1.x headers" is left here. No use of headers currently ;)
    printf("success\n");
    //char * last_path_char = strstr (buffer, " HTTP/1.");
    printf("%s", buffer);
    printf("success\n");
    //printf("%i\n", last_path_char);
    //*last_path_char=0;
    printf("success\n");
    //last_path_char=strchr(buffer,'/');
    printf("success\n");
    //buffer=last_path_char;
    printf("success\n");
    return EXIT_SUCCESS;
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

    char msg[] = "Hello World !\n";

    struct sockaddr_in dest;
    struct sockaddr_in serv;
    int mysocket;
    unsigned int socksize = sizeof(struct sockaddr_in);

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("192.168.0.2"); // INADDR_ANY for any address
    serv.sin_port = htons(atoi(argv[1]));

    mysocket = socket(AF_INET, SOCK_STREAM, 0);
    bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr));
    listen(mysocket, 1);
    int consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
        //while ((errno==EAGAIN) ||(errno == EWOULDBLOCK))
        //consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
    perror(NULL);
    while (consocket) {
        //printf("Incoming connection from %s - sending welcome\n", inet_ntoa(dest.sin_addr));
        char content_type[]= "HTTP/1.0 200 OK\n\rContent-Type: text/html\r\nConnection: Close\r\nContent-Length: 127\r\n\r\n";
        char * buffer;
        //FIXME: MAX_REQUEST_SIZE is not a constant!
        buffer= (char *)malloc( (MAX_REQUEST_SIZE + 1) *sizeof(char));
	memset(buffer, 0, MAX_REQUEST_SIZE+1);
        buffer[MAX_REQUEST_SIZE]=NULL;
        int bytes_read=read(consocket, buffer, sizeof(buffer));
        parse_request(buffer,bytes_read);
        struct stat *file_stats;
	file_stats=(struct stat *) malloc(sizeof(struct stat));
        lstat(buffer, file_stats);
        lstat("index.html", file_stats);
        if (S_ISREG(file_stats->st_mode)){
            printf("will send file\n");
            send(consocket, content_type, strlen(content_type), 0);
	    off_t * offset= (off_t *) malloc(sizeof(off_t));
            memset(offset,0,sizeof(offset));
            while(*offset!=file_stats->st_size) {
                sendfile(consocket, open("index.html",O_RDONLY),offset,1500);
		printf("offset is %i, size is %i \n", *offset, file_stats->st_size);
            };
            free(offset);
	    send(consocket, "\r\n", 2, 0);
            printf("sent file\n");
        }
        consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
    }
        close(consocket); 
    close(mysocket);
    return EXIT_SUCCESS;
};
