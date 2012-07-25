#ifndef CLIENT_H
#define CLIENT_H
#include "request.h"
#include <sys/types.h>
struct client {
   int cl_fd;                       // socket fd.
   struct sockaddr_in cl_client;    // client socket
   int cl_bytes_read;               // how much data we have read till now.
   int cl_bytes_sent;               // how much data we have sent to this client till now.
   char * cl_rbuffer;               // buffer for reading data. 
                                    //  cl_bytes_read are meaningfull when reading. Buffer is 4k long and should be extended on a by page basis.
   char * cl_sbuffer;               // buffer for sending data. 
                                    //  start sending at cl_bytes_sent and till /0 when sending data.
   struct request * cl_request;     // parsed request.
   time_t cl_first_seen;            // time when we got connection. 
   time_t cl_last_seen;             // time when last activity seen.
   int cl_header_sent;
};
#endif
