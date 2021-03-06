#ifndef __HSERVER__
#define __HSERVER__

#define MAX_QUE 5
#define TAM 256
#define TAMNOM 32
#define PUERTO 6666

#define EXITO 0
#define ERROR 1

#define ON	1
#define OFF	-1

#define EXIT_CODE 3

#define LISTAR_CODE 4

#define EXITO_REG_CLIENTE 10
#define ERROR_REG_CLIENTE 11
#define EXITO_CL_CHARL	20
#define ERROR_CL_CHARL	21
#define EXITO_CHAT	22
#define ERROR_CHAT	23
#define EXITO_MSG	24
#define ERROR_MSG	25
#define EXITO_SILENCIO	26
#define ERROR_SILENCIO	27
#define EXITO_VER_CONV	30
#define ERROR_VER_CONV	31
#define ERROR_CLIENTE_INC 41


typedef struct ptr 
{
	pthread_t _id_thread_;		// id_thread encargado de la conexion
	int _id_socket_;			// id_socket
	char* _nombre_;				// nombre del cliente (nickname)
	struct ptr *_next_;
} lista_pt;

typedef struct mensaje_cola
{
	long int tipo;			// Cada elemento lleva untipo asociado con el cliente al que pertenece
	char msj[TAM];			// Mensaje que contiene.
} elem_cola;

void list_print(lista_pt *); 
lista_pt **list_search_d1(lista_pt **, pthread_t);
lista_pt **list_search_d2(lista_pt **, int);
lista_pt **list_search_d3(lista_pt **, char*);
void list_remove(lista_pt **);
lista_pt *list_add(lista_pt **, pthread_t, int);
int lista_add_nombre(int, char*);

void *ejec_cliente(void *);
int nuevo_cliente(lista_pt **, int);
int aceptar_conexion(int);
int iniciar_servidor (int);
int iniciar_cola();
int list_len(lista_pt *);
void* ejec_servidor(void *);
int verificar_msj(char *, int);
int preguntar_chat(int, char*);
int responder_chat(char*, char*, char*);
int broadcast(int, char*);
int enviar_msj(char*, char*, char*);
int registrar_chat(char*, char*);
int verificar_chat(char*, char*);

int archivo_agregar(char*, char*);
char* archivo_listar(char*);
int archivo_buscar(char *, char *);
int archivo_borrar(char*, char*);
int registrar_usuario(int, char *);
void deregistrar_usuario(int);

void broadcast_clientes(char *);
char * listar_clientes(int);

int obtener_id_nombre(char*);
char* obtener_nombre_id(int);

//void iniciar_conversacion(cliente_origen, cliente_destino);

int log_evento(char *, char *);

#endif /* __HSERVER__ */
