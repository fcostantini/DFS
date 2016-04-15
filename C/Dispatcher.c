#include "Common.h"
#include "Dispatcher.h"
#include "Worker.h"
#include "Socket_Process.h"

Session *create_session(int conn_s, int worker_id, int client_id)
{
	Session *session = malloc(sizeof(Session));
	char client_name[5] = "/cx";

	attr.mq_flags = 0;  
	attr.mq_maxmsg = MAX_MESSAGES;  
	attr.mq_msgsize = MESSAGE_SIZE;  
	attr.mq_curmsgs = 0;

	client_name[2] = client_id;
	mqd_t client_queue = mq_open(client_name, O_RDWR | O_CREAT, 0666, &attr);	

	session -> conn_s = conn_s;
	session -> client_id = client_id;
	session -> worker_id = worker_id;
	session -> worker_queue = worker_messages[worker_id];
	session -> client_queue = client_queue;

	return session;	
}

int initialize_dispatcher(int list_s)
{
	int id = 0;
	int conn_s, worker_id;
	pthread_t new_client;

	srand(time(NULL));	
	
	while (1)
	{
		if ((conn_s = accept(list_s, NULL, NULL) ) < 0)
		{
			fprintf(stderr, "DFSSERV: Error calling accept()\n");
			return 0;
		}
		
        worker_id = rand() % NUMBER_OF_WORKERS;
		Session *session = create_session(conn_s, worker_id, id);
		id++;

		pthread_create(&new_client, NULL, socket_process, session);
		
	}

	return 1;
}
