try test and test2 directories. the second one contains index.html

to compile: just run make. need libc 6+ and linux kernel 2.6.26+
to run: 
	http.test ip_address port
like:
	http.test 127.0.0.1 80
parameters are mandatory. there is no "content root" currently.
ip_address and port are numerical.

server should reply to HEAD and GET correctly with static .html .png .gif and .jpeg files.
everything else is sent as text/html.

everything else is undefined;)
