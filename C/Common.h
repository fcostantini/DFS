#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>			 
#include <sys/types.h>			 
#include <arpa/inet.h>			
#include <unistd.h>				
#include <string.h>	
#include <stdlib.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define NUMBER_OF_WORKERS 5
#define MAX_MESSAGES 10
#define MESSAGE_SIZE 1024
#define BUFFER_IN 1023
#define BUFFER_OUT 1025
#define LSD_SIZE 768
#define SERV_BROAD 128
