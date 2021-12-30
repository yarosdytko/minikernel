/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

/* -----------cosas añadidas para mutex----------- */
#define NO_RECURSIVO 0	/* tipo de mutex no recursivo */
#define RECURSIVO 1		/* tipo de mutex recursivo */
#define LOCKED 0		/* mutex bloqueado */
#define UNLOCKED 1		/* mutex desbloqueado */

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
    int id;						/* ident. del proceso */
    int estado;					/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
    contexto_t contexto_regs;	/* copia de regs. de UCP */
    void * pila;				/* dir. inicial de la pila */
	BCPptr siguiente;			/* puntero a otro BCP */
	void *info_mem;				/* descriptor del mapa de memoria */
	/* -----------cosas añadidas----------- */
	/* añadidos para la llamada dormir */
	int tiempo_dormir;			/* tiempo que se queda el proceso dormido */
	/* añadidos para mutex */
	int descriptores[NUM_MUT_PROC];	/* conjunto de descriptores vinculados con los mutex usados por el proceso */
	int num_descriptores_abiertos;		/* guarda el numero de descriptores abiertos por el proceso */
	/* añadidos para round-robin */
	int contadorTicks;
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/** ----------------------------Estructuras de datos añadidas---------------------------- **/

/* ---------llamada al sistema dormir--------- */
/* Variable global que representa la cola de procesos bloqueados por la llamada al sistema dormir()*/
lista_BCPs lista_bloqueados_dormir = {NULL,NULL};

/* ---------mutex--------- */
/* definicion de tipo que corresponde con un mutex */

typedef struct MUTEX_t *MUTEXptr;

typedef struct MUTEX_t{
	char nombre[MAX_NOM_MUT];							/* nombre del mutex */
	int estado;											/* estado de mutex LIBRE|OCUPADO */
	int tipo;											/* tipo de mutex NO RECURSIVO|RECURSIVO */
	int num_procesos_esperando;							/* contador de procesos esperando al mutex */
	lista_BCPs lista_procesos_esperando;				/* lista de procesos esperando al mutex */
	int	id_proceso_propietario;							/* puntero al proceso actual poseedor del mutex */
	int contador_bloqueos;								/* contador de bloqueos del mutex */
	int mutex_lock;										/* 1 - LOCK | 0 - UNLOCK */
} mutex;

mutex lista_mutex[NUM_MUT];			/* lista de nombres de mutex en el sistema */
int contador_lista_mutex;			/* contador de mutex en el sistema */

/* Variable global que representa la cola de procesos esperando al mutex*/
lista_BCPs lista_bloqueados_mutex = {NULL,NULL};
int contador_lista_bloqueados_mutex;

/** ------------------------------------------------------------------------------------ **/


/*
 *
 * Definicion del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*---------prototipos de funciones auxiliares---------*/
/* funciones para dormir */
void cuentaAtrasBloqueados();

/* funciones para mutex */
int buscarPosicionMutexLibre();
int buscarMutexPorNombre(char *nombre);
int buscarDescriptorLibrePorceso();
int* buscarMutexPorID(int mutexid);
void iniciar_lista_mutex();

/* funciones para round-robin */
void actualizarTick();
void tratarIntSW();

/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
/* prototipos de rutinas añadidos */
int obtener_id_pr();
int dormir(unsigned int segundos);
int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);
/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	
					{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{obtener_id_pr},
					{dormir},
					{crear_mutex},
					{abrir_mutex},
					{lock},
					{unlock},
					{cerrar_mutex}
					};

#endif /* _KERNEL_H */
