all: client server utils
client: client.c
	gcc client.c -o client -lpthread
server: server.c
	gcc server.c -o server -lpthread
utils: utils.c
	gcc utils.c -o utils 
clean:
	rm -f *.o client
	rm -f *.o server
	rm -f *.o utils
