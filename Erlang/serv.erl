-module(serv).
-compile(export_all).
-define (NUMBER_OF_WORKERS, 5).

%Inicializacion del servidor     
init_serv() ->
	io:format("Initializing Distributed File System (DFS) server~n", []),
	io:format("Launching workers...", []),
	Workers = lists:reverse(init_workers(?NUMBER_OF_WORKERS)),	%se revierte porque init los genera [5,4,3,2,1]
	worker_ring(Workers, 1),
	register(fd, spawn(?MODULE, gfd, [75])), %Registro para manejo mas facil
	case gen_tcp:listen(8000, [list, {active,false}]) of
		{ok, ListenSocket}  ->  dispatcher(ListenSocket, Workers, 1);
        _                   ->  io:format("Error creating listening socket.~n", [])
    end.

%Dispatcher	
dispatcher(ListenSocket, Workers, Id) ->
    case gen_tcp:accept(ListenSocket) of
        {ok, Conn_s} ->  spawn(?MODULE, dispatcher, [ListenSocket, Workers, Id+1]),
						 Worker_id = Id rem ?NUMBER_OF_WORKERS + 1,
						 Worker = lists:nth(Worker_id, Workers),
						 handle_client(Conn_s, Id, Worker, Worker_id);
		_            ->  dispatcher(ListenSocket, Workers, Id)
	end.

%Inicializacion de workers
init_workers(0)	->	io:format("Workers launched~n"), [];
init_workers(N)	->	Worker = spawn(?MODULE, worker, []),
					[Worker | init_workers(N-1)].

%Inicializacion del anillo (que permite el broadcast)
worker_ring(Workers, N) -> case N of
		?NUMBER_OF_WORKERS 	-> lists:last(Workers) ! {lists:nth(1, Workers)};
		_					-> lists:nth(N, Workers) ! {lists:nth(N+1, Workers)}, worker_ring(Workers, N+1)
	end.
						   
%Conexion del cliente		
handle_client(Conn_s, Id, Worker, Worker_id) ->
	case gen_tcp:recv(Conn_s, 0) of
		{ok, Buff_in} -> Req =  parse_request(lists:sublist(Buff_in, length(Buff_in) - 1)),
					     case Req of
				            {con} -> 	Buff_out = "OK ID " ++ integer_to_list(Id) ++ "\n",
									    gen_tcp:send(Conn_s, Buff_out),
						        	    io:format("New client ~p connected to worker ~p~n", [Id, Worker_id]),
							        	handle_client(Conn_s, Id, Worker);
						    _   	->	Buff_out = "ERROR NOT IDENTIFIED\n",
									    gen_tcp:send(Conn_s, Buff_out),
								        handle_client(Conn_s, Id, Worker, Worker_id)
						 end;
		{error, closed} ->  io:format("Disconnected client ~p~n", [Id])
	end.

%Operaciones normales		
handle_client(Conn_s, Id, Worker) ->
	case gen_tcp:recv(Conn_s, 0) of
		{ok, Buff_in}      -> 	Req = parse_request(lists:sublist(Buff_in, length(Buff_in) - 1)),
								case Req of
									{error, badcmd} 	-> 	Buff_out = "ERROR 35 EBADCMD\n",
															gen_tcp:send(Conn_s, Buff_out),
															handle_client(Conn_s, Id, Worker);
									{error, badfd}  	-> 	Buff_out = "ERROR 77 EBADFD\n",
															gen_tcp:send(Conn_s, Buff_out),
															handle_client(Conn_s, Id, Worker);
									{error, badsize} 	-> 	Buff_out = "ERROR 82 EBADSIZE\n",
															gen_tcp:send(Conn_s, Buff_out),
															handle_client(Conn_s, Id, Worker);
									{error, insarg} 	-> 	Buff_out = "ERROR 61 EINSARG\n",
															gen_tcp:send(Conn_s, Buff_out),
															handle_client(Conn_s, Id, Worker);
									{bye}				-> 	Worker ! {{bye}, self()},
															receive
																Msg -> 	Buff_out = Msg ++ "\n",
																		gen_tcp:send(Conn_s, Buff_out),
																		gen_tcp:close(Conn_s)
															end,
															io:format("Client ~p disconnected~n",[Id]);
									{con} 			    -> 	Buff_out = "ALREADY CONNECTED",
															gen_tcp:send(Conn_s, Buff_out),
															handle_client(Conn_s, Id, Worker);
									_					->	Worker ! {Req, self()},
															receive
																Msg -> 	Buff_out = Msg ++ "\n",
																		gen_tcp:send(Conn_s, Buff_out),
																		handle_client(Conn_s, Id, Worker)
															end
								end;
		{error, closed} ->  io:format("Disconnected client ~p~n", [Id])
	end.												
																
%Parseo de la solicitud
parse_request(R) ->
	case string:tokens(R, " ") of
		["CON"]												->	{con};
		["LSD"]												->	{lsd_c};
		["DEL"]												->	{error, insarg};
		["DEL", Arg0c]										->	{del_c, Arg0c};
		["CRE"]												->	{error, insarg};
		["CRE", Arg0c]										->	{cre_c, Arg0c};
		["OPN"]												->	{error, insarg};
		["OPN", Arg0c]										->	{opn_c, Arg0c};
		["WRT"]												->	{error, insarg};
		["WRT", "FD"]										->	{error, insarg};
		["WRT", "FD", _]								    ->	{error, insarg};
		["WRT", "FD", _, "SIZE"]	    				    ->	{error, insarg};
		["WRT", "FD", _, "SIZE", _]     			        ->	{error, insarg};
		["WRT" |["FD"| [Arg0i| ["SIZE"| [Arg1| Arg2]]]]]	->	if 	Arg0i < 75	-> 	{error, badfd};
																	Arg1 < 1	->	{error, badsize};
																	true		-> 	{wrt_c, list_to_integer(Arg0i), list_to_integer(Arg1), string:join(Arg2, " ")} %Sin espacios en la escritura
                                                                end;
		["REA"]												->	{error, insarg};															
		["REA", "FD"]										->	{error, insarg};
		["REA" ,"FD", _]								    ->	{error, insarg};
		["REA", "FD", _, "SIZE"]						    ->	{error, insarg};
		["REA", "FD", Arg0i, "SIZE", Arg1]					->	if 	Arg0i < 75	-> 	{error, badfd};
																	Arg1 < 1	->	{error, badsize};
																	true		->	{rea_c, list_to_integer(Arg0i), list_to_integer(Arg1)}
                                                                end;
		["CLO"]												->	{error, insarg};
		["CLO"| "FD"]										->	{error, insarg};
		["CLO", "FD", Arg0i]								->	if 	Arg0i < 75	-> 	{error, badfd};
																	true		->	{clo_c, list_to_integer(Arg0i)}
                                                                end;
		["BYE"]												->	{bye};
		_													->	{error, badcmd}
    end.

%Worker "vacio" que espera al anillo
worker() -> receive
				{W}	-> worker([], W)
			end.		
		
%Worker
worker(Files, Next)	->
	receive
	    %Solicitudes de clientes
		{Req, Client}	->	case Req of
			{lsd_c}						->	Lsd = my_files(Files),
		    								Next ! {{lsd_w, Lsd}, Client, self()};
			{opn_c, Arg0c}				->	case search_name(Files, Arg0c) of
												{yes, File} ->	case who_opened(File, Client) of
																	noone	->	NewFiles = open_file(File, Files, Client),
		    																	Client ! "OK FD " ++ integer_to_list(extract_fd(lists:nth(1, NewFiles))),
																				worker(NewFiles, Next);
																	_		->	Client ! "ERROR 45 EFILEOPEN"
																end;
												{no}		->	Next ! {{opn_w, Arg0c}, Client, self()}
											end;						
			{del_c, Arg0c}				->	case search_name(Files, Arg0c) of
												{yes, File} ->	case who_opened(File, Client) of
																	noone	->	Client ! "OK",
		    																	New_Files = lists:delete(File, Files),
																				worker(New_Files, Next);
																	_		->	Client ! "ERROR 45 EFILEOPEN"
																end;
												{no}		->	Next ! {{del_w, Arg0c}, Client, self()}
											end;
			{cre_c, Arg0c}				->	case search_name(Files, Arg0c) of
												{yes, _}	->	Client ! "ERROR 17 ENAMEEXISTS";
												{no}		-> 	Next ! {{cre_w, Arg0c}, Client, self()}
											end;										
			{wrt_c, Arg0i, Arg1, Arg2}	->	case search_fd(Files, Arg0i) of
												{yes, File}	->	case who_opened(File, Client) of
																	me		->	NewFiles = write_file(File, Files, Arg1, Arg2),
																				Client ! "OK",
		    																	worker(NewFiles, Next);
																	noone	->	Client ! "ERROR 80 ENOTOPENED";
																	notme	->	Client ! "ERROR 81 ENOTOPENEDBYCLIENT"
																end;
												{no}		-> 	Next ! {{wrt_w, Arg0i, Arg1, Arg2}, Client, self()}
		    								end;
			{rea_c, Arg0i, Arg1}		->	case search_fd(Files, Arg0i) of
												{yes, File}	->	case who_opened(File, Client) of
																	me		->	{Read, NewFiles} = read_file(File, Files, Arg1),
																				Client ! "OK " ++ Read,
																				worker(NewFiles, Next);
																	noone	->	Client ! "ERROR 80 ENOTOPENED";
																	notme	->	Client ! "ERROR 81 ENOTOPENEDBYCLIENT"
																end;
												{no}		-> 	Next ! {{rea_w, Arg0i, Arg1}, Client, self()}
											end;
			{clo_c, Arg0i}				->	case search_fd(Files, Arg0i) of
												{yes, File}	->	case who_opened(File, Client) of
																	me		->	NewFiles = close_file(File, Files),
																				Client ! "OK",
																				worker(NewFiles, Next);
																	noone	->	Client ! "ERROR 80 ENOTOPENED";
																	notme	->	Client ! "ERROR 81 ENOTOPENEDBYCLIENT"
																end;					
												{no}		-> 	Next ! {{clo_w, Arg0i}, Client, self()}
											end;
		    {bye}						->	NewFiles = close_all_files(Files, Client),
											Next ! {{bye}, Client, self()},
											worker(NewFiles, Next)
								end;
		%Solicitudes de workers				
		{Req, Client, From}	->	case Req of
				{lsd_w, Lsd} when From == self()		->	Msg = "OK " ++ Lsd,
															Client ! Msg;
		    	{lsd_w, Lsd}							->	NewLsd = Lsd ++ my_files(Files),
															Next ! {{lsd_w, NewLsd}, Client, From};
				{opn_w, opened, Fd} when From == self()	->	Client ! "OK FD " ++ Fd;
				{opn_w, _} when From == self()			->	Client ! "ERROR 56 EBADNAME";
				{opn_w, Arg0c}							->	case search_name(Files, Arg0c) of
																{yes, File}	->	case who_opened(File, Client) of
		            																noone	->	New_Files = open_file(File, Files, Client),
																								Fd = integer_to_list(extract_fd(lists:nth(1, New_Files))),
																								Next ! {{opn_w, opened, Fd}, Client, From},
																								worker(New_Files, Next);
																					_		->	Next ! {{error, fileopened}, Client, From}
																				end;
																{no}		-> Next ! {Req, Client, From}
															end;
				{del_w, deleted} when From == self()	->	Client ! "OK";
				{del_w, _} when From == self()			->	Client ! "ERROR 56 EBADNAME";
				{del_w, Arg0c}							-> 	case search_name(Files, Arg0c) of
																{yes, File}	->	case who_opened(File, Client) of
    			    											                	noone	->	Next ! {{del_w, deleted}, Client, From},
    		    													                			New_Files = lists:delete(File, Files),
				    												                			worker(New_Files, Next);
				    												                _		->	Next ! {{error, fileopened}, Client, From}
				            													end;
																{no}		-> Next ! {Req, Client, From}
															end;
    			{cre_w, Arg0c} when From == self()		->	NewFiles = [create_file(Arg0c) | Files],
															Client ! "OK",
															worker(NewFiles, Next);
    			{cre_w, Arg0c}							->	case search_name(Files, Arg0c) of
																{yes, _}	->	Next ! {{error, nameexists}, Client, From};
																{no}		->	Next ! {Req, Client, From}
															end;
				{wrt_w, wrote} when From == self()		->	Client ! "OK";
				{wrt_w, _, _, _} when From == self()	->	Client ! "ERROR 77 EBADFD";
				{wrt_w, Arg0i, Arg1, Arg2}				->	case search_fd(Files, Arg0i) of
																{yes, File}	->	case who_opened(File, Client) of
																					me		->	NewFiles = write_file(File, Files, Arg1, Arg2),
																								Next ! {{wrt_w, wrote}, Client, From},
																								worker(NewFiles, Next);
																					noone	->	Next ! {{error, notopened}, Client, From};
																					notme	->	Next ! {{error, notopenedbyclient}, Client, From}
																				end;
    															{no}		-> 	Next ! {Req, Client, From}
														end;
				{rea_w, read, Read} when From == self()	->	Client ! "OK " ++ Read;
				{rea_w, _, _} when From == self()		->	Client ! "ERROR 77 EBADFD";
				{rea_w, Arg0i, Arg1}					->	case search_fd(Files, Arg0i) of
																{yes, File}	->	case who_opened(File, Client) of
																					me		->	{Read, NewFiles} = read_file(File, Files, Arg1),
																								Next ! {{rea_w, read, Read}, Client, From},
																								worker(NewFiles, Next);
																					noone	->	Next ! {{error, notopened}, Client, From};
																					notme	->	Next ! {{error, notopenedbyclient}, Client, From}
			    																end;
																{no}		-> 	Next ! {Req, Client, From}
			    											end;					
				{clo_w, closed} when From == self()		->	Client ! "OK";						
				{clo_w, _} when From == self()			->	Client ! "ERROR 77 EBADFD";						
				{clo_w, Arg0i}							->	case search_fd(Files, Arg0i) of
																{yes, File}	->	case who_opened(File, Client) of
																					me		->	NewFiles = close_file(File, Files),
				    																			Next ! {{clo_w, closed}, Client, From},
																								worker(NewFiles, Next);
				    																noone	->	Next ! {{error, notopened}, Client, From};
																					notme	->	Next ! {{error, notopenedbyclient}, Client, From}
																				end;
																{no}		-> 	Next ! {Req, Client, From}
															end;
				{bye} when From == self()				->	Client ! "OK";
				{bye}									->	NewFiles = close_all_files(Files, Client),
																		Next ! {{bye}, Client, From},
				    													worker(NewFiles, Next);
				%Errores
				{error, fileopened} when From == self()			->	Client ! "ERROR 45 EFILEOPEN";
				{error, notopened} when From == self()			->	Client ! "ERROR 80 ENOTOPENED";						
				{error, notopenedbyclient} when From == self()	->	Client ! "ERROR 81 ENOTOPENEDBYCLIENT";						
				{error, nameexists} when From == self()			->	Client ! "ERROR 17 ENAMEEXISTS";
				_												->	Next ! {Req, Client, From}
		end
	end,
	worker(Files, Next).

%Operaciones de archivos (tienen la forma {Nombre, FD, Contenido, Puntero, Opener})
%fd global (con un proceso)
gfd(N)	->	receive
				{get_fd, Get}	->	Get ! (N+1)
			end,
			gfd(N+1).

%Obtiene un fd (usando el global)	
get_fd() ->	fd ! {get_fd, self()},
			receive
				X -> X
			end.

%Devuelve el fd de un archivo
extract_fd({_, Fd, _, _, _})	->	Fd.			

%Busca un archivo por el nombre			
search_name(Files, Name)	->	case lists:filter(fun({Namee, _, _, _, _}) -> Name == Namee end, Files) of
									[]		->	{no};
									[File]	->	{yes, File}
								end.

%Busca un archivo por el fd								
search_fd(Files, Fd)	->	case lists:filter(fun({_, Fdd, _, _, _}) -> Fd == Fdd end, Files) of
									[]		->	{no};
									[File]	->	{yes, File}
								end.

%Lista los archivos de un worker								
my_files([]) 						->	[];
my_files([{Name, _, _, _, _} | Xs])	->	Lsd = Name ++ " ",
										Lsd ++ my_files(Xs).

%Crea un archivo (se agrega a la lista en worker)
create_file(Name)	->	{Name, 0, "", 1, false}.
		
%Abre un archivo
open_file({Name, 0, Content, Ptr, false}, Files, Client)->	NewFd = get_fd(),
														    FilesAux = lists:delete({Name, 0, Content, Ptr, false}, Files), 
                                                            [{Name, NewFd, Content, 1, Client} | FilesAux].												    
									
%Cierra un archivo
close_file({Name, Fd, Content, Ptr, Opener}, Files)	->	FilesAux = lists:delete({Name, Fd, Content, Ptr, Opener}, Files),
														[{Name, 0, Content, 1, false} | FilesAux].
														
%Cierra todos los archivos asociados a un cliente
close_all_files([], _)	->	[];
close_all_files([{Name, Fd, Content, Ptr, Opener} | Xs], Client)	->	if 	Opener == Client	->	[{Name, 0, Content, 1, false} | close_all_files(Xs, Client)];
																			true				->	[{Name, Fd, Content, Ptr, Opener} | close_all_files(Xs, Client)]
																		end.
																		
%Escribe en un archivo
write_file({Name, Fd, Content, Ptr, Opener}, Files, Size, Text)	->	FilesAux = lists:delete({Name, Fd, Content, Ptr, Opener}, Files),
																	[{Name, Fd, Content ++ string:substr(Text, 1, Size), Ptr, Opener} |	FilesAux].

%Lee un archivo
read_file({Name, Fd, Content, Ptr, Opener}, Files, Size)	->	if 	Ptr >= length(Content)			->	{"SIZE 0", Files};
																	Size =<	length(Content) - Ptr	->	FilesAux = lists:delete({Name, Fd, Content, Ptr, Opener}, Files),
																										{"SIZE " ++ integer_to_list(Size) ++ " " ++ string:substr(Content, Ptr, Size), [{Name, Fd, Content, Ptr + Size, Opener} | FilesAux]};
																	true							->	FilesAux = lists:delete({Name, Fd, Content, Ptr, Opener}, Files),
																										{"SIZE " ++ integer_to_list(length(Content) - Ptr + 1) ++ " " ++ string:substr(Content, Ptr), [{Name, Fd, Content, length(Content) + 1, Opener} | FilesAux]}	
																end.										

%Se fija quien abrio el archivo indicado
who_opened({_, _, _, _, Opener}, Client) -> if Opener == false  -> noone;
                                               Opener == Client -> me;
                                               true             -> notme
                                            end.
