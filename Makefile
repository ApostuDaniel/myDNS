all:
	gcc resolver.c -o resolver
	gcc server.c -o server -lsqlite3
clean:
	rm -f resolver server