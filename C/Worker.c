#include "Worker.h"

int global_fd = 75;
pthread_mutex_t sem = PTHREAD_MUTEX_INITIALIZER;

//Envia una request a todos los workers, en caso de que el emisor sea un worker se lo saltea
void broadcast(int from, Request req)
{
	int i;
	
	if(from == SERV_BROAD)
	{
		for(i = 0;i < NUMBER_OF_WORKERS;i++)
			if (mq_send(worker_messages[i], (char *) &req, sizeof(Request), PSERVER) < 0)
				fprintf(stderr, "An error has ocurred sending the request\n");
	}
	else
	{
		req.from_queue = worker_messages[from];
		for(i = 0;i < NUMBER_OF_WORKERS;i++)
		{ 
			if(i == from)
				continue;
			if (mq_send(worker_messages[i], (char *) &req, sizeof(Request), PWORKER) < 0)
				fprintf(stderr, "An error has ocurred sending the request\n");
		}
	}
	
	return;
}

//Toma la variable global fd, hace un lock y le agrega uno, deslockea y devuelve este nuevo valor
int new_fd (void)
{    
    int fd;
    pthread_mutex_lock(&sem);
    global_fd++;
    fd = global_fd;
    pthread_mutex_unlock(&sem);
    return fd;
}

//Busca el nombre indicado en una lista de archivos, y en caso de encontrarlo devuelve el archivo
File *search_name(File *files, char *name)
{   
	File *node = files;
	
    if (name == NULL)
		return NULL;
    
    while (node != NULL)
	{
        if(!strcmp(node -> name, name))
            return node;
        else 
            node = node -> next;
    }  
    
    return NULL;       
}

//Busca el file descriptor indicado en una lista de archivos, y en caso de encontrarlo devuelve el archivo
File *search_fd(File *files, int fd)
{   
	File *node = files;
	             
    while (node != NULL)
	{
        if(fd == node -> fd)
            return node;
        else 
            node = node -> next;
    }  
    
    return NULL;       
}

//Lista los archivos del workers (se guardan en list)
void my_files(Worker *worker, char *list)
{
    File *node = worker -> files;
        
    while(node != NULL)
	{
        strcat(list, node -> name);
        strcat(list, " ");
		node = node -> next;
    }
    return;
}

//Crea un archivo (se pone al principio de la lista argumento)
File *create_file(char *name, File *files)
{
    File *new_file = malloc(sizeof(File));
    
    strcpy(new_file -> name, name);
    new_file -> content = NULL;
    new_file -> position = 0;
	new_file -> size = 0;
	new_file -> opener = 0;
    new_file -> fd = 0; //El fd se asigna al abrirse el archivo
	new_file -> can_delete = 0;
    new_file -> next = files; 
    
    return new_file;
}
         
//Borra un archivo         
File *delete_file(char *name, File *list)
{	
	if(!list)
		return NULL;
	
	File *node, *node_del = list;
	
	if(!strcmp(node_del -> name, name))
	{
		list = list -> next;
		free(node_del -> content);
		free(node_del);
		return list;
	}
	else
	{
		node = list;
		while(strcmp((node -> next) -> name, name))
			node = node -> next;
			
		node_del = node -> next;
		node -> next = node_del -> next;
		free(node_del -> content);
		free(node_del);
		return list;
	}	
}

//Abre un archivo (basicamente asignarle un fd)
int open_file(File *file, int opener)
{
	file -> opener = opener;
	file -> fd = new_fd();
	
	return file -> fd;
}

//Cierra un archivo (pone en 0 el fd y resetea la posicion del puntero)
int close_file(File *file)
{	
	file -> opener = 0;
	file -> fd = 0;
	file -> position = 0;
	
	return 0;
}

//Cierra todos los archivos asociados a un cliente
void close_all_files(File *list, int client_id)
{
    File *node = list;
    
    for(; node != NULL; node = node -> next)
	{
        if(node -> opener == client_id)
        {
            if(node -> can_delete == 1)	
                list = delete_file(node -> name, list);
            else
                close_file(node);
        }
    }

    return;
}

//Escribe el texto indicado en un archivo
int write_file(File *file, char *text, int size)
{
    if(size > strlen(text))
        size = strlen(text);  

	file -> content = realloc(file -> content, (file -> size) + size);
	memcpy((file -> content) + (file -> size), text, size);
	file -> size += size;
	
	return 0;
}

//Lee la cantidad indicada de un archivo
char *read_file(File *file, char *buffer, int number)
{
	
	int available, l;
	int check = (file -> size) - (file -> position);
		
	if(check <= 0)
		sprintf(buffer, "OK SIZE 0 NULL");
	else
	{
		if (check >= number)
			available = number;
		else
			available = check;

		sprintf(buffer, "OK SIZE %d ", available);
		l = strlen(buffer);
		memcpy(buffer + l, (file -> content) + (file -> position), available);
		buffer[l + available] = '\0';
		file -> position += available;
	}	
	return buffer;
}

//Chequea si un archivo esta abierto
int is_open(File *file)
{
    return file -> fd;
}

//Se fija si el cliente indicado abrio el archivo
int client_opened(File *file, Request req)
{    
    return file -> opener == req.client_id;
}

//Crea un nuevo registro (al comienzo de la lista)
Record *new_record(Record *list, int client_id, mqd_t client_queue)
{
    Record *newr = malloc(sizeof(Record));
    
    newr -> lsd[0] = '\0';
    newr -> client_id = client_id;
    newr -> counter = 0;
    newr -> client_queue = client_queue;
    newr -> next = list;
    
    return newr;
}

//Busca el registro asociado al cliente indicado y si lo encuentra lo devuelve
Record *retrieve_record(Record *list, int client_id)
{     
    while(list != NULL)
	{
        if(list -> client_id == client_id)
            return list;
        list = list -> next;
    }

    return NULL;
}

//Cambia un registro (incrementa contador y en caso de LSD concatena un listado de archivos)
void change_record(Record *record, char *to_add)
{    
    (record -> counter)++;
    
    if(to_add != NULL)
        strcat(record -> lsd, to_add);
          
    return;   
}

//Chequea cuando un registro esta completo (llegaron todas las respuestas)
int full_record(Record *record)
{
    return (record -> counter == NUMBER_OF_WORKERS - 1);
}

//Borra un registro
Record *delete_record(Record *list, int client_id)
{
    if(list == NULL)
		return NULL;
	
	Record *node, *node_del = list;
	
	if(list -> client_id == client_id)
	{
		node_del = list;
		list = list -> next;
		free(node_del);
		return list;
	}
	else
	{
		node = list;
		while((node -> next) -> client_id != client_id)
			node = node -> next;
			
		node_del = node -> next;
		node -> next = node_del -> next;
		free(node_del);
		return list;
	}	 
}

int initialize_workers()
{
	int i;
	char mq_name[3] = "/wx";
	Worker *worker;

	worker_messages = malloc(sizeof(mqd_t) * NUMBER_OF_WORKERS);

	attr.mq_flags = 0;  
	attr.mq_maxmsg = MAX_MESSAGES;  
	attr.mq_msgsize = MESSAGE_SIZE;  
	attr.mq_curmsgs = 0;
	
	for(i = 0; i < NUMBER_OF_WORKERS; i++)
    {
		mq_name[2] = '0' + i;
		worker_messages[i] = mq_open(mq_name, O_RDWR | O_CREAT, 0666, &attr);								

		if(worker_messages[i] == -1)
        {
			fprintf(stderr, "ERROR: Failure to create/open the message queue %d \n", i);
			return -1;
		}
	}
  	
	for(i = 0; i < NUMBER_OF_WORKERS; i++)
    {
    	worker = malloc(sizeof(Worker));
    	worker -> id = i;
    	worker -> msg_queue = worker_messages[i];
    	worker -> files = NULL;

    	if(pthread_create(&workers[i], NULL, worker_fun, worker))
        {
			fprintf(stderr, "ERROR: Failure to launch worker %d \n", i);
			return -1; 
		}
	}

	return 0;
}

void *worker_fun(void *w_space)
{
	Worker workspace = *(Worker *)w_space;
    free(w_space);
    char aux[BUFFER_OUT], message[BUFFER_IN];
    Request req;
	Record *record = NULL, *recordaux = NULL;
	File *file;
		
	while(1)
    {
	
		memset(message, 0, BUFFER_IN - 1);
		
	    if(mq_receive(workspace.msg_queue, aux, BUFFER_OUT, NULL) < 0)
	        fprintf(stderr, "ERROR: Failure to retrieve the message from the queue\n");
		
        req = *(Request *)aux;	
        
        switch(req.op)
        {
            case LSD_C:
		        record = new_record(record, req.client_id, req.from_queue);
			    strcat(record -> lsd, "OK ");
			    my_files(&workspace, record -> lsd);
			    req.op = LSD_W;
			    broadcast(workspace.id, req);
                break;

            case OPN_C:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = OPN_W;
				    broadcast(workspace.id, req);
			    }
			    else
                {
				    if(is_open(file))
					    sprintf(message, "ERROR 45 EFILEOPEN");
				    else
					    sprintf(message, "OK FD %d", open_file(file, req.client_id));
				
				    if (mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
                        fprintf(stderr, "An error has ocurred sending the message\n");
                }
                break;

            case DEL_C:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = DEL_W;
				    broadcast(workspace.id, req);
			    }
				else
                {
				    if(is_open(file))
                    {
					    sprintf(message, "ERROR 45 EFILEOPEN (DELETION WHEN CLOSED)");
                        file -> can_delete = 1;
                    }
				    else
                    {
					    workspace.files = delete_file(req.arg0.arg0c, workspace.files);
					    sprintf(message, "OK");
				    }
				    if(mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
						fprintf(stderr, "An error has ocurred sending the message\n");	
			    }
                break;

            case CRE_C:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = CRE_W;
				    if(!pthread_mutex_trylock(&sem))
					    broadcast(workspace.id, req);					
				    else //Reenvio
					    if(mq_send(worker_messages[workspace.id], (char *) &req, sizeof(Request), PRESEND) < 0)
						fprintf(stderr, "An error has ocurred resending the request\n");
			    }
			    else
                {
				    sprintf(message, "ERROR 17 ENAMEEXISTS");
				    if(mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
						fprintf(stderr, "An error has ocurred sending the message\n");
			    }
                break;

            case WRT_C:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = WRT_W;
				    broadcast(workspace.id, req);
			    }
			    else
                {
				    if(is_open(file))
                       {
					    if(client_opened(file, req))
                        {
						    write_file(file, req.arg2, req.arg1);
						    sprintf(message,"OK");
					    }
					    else
						    sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
				    }
				    else
					    sprintf(message, "ERROR 80 ENOTOPENED");
				    free(req.arg2);
				    if(mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
						fprintf(stderr, "An error has ocurred sending the message\n");
			    }		
                break;

            case REA_C:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = REA_W;
				    broadcast(workspace.id, req);
			    }
			    else
                {
				    if(is_open(file))
                    {
					    if(client_opened(file, req))
						    read_file(file, message, req.arg1);
					    else
						    sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
				    }
				    else
					    sprintf(message, "ERROR 80 ENOTOPENED");
				    if(mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
						fprintf(stderr, "An error has ocurred sending the message\n");
			    }
                break;

            case CLO_C:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
                {
				    record = new_record(record, req.client_id, req.from_queue);
				    req.op = CLO_W;
				    broadcast(workspace.id, req);
			    }
			    else
                {
				    if(is_open(file))
                    {
					    if(client_opened(file, req))
                        {
						    sprintf(message, "OK");
						    close_file(file);
                            if(file -> can_delete == 1)
							{
								workspace.files = delete_file(file -> name, workspace.files);
								strcat(message, " (DELETED)");
							}
					    }
					    else
						    sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
				    }
				    else
					    sprintf(message, "ERROR 80 ENOTOPENED");
				    if(mq_send(req.from_queue, message, strlen(message), PCLIENT) < 0)
						fprintf(stderr, "An error has ocurred sending the message\n");
			    }
                break;  
              
             case LSD_W:
			    my_files(&workspace, message);
			    req.op = LSD_A;
			    req.arg2 = malloc(strlen(message) + 1);
			    strcpy(req.arg2, message);
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
               break;

             case OPN_W:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file == NULL)
				    req.err = EBADNAME;
			    else
                {
				    if(is_open(file))
					    req.err = EFILEOPEN;
				    else
					    req.arg0.arg0i = open_file(file, req.client_id);
			    }
			    req.op = OPN_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;

		    case DEL_W:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file == NULL)
				    req.err = EBADNAME;
			    else
                {
				    if(is_open(file))
                    {
					    req.err = EFILEOPEN;
                        file -> can_delete = 1;
                    }
				    else
					    workspace.files = delete_file(req.arg0.arg0c, workspace.files);
			    }
			    req.op = DEL_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;

		    case CRE_W:
			    file = search_name(workspace.files, req.arg0.arg0c);
			    if(file != NULL)
				    req.err = ENAMEEXISTS;
			    req.op = CRE_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;	

            case WRT_W:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
				    req.err = EBADFD;
			    else
                {
				    if(is_open(file))
                    {
					    if(client_opened(file, req))
						    write_file(file, req.arg2, req.arg1);
					    else
						    req.err = ENOTOPENEDBYCLIENT;
				    }
				    else
					    req.err = ENOTOPENED;
			    }
			    req.op = WRT_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;

            case REA_W:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
				    req.err = EBADFD;
			    else
                {
				    if(is_open(file))
                    {
					    if(client_opened(file, req))
                        {
						    read_file(file, message, req.arg1);
						    req.arg2 = malloc(strlen(message) + 1);
						    strcpy(req.arg2, message);
					    }
					    else
						    req.err = ENOTOPENEDBYCLIENT;
				    }
				    else
					    req.err = ENOTOPENED;
			    }
			    req.op = REA_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;

            case CLO_W:
			    file = search_fd(workspace.files, req.arg0.arg0i);
			    if(file == NULL)
				    req.err = EBADFD;
			    else
                {
				    if(is_open(file))
                    {
					    if(client_opened(file, req))
                        {
						    close_file(file);
                            if(file -> can_delete == 1)
							{
								workspace.files = delete_file(file -> name, workspace.files);
								req.arg1 = -1; 
							}
                        }
					    else
						    req.err = ENOTOPENEDBYCLIENT;							
				    }
				    else
					    req.err = ENOTOPENED;
			    }
			    req.op = CLO_A;
			    if(mq_send(req.from_queue, (char *) &req, sizeof(Request), PWORKER) < 0)
					fprintf(stderr, "An error has ocurred sending the request\n");
                break;

		    case LSD_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    change_record(recordaux, req.arg2);
				    if(full_record(recordaux))
                    {
					    if(mq_send(recordaux -> client_queue, recordaux -> lsd, strlen(recordaux -> lsd), PCLIENT) < 0)
							fprintf(stderr, "An error has ocurred sending the message\n");
					    record = delete_record(record, recordaux -> client_id);
				    }
			    }
                break;

			case OPN_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    switch(req.err)
                    {
				        case EBADNAME:
					        change_record(recordaux, NULL);
					        if(full_record(recordaux))
                            {
					            sprintf(message, "ERROR 56 EBADNAME");
					            if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        }
					        break;

				        case EFILEOPEN:
					        sprintf(message, "ERROR 45 EFILEOPEN");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        default: //O sea, no hay error en la respuesta
				            sprintf(message, "OK FD %d", req.arg0.arg0i);
				            if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
				            record = delete_record(record, recordaux -> client_id);
				            break;
			        }
                }
				break;

		    case DEL_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    switch(req.err)
                    {
				        case EBADNAME:
					        change_record(recordaux, NULL);
					        if(full_record(recordaux))
                            {
					            sprintf(message, "ERROR 56 EBADNAME");
					            if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        }
					        break;

				        case EFILEOPEN:
					        sprintf(message, "ERROR 45 EFILEOPEN (DELETION WHEN CLOSED)");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        default: //O sea, no hay error en la respuesta
				        	sprintf(message, "OK");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;
				    } 
			    }	
				break;

			case CRE_A:
				recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
					break;
				else
                {
					switch(req.err)
                    {
					case ENAMEEXISTS:
						sprintf(message, "ERROR 17 ENAMEEXISTS");
						if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
						record = delete_record(record, recordaux -> client_id);
						pthread_mutex_unlock(&sem);
						break;

					default: //O sea, no hay error en la respuesta
						change_record(recordaux, NULL);
						if(full_record(recordaux))
                        {
							workspace.files = create_file(req.arg0.arg0c, workspace.files);
							sprintf(message, "OK");
							if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
							record = delete_record(record, recordaux -> client_id);
							pthread_mutex_unlock(&sem);
						}
						break;
					}	
				}
				break;

            case WRT_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    switch(req.err)
                    {
				        case EBADFD:
					        change_record(recordaux, NULL);
					        if(full_record(recordaux))
                            {
					            sprintf(message, "ERROR 77 EBADFD");
					            if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        }
					        break;

					    case ENOTOPENED:
					        sprintf(message, "ERROR 80 ENOTOPENED");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        case ENOTOPENEDBYCLIENT:
					        sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        default: //O sea, no hay error en la respuesta
					        sprintf(message, "OK");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;
				    }
			    }
				break;

            case REA_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    switch(req.err)
                    {
				        case EBADFD:
					        change_record(recordaux, NULL);
					        if(full_record(recordaux))
                            {
					        sprintf(message, "ERROR 77 EBADFD");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        }
					        break;

				        case ENOTOPENED:
					        sprintf(message, "ERROR 80 ENOTOPENED");
					       if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;
				        case ENOTOPENEDBYCLIENT:
					        sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        default: //O sea, no hay error en la respuesta
					        sprintf(message, "%s", req.arg2);
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
                            free(req.arg2);
					        break;
				    }                                    			    
			    }
				break;

            case CLO_A:
			    recordaux = retrieve_record(record, req.client_id);
			    if (recordaux == NULL)
				    break;
			    else
                {
				    switch(req.err)
                    {
				        case EBADFD:
					        change_record(recordaux, NULL);
					        if(full_record(recordaux))
                            {
					            sprintf(message, "ERROR 77 EBADFD");
					            if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        }
					        break;

				        case ENOTOPENED:
					        sprintf(message, "ERROR 80 ENOTOPENED");
					        if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        case ENOTOPENEDBYCLIENT:
					        sprintf(message, "ERROR 81 ENOTOPENEDBYCLIENT");
					       if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;

				        default: //O sea, no hay error en la respuesta
					        sprintf(message, "OK");
					       if(mq_send(recordaux -> client_queue, message, strlen(message), PCLIENT) < 0)
									fprintf(stderr, "An error has ocurred sending the message\n");
					        record = delete_record(record, recordaux -> client_id);
					        break;
				    }
			    }
				break;

		    case BYE:
				recordaux = retrieve_record(record, req.client_id);
			    if (recordaux != NULL)
					record = delete_record(record, recordaux -> client_id);
				close_all_files(workspace.files, req.client_id);
				break;

	        default:
	            break;	
    	}		
	}	
	return NULL;
}
