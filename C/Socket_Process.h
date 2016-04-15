typedef struct{
	int conn_s;
	int client_id;
	int worker_id;
	mqd_t worker_queue;
	mqd_t client_queue;
}Session;

void end_session(Session session);
int checkname(char *buffer);
void* socket_process(void * arg);	
