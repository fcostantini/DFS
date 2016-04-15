all:
	gcc *.c -lrt -lpthread -o sv -g

Dispatcher.o: Dispatcher.c Common.h Dispatcher.h Worker.h \
 Socket_Process.h
Server.o: Server.c Common.h Worker.h Dispatcher.h
Socket_Process.o: Socket_Process.c Common.h Socket_Process.h Dispatcher.h \
 Worker.h
Worker.o: Worker.c Worker.h Common.h

clean:
	rm *.c~ *.h~ sv
