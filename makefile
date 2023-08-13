all:	clean	client

client:	client.c
	gcc client.c -o client -lpthread

all:	clean	server

server:	server.c
	gcc server.c -o server -lpthread

clean:
	$(RM) client
	$(RM) server
