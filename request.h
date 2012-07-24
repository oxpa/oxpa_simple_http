
struct request {
    char *  rq_request_line;    //first line of request "METHOD OBJ HTTP/1...."
    char ** rq_headers;         //headers of request
    char ** rq_parameters;      //parameters from request (get of post)
    char *  rq_object_requested;//path to a requested object
    char *  rq_object_mime;     //mimetype of a requested object
    int     rq_status;          //request status in HTTP codes.
    int     rq_size;
    
};
