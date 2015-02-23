all: radio

radio: main.c
	cc -O2 -o index.cgi main.c
	strip index.cgi
	chmod +s index.cgi
	ls -l index.cgi

test: test.c
	cc -O2 -o test.cgi test.c
	strip test.cgi
#	chmod +s test.cgi
	ls -l test.cgi
