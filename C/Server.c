#include "Common.h"
#include "Worker.h"
#include "Dispatcher.h"

int main()
{
	int list_s,conn_s=-1,res;
	struct sockaddr_in servaddr;
	pthread_t temp_th;
	int *temp_i, optval;
	
	optval = 1;
	
	fprintf(stdout, "Initializing Distributed File System (DFS) server\n");

	if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		fprintf(stderr, "DFSSERV: Error creating listening socket.\n");
		return -1;
	}
	
	setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(8000);
	
	if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		fprintf(stderr, "DFSSERV: Error calling bind()\n");
		return -1; 
	}

	if (listen(list_s, 10) < 0)
	{
		fprintf(stderr, "DFSSERV: Error calling listen()\n");
		return -1;													
	}

	fprintf(stdout, "Launching workers...\n");

	//Lanzamiento de workers
	if(initialize_workers() < 0)
		fprintf(stderr, "DFSSERV: Error initializing workers\n"); 
	
	fprintf(stdout, "Launching dispatcher...\n");
	
	//Lanzamiento de dispatcher
	if(!initialize_dispatcher(list_s))
		fprintf(stderr, "DFSSERV: Error initializing dispatcher\n");

	return 0;
}

