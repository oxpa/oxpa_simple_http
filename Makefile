all: 
	gcc -g -Wall -o http.test http.c urldecode.c
clean:
	rm http.test
