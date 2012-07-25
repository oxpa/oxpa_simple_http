oxpa_simple_http
================

Simple http is a 1.0 http web server.
It should be able to serve static files to a client and provide directory listings if needed.

1) server should parse HTTP/1.0 request and generate answer on any request
2) answer should have a valid HTTP form
3) code is determined in accordance with diagram (http://http-headers-status.googlecode.com/files/http-headers-status%20v3%20draft.png) but could be "Not Implemented"
4) logging is done via syslog
5) mime type is hardcoded or determined by "file -ib "

server should be able to serve content to several clients
server should be able to serve html, css, xml, xslt, gif,jpeg
mime type is determined by filename (.html, .css, .xml, .xslt, .gif, .jpeg)


on start: 
    parse command line options
    create listening sockets
    wait for connections

on accepting connection:
    read whole request and peform formal check
        stop request processing at first fault
    parse request string
        if incorrect - return 400 status and page
    parse request headers
        if incorrect - return 400 status and page
    remove "../" strings from file address =) and make request clean silently
    uudecode?
    check if file is accessible 
    prepare iether 404, or 403, or 200 answer
    begin file or listing transfer
    close socket on connection lost or file sent.

