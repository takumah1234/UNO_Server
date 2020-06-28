#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "TCPUDP.h"		// StartConnection()

int main(int argc, char *argv[])
{
	int servPort[2];

	printf("StartSystem\n");

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <Server Port1(GM)> <Server Port2(Chat)>\n", argv[0]);
		exit(1);
	}

	srand((unsigned int)time(NULL));

	for (int i = 1; i < 3; ++i)
	{
		servPort[i-1] = atoi(argv[i]);
	}

	signal(SIGPIPE, SIG_IGN);

	StartConnection(servPort);

	printf("EndSystem\n");

	return 0;
}
