#ifndef REQUEST_H
#define REQUEST_H

#define RQ_GET      0
#define RQ_HEAD     1
#define RQ_OPTIONS  2
#define RQ_PUT      3

struct request {
    char *  rq_request_line;    //first line of request "METHOD OBJ HTTP/1...."
    int     rq_num_of_headers;  //quantity of headers
    char ** rq_headers;         //headers of request
    int     rq_num_of_params;   //quantity of parameters
    char ** rq_parameters;      //parameters from request (get of post)
    char *  rq_object_requested;//path to a requested object
    char *  rq_object_mime;     //mimetype of a requested object
    int     rq_status;          //request status in HTTP codes.
    int     rq_size;
    int     rq_method;
};
#endif

