#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <regex.h>
#include <sys/msg.h>
#include "hserver.h"

int SERVER_ACTIVO, ID_COLA;
char * ERROR_MSJ;
key_t K_COLA;
//elem_cola COLA_GRAL;

lista_pt *thread = NULL;

pthread_mutex_t mutex_listapt = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_archivo_clientes = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) 
{
	int des_servidor;
	pthread_t th_conex;

	system("clear"); 

	if ((des_servidor = iniciar_servidor(PUERTO)) == -1)
		exit(EXIT_FAILURE);

	if (pthread_create(&th_conex, NULL, ejec_servidor, (void*) &des_servidor) != 0)
	{
		printf ("ERROR PTHREAD SERVIDOR\n");
		return -1;
	}

	/* Esperar a que termine el thread de ejecución. */
	pthread_join(th_conex, NULL);	
	exit(EXIT_SUCCESS);
}

int iniciar_servidor (int puerto)
{
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
	{ 
		perror("socket");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons (puerto);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("bind");
		return -1;
	}

	if(listen (sockfd, MAX_QUE) < 0)
	{
		perror("listen");
		return -1;
	}

	SERVER_ACTIVO = ON;
	printf ("## Homero-Server disponible, puerto %d\n", ntohs(serv_addr.sin_port));

//	printf ("## creando logs...\n");
/*	if ((archivo_crear("clientes") != EXITO) || (archivo_crear("serv.log") != EXITO) || (archivo_crear("chat.log") != EXITO))
	{
		printf("ERROR creando logs!!!\n");
		return -1;
	}*/
	return sockfd;
}

void* ejec_servidor(void *ptr)
{
	int *iptr, des_servidor, des_cliente;

	iptr = (int *) ptr;
	des_servidor = *iptr;

	if(iniciar_cola() == ERROR)
	{
		printf("Error al abrir cola de mensajes. Abortado\n");
		exit(EXIT_FAILURE);
	}

	while(SERVER_ACTIVO)
	{
		des_cliente=aceptar_conexion(des_servidor);
		if (nuevo_cliente(&thread, des_cliente) < 0)
			exit(EXIT_FAILURE);
		list_print(thread);
	}
	return NULL;
}

int iniciar_cola()
{
	K_COLA = ftok ("/bin/ls", 33);		//Primero obtengo un KEY para crear la Q
	if (K_COLA == (key_t)-1)
		return ERROR;

	ID_COLA = msgget (K_COLA, 0600 | IPC_CREAT);	//Segundo obtengo el ID de la Q creada
	if (ID_COLA == -1)
		return ERROR;

	return EXITO;
}

int aceptar_conexion(int socket)
{
	int newsockfd;
	size_t clilen;
	struct sockaddr_in cli_addr;

	clilen = sizeof(cli_addr);
	newsockfd = accept(socket, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0) 
	{
		perror("accept");
		return -1;
	}
	return newsockfd;
}

int nuevo_cliente(lista_pt **n, int dsc)
{
	pthread_t new;

	if (pthread_create(&new, NULL, ejec_cliente, (void *) &dsc) != 0)
	{
		printf ("ERROR PTHREAD CLIENTE\n");
		return -1;
	}
	pthread_mutex_lock(&mutex_listapt);
	list_add(n, new, dsc);
	pthread_mutex_unlock(&mutex_listapt);
	return 0;
}

void *ejec_cliente(void *ptr)
{
	int *iptr, mi_descriptor;//, fl_recv = OFF;
	int CLIENTE_ACTIVO = ON;
	size_t clilen;
	struct sockaddr_in cli_addr;
	struct sockaddr_in* s;
	char ipstr[INET_ADDRSTRLEN], buffer[TAM], resp_servidor[TAM];

	iptr = (int *) ptr;
	mi_descriptor = *iptr;

	clilen = sizeof(cli_addr);
	s = (struct sockaddr_in *) &cli_addr;
	getpeername(mi_descriptor, (struct sockaddr*) &cli_addr, &clilen);

	printf("hilo %lu del cliente %s\n", pthread_self(), inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr));

	//Hilo en ejecucion y listo para laburar y escuchar al cliente.
	//Bucle principal...
	while ((CLIENTE_ACTIVO == ON) && (getpeername(mi_descriptor, (struct sockaddr*) &cli_addr, &clilen) == 0))
	{
		//Esperar por el primer msj.
		memset(buffer, '\0', TAM);
		if (recv(mi_descriptor, buffer, TAM, MSG_DONTWAIT) > 0)
//			fl_recv = ON;
		/*if (read(mi_descriptor, buffer, TAM) < 0) 
		{
			perror("read");
			exit(EXIT_FAILURE);
		}*/

//		if (fl_recv == ON)
		{
			switch (verificar_msj(buffer, mi_descriptor))
			{
				case EXIT_CODE:
					CLIENTE_ACTIVO = OFF;
					continue;
				case CTRL_CODE:
					printf("MSJ de CONTROL recibido: %s", buffer);
					if (write(mi_descriptor, "__OK__\n", TAM) < 0) 
					{
						perror("write");
						exit(EXIT_FAILURE);
					}
					break;

				case EXITO_REG_CLIENTE:
					//strcat(resp_servidor, "CTRL QUETAL");
					//broadcast_clientes("CTRL ENTRO\n");
					if (write(mi_descriptor, "CTRL QUETAL\n", TAM) < 0) 
					{
						perror("write");
						exit(EXIT_FAILURE);
					}
					break;

				case ERROR_REG_CLIENTE:
					//strcat(resp_servidor, "CTRL FUERA ");
					//strcat(resp_servidor, ERROR_MSJ);
					if (write(mi_descriptor, "CTRL FUERA\n", TAM) < 0) 
					{
						perror("write");
						exit(EXIT_FAILURE);
					}
					break;

				case LISTAR_CODE:
					strcpy(resp_servidor, "# Lista de usuarios: \n");
					strcat(resp_servidor, listar_clientes(mi_descriptor));
					if (write(mi_descriptor, resp_servidor, TAM) < 0) 
					{
						perror("write");
						exit(EXIT_FAILURE);
					}
					break;
	
				default:
					printf("Msj recibido: %s",buffer);
					break;
			}
			//fl_recv = OFF;
		}

	}
	printf("hilo %lu finalizado\n", pthread_self());
	//deregistrar_usuario(mi_descriptor);
	list_remove(list_search_d2(&thread, mi_descriptor));
	return NULL;
}

/* esta verifica el msj recibido en el buffer y llama a la función correspondiente */
int verificar_msj(char * buffer_entrada, int ssock)
{
	char temp[TAM];

	strcpy(temp, buffer_entrada);
	if (strcmp(temp, "exit\n") == 0)
		return EXIT_CODE;

	if (strcmp(temp, "CTRL LISTAR\n") == 0)
		return LISTAR_CODE;

	if (strstr(buffer_entrada,"CTRL HOLA") != NULL)
	{
		if (registrar_usuario(ssock, strtok(buffer_entrada," ")) == EXITO)
			return EXITO_REG_CLIENTE;
		else
			return ERROR_REG_CLIENTE;
	}

	return 0;
}

/* si el nombre como argumento es válido, registra al usuario */
int registrar_usuario(int id, char *nombre)
{
	pthread_mutex_lock(&mutex_archivo_clientes);
	if (archivo_buscar("clientes",nombre) == ERROR) //pregunto por error porque no quiero que este en el archivo...
	{
		if(archivo_agregar("clientes", nombre) == EXITO)
		{
			pthread_mutex_unlock(&mutex_archivo_clientes);
			pthread_mutex_lock(&mutex_listapt);
			if(lista_add_nombre(id, nombre) == ERROR)
				return ERROR;
			pthread_mutex_unlock(&mutex_listapt);
			return EXITO;
		}
		else
		{
			ERROR_MSJ = "archivo_agregar()";
			pthread_mutex_unlock(&mutex_archivo_clientes);
			return ERROR;
		}
	}
	pthread_mutex_unlock(&mutex_archivo_clientes);
	ERROR_MSJ = "\"Error registrando usuario, el nombre ya está siendo usado\"";
	return ERROR;
}

/* borra el registro de un usuario para que otro se pueda conectar con ese nombre */
void deregistrar_usuario(int descriptor)
{
	return;
}

/*envía un msj a todos los clientes */
int broadcast(int origen, char *msj)
{
	lista_pt *n = (lista_pt *)malloc(sizeof(lista_pt));
	if (n == NULL)
		return ERROR;
	n = thread;
    while (n != NULL) 
	{
        send(n->_id_socket_, msj, TAM, MSG_DONTWAIT);
        n = n->_next_;
    }
	return EXITO;
}

/* Devuelve una lista de clientes registrados, excluyendo al que llama la funcion */
char * listar_clientes(int socket)
{
	char* lista = (char*) malloc (sizeof(char)*TAM);
	strcpy(lista, archivo_listar("clientes"));
	return lista;
}

/********** Funciones que tratan con archivos *********************/

int archivo_buscar(char *nombre, char *cadena)
{
	char * buffer;

	buffer = archivo_listar(nombre);

	if (strstr(buffer,cadena) != NULL)
		return EXITO;
	else
		return ERROR;
}

char* archivo_listar(char* nombre)
{
	FILE *pf;
	char * buffer;
	long lsize;
	size_t result;

	pf = fopen(nombre, "r");
	if (pf==NULL) 
	{
		fclose(pf);
		return NULL;		
	}

    fseek (pf, 0, SEEK_END);
    lsize = ftell (pf);
    rewind (pf);

	buffer = (char*) malloc (sizeof(char)*lsize);
	if (buffer == NULL)
	{
		fclose(pf);
		return NULL;
	}

	result = fread (buffer,1,lsize,pf);
	if (result != lsize)
	{
		fclose(pf);
		return NULL;
	}
	fclose(pf);
	return buffer;
}

int archivo_agregar(char* nombre_archivo, char* texto)
{
	FILE *pf;

	if ((pf=fopen(nombre_archivo, "a")) == NULL) 
	{
		fclose(pf);
		return ERROR;		
	}

	fputs(strcat(texto,"\n"),pf);
	fclose(pf);
	return EXITO;
}

int archivo_borrar(char* nombre_archivo, char* texto)
{
	char *temp1, *temp2, *str, *lista_nombres;
	FILE *pf;
	if ((pf=fopen(nombre_archivo, "w+r")) == NULL)
	{
		fclose(pf);
		return ERROR;
	}

	lista_nombres = malloc(sizeof(char)*TAMNOM*TAMNOM);
	strcpy(lista_nombres, archivo_listar(nombre_archivo));

	temp1 = malloc(sizeof(char)*TAMNOM*TAMNOM);
	temp2 = malloc(sizeof(char)*TAMNOM*TAMNOM);

	//primero ir hasta que encuentre el nombre...
	while (strcmp(texto, fgets(str, TAMNOM, pf)) != 0)
		strcpy(temp1, str);

	//ahora ir hasta el final...
	while (fgets(str, TAMNOM, pf) != NULL)
		strcpy(temp1, str);

	
}


/********** Funciones que tratan con la lista de threads **********/

lista_pt *list_add(lista_pt **p, pthread_t i, int d) 
{
    lista_pt *n = (lista_pt *)malloc(sizeof(lista_pt));
    if (n == NULL)
        return NULL;
    n->_next_ = *p;
    *p = n;
    n->_id_thread_ = i;
	n->_id_socket_ = d;
    return n;
}

void list_remove(lista_pt **p) 
{ 
    if (*p != NULL) 
	{
        lista_pt *n = *p;
        *p = (*p)->_next_;
        free(n);
    }
}
 
lista_pt **list_search_d1(lista_pt **n, pthread_t i) 
{
    while (*n != NULL) 
	{
        if ((*n)->_id_thread_ == i) 
			return n;
        n = &(*n)->_next_;
    }
    return NULL;
}

lista_pt **list_search_d2(lista_pt **n, int i) 
{
    while (*n != NULL) 
	{
        if ((*n)->_id_socket_ == i) 
			return n;
        n = &(*n)->_next_;
    }
    return NULL;
}

lista_pt **list_search_d3(lista_pt **n, char* nom) 
{
    while (*n != NULL) 
	{
        if (strstr((*n)->_nombre_, nom) != NULL) 
			return n;
        n = &(*n)->_next_;
    }
    return NULL;
}

int lista_add_nombre(int id, char* nombre)
{
	lista_pt *n = (lista_pt *)malloc(sizeof(lista_pt));

	if ((n = *list_search_d2(&thread, id)) == NULL)
		return ERROR;
	n->_nombre_ = malloc(sizeof(char[16]));
	strcpy(n->_nombre_, nombre);
	return EXITO;
}

void list_print(lista_pt *n) 
{
    if (n == NULL) {
        printf("lista esta vacía\n");
    }
    while (n != NULL) {
        printf("print %p %p %lu %d\n", n, n->_next_, n->_id_thread_, n->_id_socket_);
        n = n->_next_;
    }
}

int list_len(lista_pt *s)
{
	int cont = 0;

	lista_pt *n = s;
	while (s != NULL)
	{
		n = n->_next_;
		cont+=1;
	}
	return cont;
}