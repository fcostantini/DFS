#include "Common.h"

#define PCLIENT 0
#define PRESEND 1
#define PWORKER 2
#define PSERVER 3

struct mq_attr attr;  
mqd_t *worker_messages;
pthread_t workers[NUMBER_OF_WORKERS];

typedef enum{
LSD_C, OPN_C, DEL_C, CRE_C, WRT_C, REA_C, CLO_C, LSD_W, OPN_W, DEL_W, CRE_W, WRT_W, REA_W, CLO_W, LSD_A, OPN_A, DEL_A, CRE_A, WRT_A, REA_A, CLO_A, BYE, NONE 
}Operation;

typedef enum
{
NERR, EFILEOPEN, EBADFD, ENAMEEXISTS, EBADNAME, ENOTOPENEDBYCLIENT, ENOTOPENED
}Error;

//Pedidos (para clientes y comunicacion entre workers)
typedef struct{
	Operation op;
	Error err;
	union{
		char arg0c[32];
		int arg0i;
	}arg0;
	int arg1;
	char *arg2;
	int client_id;
	mqd_t from_queue;
}Request;

//Estructura para llevar un registro de la operacion actual (explicada en informe)
typedef struct _Record{
	char lsd[LSD_SIZE];
	int client_id;
	int counter;
	mqd_t client_queue;
	struct _Record *next;
}Record;

//Archivos
typedef struct _File{
	char name[32];	
	char *content;
	int fd;
	int opener;
	int position;
	int size;
    int can_delete;
	struct _File *next;
}File;

//Workers
typedef struct{
	int id;
	File *files;
	mqd_t msg_queue;
}Worker;

//Funciones de archivos
int new_fd (void);
File *search_name(File *files, char *name);
File *search_fd(File *files, int fd);
void my_files(Worker *w, char *list);
File *create_file(char *name, File *files);
File *delete_file(char *name, File *list);
int open_file(File *file, int opener);
int close_file(File *file);
void close_all_files(File *list, int client_id);
int write_file(File *file, char *text, int size);
char *read_file(File *file, char *buffer, int number);
int is_open(File *file);
int client_opened(File *file, Request req);

//Funciones de registros
Record *new_record(Record *list, int client_id, mqd_t client_queue);
Record *retrieve_record(Record *list, int client_id);
void change_record(Record *record, char *to_add);
int full_record(Record *record);
Record *delete_record(Record *list, int client_id);

//Funciones de workers
int initialize_workers();
void broadcast(int from, Request req);
void *worker_fun(void *worker);
