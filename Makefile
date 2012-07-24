all: 
	gcc -g -Wall -o http.test http.c
clean:
	rm http.test
