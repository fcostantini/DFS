#include "Common.h"
#include "Socket_Process.h"
#include "Dispatcher.h"
#include "Worker.h"

//Esta funcion cierra y borra la mqueue asociada al cliente, luego envia un broadcast
//que cierra los archivos abiertos por el cliente que se desconecta y borra los registros
//correspondientes que pudieran haber quedado (en caso de desconexion sin BYE).
void end_session(Session session)
{

	Request req;
	char name[8];
	
	req.op = BYE;
	req.client_id = session.client_id;
	mq_close(session.client_queue);
	sprintf(name, "/c%d", session.client_id);
	mq_unlink(name);

	broadcast(SERV_BROAD, req);
}

int global_id=-1;
pthread_mutex_t id_lk = PTHREAD_MUTEX_INITIALIZER;

int checkname(char *buffer)
{
	int i, flag;

	for(i = 0; i < strlen(buffer); i++)
    {
		if((buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z') || (buffer[i] >= '0' && buffer[i] <= '9') || (buffer[i] == '.'))
			flag = 1;
		else{
			flag = 0;
			break;
		}
	}

	return flag;				
}

void* socket_process(void *arg)
{
	Session session = *(Session *)arg;
	int conn_s = session.conn_s;
	char buffer[BUFFER_IN], buffer_aux[BUFFER_OUT];
	int res;
	char aux[32];
	
	Request req;
	
	req.client_id = session.client_id;
	req.from_queue = session.client_queue;
	
	free(arg);

	fprintf(stderr, "New client %d connected to worker %d\n", session.client_id, session.worker_id);
	
	while(1)
	{
		req.op = NONE;

	    res = read(conn_s, buffer, BUFFER_IN);
		if (res <= 0)
		{
		    fprintf(stderr, "ERROR CON ID %d\n", session.client_id);
			close(conn_s);
			end_session(session);
			break;
		}
		buffer[res] = '\0';	
		if(!strncmp(buffer, "CON", 3))
        {
		    sprintf(buffer_aux, "OK ID %d\n", req.client_id);
			write(conn_s,buffer_aux, BUFFER_OUT); 
			break;
		}
		    else write(conn_s, "ERROR NOT IDENTIFIED\n", strlen("ERROR NOT IDENTIFIED\n"));

	}

	while(1)
	{
	    memset(buffer_aux, 0, BUFFER_OUT -1); 
		req.op = NONE;

  		res = read(conn_s, buffer, BUFFER_IN);
		if (res <= 0)
        {
		    fprintf(stderr, "Reading Error in Socket Process\n");
			close(conn_s);
			end_session(session);
			break;
		}
		buffer[res] = '\0';
        if(!strncmp(buffer, "CON", 3))
				write(conn_s,"ALREADY CONNECTED\n",strlen("ALREADY CONNECTED\n"));	
		else
            if(strlen(buffer) < 4)
			    write(conn_s,"ERROR 35 EBADCMD\n",strlen("ERROR 35 BEADCMD\n"));		
		else
			if(!strncmp(buffer, "LSD", 3))
			    req.op = LSD_C;			
		else
			if(!strncmp(buffer, "DEL", 3 ))
            {
				if(sscanf (buffer + 4, "%s", req.arg0.arg0c) != 1)
					write(conn_s,"ERROR DEL 61 EINSARG\n",strlen("ERROR DEL 61 EINSARG\n"));
				else
					if (!checkname(req.arg0.arg0c))
						write(conn_s,"ERROR DEL 56 EBADNAME\n",strlen("ERROR DEL 56 EBADNAME\n"));
					else
				    	req.op = DEL_C;
		    }
		else
			if(!strncmp(buffer, "CRE", 3))
            {
				if(sscanf (buffer + 4, "%s", req.arg0.arg0c) != 1){
					write(conn_s,"ERROR CRE 61 EINSARG\n",strlen("ERROR CRE 61 EINSARG\n"));}
				else
					if (!checkname(req.arg0.arg0c))
						write(conn_s,"ERROR CRE 56 EBADNAME\n",strlen("ERROR CRE 56 EBADNAME\n"));
					else
						req.op = CRE_C;
			}
		else
			if(!strncmp(buffer, "OPN", 3))
            {
				if(sscanf (buffer + 4, "%s", req.arg0.arg0c) != 1)
					write(conn_s,"ERROR OPN 61 EINSARG\n",strlen("ERROR OPN 61 EINSARG\n"));
				else
					if (!checkname(req.arg0.arg0c))
						write(conn_s,"ERROR OPN 56 EBADNAME\n",strlen("ERROR OPN 56 EBADNAME\n"));
					else
					    req.op = OPN_C;
			}
		else
			if(!strncmp(buffer, "WRT FD", 6))
            {
				if(sscanf (buffer + 7, "%d", &req.arg0.arg0i) != 1)
					write(conn_s,"ERROR WRT 61 EINSARG\n",strlen("ERROR WRT 61 EINSARG\n"));
				else
                {
					if(req.arg0.arg0i < 75)
						write(conn_s,"ERROR WRT 77 EBADFD\n",strlen("ERROR WRT 77 EBADFD\n"));
					else
                    {
						sprintf(aux, "WRT FD %d", req.arg0.arg0i);
						if(strncmp(buffer + strlen(aux) + 1, "SIZE", 4))
							write(conn_s,"ERROR 35 EBADCMD\n",strlen("ERROR 35 EBADCMD\n"));
						else
                        { 
							if(sscanf(buffer + strlen(aux) + 6, "%d", &req.arg1) != 1)
								write(conn_s,"ERROR WRT 61 EINSARG\n",strlen("ERROR WRT 61 EINSARG\n"));
							else
                            {
								if(req.arg1 < 0)
									write(conn_s,"ERROR WRT 82 EBADSIZE\n",strlen("ERROR WRT 82 EBADSIZE\n"));
								else
                                {					
									sprintf(aux, "WRT FD %d SIZE %d", req.arg0.arg0i, req.arg1);
									req.arg2 = malloc(req.arg1);
									memcpy(req.arg2, buffer + strlen(aux) + 1, req.arg1);
                                    int lenaux = strlen(req.arg2);             
                                    if(req.arg2[lenaux-1] == '\n')
                                        req.arg2[lenaux-1] = '\0';

									req.op = WRT_C;
								}
							}
						}
					}
				}

			}
		else
			if(!strncmp(buffer,  "REA FD", 6))
            {
				if(sscanf (buffer + 7, "%d", &req.arg0.arg0i) != 1)
					write(conn_s,"ERROR REA 61 EINSARG\n",strlen("ERROR REA 61 EINSARG\n"));
				else
                {
					if(req.arg0.arg0i < 75)
						write(conn_s,"ERROR REA 77 EBADFD\n",strlen("ERROR REA 77 EBADFD\n"));
					else
                    {
						sprintf(aux, "REA FD %d", req.arg0.arg0i);
						if(strncmp(buffer + strlen(aux) + 1, "SIZE", 4))
							write(conn_s,"ERROR 35 EBADCMD\n",strlen("ERROR 35 EBADCMD\n"));
						else
                        {
							if(sscanf(buffer + strlen(aux) + 6, "%d", &req.arg1) != 1)
								write(conn_s,"ERROR REA 61 EINSARG\n",strlen("ERROR REA 61 EEINSARG\n"));
							else
                            {
								if(req.arg1 < 0)
									write(conn_s,"ERROR REA 82 EBADSIZE\n",strlen("ERROR REA 82 EBADSIZE\n"));
								else
									req.op = REA_C;
							}
						}
					}
				}
			}
		else
			if(!strncmp(buffer,  "CLO FD", 6))
            {
				if(sscanf (buffer + 7, "%d", &req.arg0.arg0i) != 1)
					write(conn_s,"ERROR CLO 61 EINSARG\n",strlen("ERROR CLO 61 EINSARG\n"));
				else
                {
					if(req.arg0.arg0i < 75)
						write(conn_s,"ERROR CLO 77 EBADFD\n",strlen("ERROR CLO 77 EBADFD\n"));
					else
						req.op = CLO_C;
				}
			}
		else
			if(!strncmp(buffer,  "BYE", 3))
            {
				write(conn_s,"OK\n",strlen("OK\n"));
				close(conn_s);
				end_session(session);
				break;
	    	}
		else
			write(conn_s,"ERROR 35 EBADCMD\n",strlen("ERROR 35 EBADCMD\n"));

		switch(req.op)
        {
		    case NONE:
			    continue;
		    default:
			    if(mq_send(session.worker_queue, (char *)&req, sizeof(Request), PCLIENT) < 0)
		    	    write(conn_s,"An error has ocurred sending the request\n",strlen("An error has ocurred sending the request\n")); 
	   		    if(mq_receive(session.client_queue, buffer_aux, BUFFER_OUT, NULL) < 0)
	        	    write(conn_s, "ERROR: Failure to retrieve the message from the queue\n", strlen("ERROR: Failure to retrieve the message from the queue\n"));		
			    strcat(buffer_aux, "\n");	
	    	    write(conn_s, buffer_aux, strlen(buffer_aux));
		}

		memset(buffer, 0, BUFFER_IN - 1);
		memset(buffer_aux, 0, BUFFER_OUT -1); 		

	}

	fprintf(stderr,"Client %d disconnected\n", session.client_id);
	
	return NULL;
}
