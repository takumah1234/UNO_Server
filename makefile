ServerForUNO: GameSystem.o TCPUDP.o Chat.o ServerForUNO.o
	cc -o ServerForUNO GameSystem.o TCPUDP.o Chat.o ServerForUNO.o

GameSystem.o: GameSystem.c GameSystem.h TCPUDP.h TCPUDP.c
	cc -c GameSystem.c

TCPUDP.o: TCPUDP.c TCPUDP.h Chat.h Chat.c GameSystem.c GameSystem.h
	cc -c TCPUDP.c

Chat.o: Chat.c Chat.h GameSystem.c GameSystem.h TCPUDP.c TCPUDP.h
	cc -c Chat.c

ServerForUNO.o: ServerForUNO.c TCPUDP.c TCPUDP.h
	cc -c ServerForUNO.c