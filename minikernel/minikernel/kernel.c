/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */
#include <string.h>	/* añadida libreria string */
#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("exec_arit: excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("exec_mem: excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	printk("-> TRATANDO INT. DE RELOJ\n");

	int n_interrupcion = fijar_nivel_int(NIVEL_3);
	/* procesos dormidos */
	cuentaAtrasBloqueados();

	/* round robin */
	actualizarTick();
	
	fijar_nivel_int(n_interrupcion);

    return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");

	/* round-robin */
	tratarIntSW();
	
	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc, i;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* cosas añadidas */
		/* llamada al sistema dormir */
		p_proc->tiempo_dormir = 0;	
		/* mutex */
		p_proc->num_descriptores_abiertos = 0;
		for (i = 0; i < NUM_MUT_PROC; i++)
		{
			p_proc->descriptores[i]=-1;
		}
		/* round-robin */
		p_proc->contadorTicks = TICKS_POR_RODAJA;

		
		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){
	
	/* comprobando si al proceso tiene mutex abiertos */
	int i = 0;
	while (p_proc_actual->num_descriptores_abiertos>0) {
		if (p_proc_actual->descriptores[i]!=-1) {
			//printk("Cierre implicito de mutexid: %d\n",i);
			cerrar_mutex(p_proc_actual->descriptores[i]);
		}
		i++;
	}
	
	printk("-> FIN PROCESO id: %d\n", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no deberia llegar aqui */
}

/* ---------------funcionalidades añadidas--------------- */

/* obtener id del proceso */
int obtener_id_pr(){
	return p_proc_actual->id;
}

/* Llamada que bloquea al proceso un plazo de tiempo */
int dormir(unsigned int segundos){

	/* lectura de registro 1 */
	unsigned int segs = (unsigned int)leer_registro(1);

	/* guardar el nivel de interrupcion */
	int n_interrupcion = fijar_nivel_int(NIVEL_1);
	BCPptr proceso_dormido = p_proc_actual;

	/* actualizacion de estructura de datos */
	proceso_dormido -> tiempo_dormir = segs*TICK;
	proceso_dormido -> estado = BLOQUEADO;

	/* cambio de lista de procesos */
	eliminar_primero(&lista_listos);
	insertar_ultimo(&lista_bloqueados_dormir,proceso_dormido);

	/* usando el planificador se obtiene el proceso a ejecutar */
	p_proc_actual=planificador();

	/* se restaura el nivel de interrupcion guardado */
	cambio_contexto(&(proceso_dormido->contexto_regs),&(p_proc_actual->contexto_regs));
	fijar_nivel_int(n_interrupcion);

	return 0;
}

/* funcion auxiliar para la llamada dormir, actualiza los tiempos de los procesos dormidos */
void cuentaAtrasBloqueados(){
	
	BCPptr auxiliar = lista_bloqueados_dormir.primero;			/* obtengo el primer proceso de la lista de bloqueados */
	/* recorro la lista y actualizo los tiempos */
	while(auxiliar != NULL){							/* mientas hay procesos en lista_bloqueados */
		BCPptr siguiente = auxiliar->siguiente;			/* se obtiene el puntero al siguente elmento de la lista */
		auxiliar->tiempo_dormir--;						/* se disminuye el contador de tiempo que le queda al proceso por dormir */
		if(auxiliar->tiempo_dormir ==0){				/* si el tiempo de dormir se ha agotado */
			auxiliar->estado = LISTO;					/* el proceso cambia de estado a LISTO */
			eliminar_elem(&lista_bloqueados_dormir, auxiliar);	/* se elimina de la lista de bloqueados */
			insertar_ultimo(&lista_listos, auxiliar);	/* y pasa a la lista de procesos listos */
		}
		auxiliar = siguiente;							/* a por el siguente elemento */
	}
}

/* mutex */

/* llamada al sistema para crear mutex */
int crear_mutex(char *nombre, int tipo){

	nombre = (char*) leer_registro(1);
	tipo = (int) leer_registro(2);

	int n_interrupcion, desc_lista_mutex, desc_libre_proc, mutex_creado;

	n_interrupcion = fijar_nivel_int(NIVEL_1);

	/* comprobacion de longitud de nombre */
	if (strlen(nombre)>MAX_NOM_MUT)
	{
		printk("Error, nombre de mutex sobrepasa la logintud establecida\n");
		fijar_nivel_int(n_interrupcion);
		return -1;
	}

	/* comprobacion de duplicidad de nombres */
	if (buscarMutexPorNombre(nombre)!=-1)
	{
		printk("Error, mutex %s ya existe en el sistema\n",nombre);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}

	/* comprobacion de descriptores libres en el proceso actual */
	desc_libre_proc = buscarDescriptorLibrePorceso();
	if (desc_libre_proc==-1)
	{
		printk("Error, el proceso id: %d no tiene descriptores libres\n",p_proc_actual->id);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}

	/* comprobacion de hueco libre en lista mutex */
	mutex_creado = 0;
	while (mutex_creado==0)
	{
		desc_lista_mutex = buscarPosicionMutexLibre();
		if (desc_lista_mutex==-1)
		{
			mutex_creado=0;
			printk("Error, alcanzado maximo de mutex creados en el sistema\n");
			printk("Bloqueando proceso id: %d\n",p_proc_actual->id);
			contador_lista_bloqueados_mutex++;
			BCPptr p_proc_bloqueado = p_proc_actual;
			p_proc_bloqueado->estado = BLOQUEADO;
			eliminar_primero(&lista_listos);
			insertar_ultimo(&lista_bloqueados_mutex,p_proc_bloqueado);
			p_proc_actual = planificador();
			printk("C.CONTEXTO POR BLOQUEO de %d a %d\n",p_proc_bloqueado->id,p_proc_actual->id);
			cambio_contexto(&(p_proc_bloqueado->contexto_regs),&(p_proc_actual->contexto_regs));
		}

		/* si ha pasado todas las pruebas y no se ha bloqueado */
		/* crea el mutex */
		MUTEXptr m = &lista_mutex[desc_lista_mutex];
		strcpy(m->nombre,nombre);
		m->tipo=tipo;
		m->estado = OCUPADO;
		contador_lista_mutex++;
		/* abre el mutex */
		p_proc_actual->descriptores[desc_libre_proc]=desc_lista_mutex;
		p_proc_actual->num_descriptores_abiertos++;
		printk("Mutex %s CREADO y ABIERTO\n",m->nombre);
		mutex_creado=1;
	}
	
	fijar_nivel_int(n_interrupcion);
	return desc_libre_proc;
}

/* llamada al sistema para abrir mutex */
int abrir_mutex(char *nombre){

	nombre = (char*) leer_registro(1);
	int n_interrupcion,pos_lista_mutex, desc_libre_proc;

	n_interrupcion = fijar_nivel_int(NIVEL_1);

	/* busqueda de mutex por su nombre */
	pos_lista_mutex = buscarMutexPorNombre(nombre);
	if (pos_lista_mutex==-1)
	{
		printk("Error, mutex %s no encontrado\n",nombre);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}

	/* comprobacion de mutex disponibles en proceso actual */
	desc_libre_proc = buscarDescriptorLibrePorceso();
	if (desc_libre_proc==-1)
	{
		printk("Error, el proceso id: %d no tiene descriptores libres\n",p_proc_actual->id);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}

	p_proc_actual->descriptores[desc_libre_proc] = pos_lista_mutex;
	p_proc_actual->num_descriptores_abiertos++;
	printk("Mutex %s ABIERTO\n",nombre);
	fijar_nivel_int(n_interrupcion);

	return desc_libre_proc;

}

/* llamada al sistema para boquear mutex */
int lock(unsigned int mutexid){

	int n_interrupcion, desc_proc, pos_lista_mutex, lock;
	int *retorno;
	unsigned int m_id = (unsigned int) leer_registro(1);
	if (m_id<NUM_MUT)
	{
		mutexid=m_id;
	}

	n_interrupcion = fijar_nivel_int(NIVEL_1);

	retorno = buscarMutexPorID((int)mutexid);
	desc_proc = *retorno;
	pos_lista_mutex = *(retorno+1);
	if (desc_proc==-1)
	{
		printk("Error, mutex con mutexid: %d no encontrado\n",mutexid);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}
	
	MUTEXptr m = &lista_mutex[pos_lista_mutex];

	//printk("Mutex %s encontrado | num_bloqueos: %d, id propietario: %d\n",m->nombre,m->contador_bloqueos,m->id_proceso_propietario);
	lock=0;
	while (lock==0)
	{
		if (m->id_proceso_propietario==-1 && m->contador_bloqueos==0)
		{
			m->contador_bloqueos++;
			m->id_proceso_propietario=p_proc_actual->id;
			m->mutex_lock=LOCKED;

			printk("Mutex %s BLOQUEADO\n",m->nombre);
			
			fijar_nivel_int(n_interrupcion);
			return desc_proc;
		}
		if (m->id_proceso_propietario==p_proc_actual->id)
		{
			if (m->mutex_lock==LOCKED && m->tipo==RECURSIVO)
			{
				lock=1;
				m->contador_bloqueos++;
				printk("Mutex RECURSIVO %s BLOQUEADO\n",m->nombre);
				return desc_proc;
			} else if (m->mutex_lock==LOCKED)
			{
				printk("Error, intento de bloquear mutex %s ya bloqueado y de tipo NO RECURSIVO\n",m->nombre);
				fijar_nivel_int(n_interrupcion);
				return -1;
			}	
		}
		
		/* si llega hasta aqui esque proceso actual no es propietario del mutex y sera bloqueado*/
		//printk("Error, proceso id: %d no es propietario de mutex %s, id propietario real: %d\n",p_proc_actual->id,m->nombre,m->id_proceso_propietario);
		//printk("Bloqueando proceso id: %d\n",p_proc_actual->id);
		lock=0;
		m->num_procesos_esperando++;
		BCPptr p_proc_bloqueado = p_proc_actual;
		p_proc_bloqueado->estado = BLOQUEADO;
		eliminar_primero(&lista_listos);
		insertar_ultimo(&(m->lista_procesos_esperando),p_proc_bloqueado);
		p_proc_actual = planificador();
		printk("C.CONTEXTO POR BLOQUEO de %d a %d\n",p_proc_bloqueado->id,p_proc_actual->id);
		cambio_contexto(&(p_proc_bloqueado->contexto_regs),&(p_proc_actual->contexto_regs));
	}
	
	fijar_nivel_int(n_interrupcion);
	return -1;
}

/* llamada al sistema para desbloquear mutex */
int unlock(unsigned int mutexid){
	int n_interrupcion, desc_proc, pos_lista_mutex;
	int *retorno;
	unsigned int m_id = (unsigned int) leer_registro(1);

	if (m_id<NUM_MUT)
	{
		mutexid=m_id;
	}

	n_interrupcion = fijar_nivel_int(NIVEL_1);

	retorno = buscarMutexPorID((int)mutexid);
	desc_proc = *retorno;
	pos_lista_mutex = *(retorno+1);
	if (desc_proc==-1)
	{
		printk("Error, mutex con mutexid: %d no encontrado\n",mutexid);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}
	
	MUTEXptr m = &lista_mutex[pos_lista_mutex];

	//printk("Mutex %s encontrado | num_bloqueos: %d, id propietario: %d\n",m->nombre,m->contador_bloqueos,m->id_proceso_propietario);

	if (m->mutex_lock==LOCKED)
	{
		//printk("Mutex %s esta bloqueado, desbloqueando\n",m->nombre);
		m->contador_bloqueos--;
		//printk("Contador de bloqueos decrementado: %d, mutex %s\n",m->contador_bloqueos,m->nombre);
		if (m->contador_bloqueos==0)
		{
			m->mutex_lock=UNLOCKED;
			m->id_proceso_propietario=-1;
			printk("Mutex %s DESBLOQUEADO\n",m->nombre);

			if (m->num_procesos_esperando>0)
			{
				m->num_procesos_esperando--;
				BCPptr p_proc_bloqueado = m->lista_procesos_esperando.primero;
				p_proc_bloqueado->estado = LISTO;
				eliminar_primero(&(m->lista_procesos_esperando));
				insertar_ultimo(&lista_listos,p_proc_bloqueado);
				printk("Proceso id: %d DESBLOQUEADO\n",p_proc_bloqueado->id);
			}

			fijar_nivel_int(n_interrupcion);
			return desc_proc;
		}
		
	} else { 	/* si el mutex no esta bloqueado el intento de desbloquearlo producira un error */
		printk("Error, intento de desbloquear mutex %s no bloqueado\n",m->nombre);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}
	

	fijar_nivel_int(n_interrupcion);
	return 0;
}

/* llamada al sistema para cerrar mutex */
int cerrar_mutex(unsigned int mutexid){

	int n_interrupcion, desc_proc, pos_lista_mutex;
	int *retorno;
	unsigned int m_id = (unsigned int) leer_registro(1);

	if (m_id<NUM_MUT)
	{
		mutexid=m_id;
	}

	n_interrupcion = fijar_nivel_int(NIVEL_1);
	//printk("MUTEXID: %d\n",mutexid);
	retorno = buscarMutexPorID((int) mutexid);
	desc_proc = *retorno;
	pos_lista_mutex = *(retorno+1);
	if (desc_proc==-1)
	{
		printk("Error, mutex con mutexid: %d no encontrado\n",mutexid);
		fijar_nivel_int(n_interrupcion);
		return -1;
	}
	//pos_lista_mutex = p_proc_actual->descriptores[desc_proc];
	MUTEXptr m = &lista_mutex[pos_lista_mutex];
	//printk("Mutex %s encontrado\n",m->nombre);

	while (m->mutex_lock==LOCKED)
	{
		unlock(mutexid);
	}
	
	m->estado = LIBRE;
	m->contador_bloqueos=0;
	m->num_procesos_esperando=0;
	p_proc_actual->descriptores[desc_proc]=-1;
	p_proc_actual->num_descriptores_abiertos--;
	contador_lista_mutex--;
	printk("Mutex %s CERRADO\n",m->nombre);

	if (contador_lista_bloqueados_mutex>0)
	{
		contador_lista_bloqueados_mutex--;
		BCPptr p_proc_bloqueado = lista_bloqueados_mutex.primero;
		p_proc_bloqueado->estado = LISTO;
		eliminar_primero(&lista_bloqueados_mutex);
		insertar_ultimo(&lista_listos,p_proc_bloqueado);
		printk("Proceso id %d DESBLOQUEADO\n",p_proc_bloqueado->id);
	}
	

	fijar_nivel_int(n_interrupcion);

	return 0;
}
/* rutinas auxiliares */

int buscarPosicionMutexLibre(){
	int i;
	for (i = 0; i < NUM_MUT; i++)
	{
		if (lista_mutex[i].estado==LIBRE)
		{
			return i;	/* devuelve la posicion del mutex libre */
		}
		
	}
	return -1;
}

int buscarMutexPorNombre(char *nombre){
	int i;
	for ( i = 0; i < NUM_MUT; i++)
	{
		if (strcmp(lista_mutex[i].nombre,nombre)==0)
		{
			return i;	/* devuelve la posicion del mutex encontrado */
		}
		
	}
	return -1;			/* devuelve -1 si no encuentra mutex */
}

int buscarDescriptorLibrePorceso(){
	int i;
	for ( i = 0; i < NUM_MUT_PROC; i++)
	{
		if (p_proc_actual->descriptores[i]==-1)
		{
			return i;	/* devuelve la posicion del descriptor libre */
		}
		
	}
	return -1;
}

int *buscarMutexPorID(int mutexid){
	int i;
	static int retorno[2] = {-1,-1};
	for ( i = 0; i < NUM_MUT_PROC; i++)
	{
		if (p_proc_actual->descriptores[i]==mutexid)
		{
			retorno[0]=i;
			retorno[1]=p_proc_actual->descriptores[i];
			return retorno;
		}
		
	}
	return retorno;
}

void iniciar_lista_mutex(){
	int i;
	for ( i = 0; i < NUM_MUT; i++)
	{
		lista_mutex[i].contador_bloqueos=0;
		lista_mutex[i].estado=LIBRE;
		lista_mutex[i].id_proceso_propietario=-1;
		lista_mutex[i].lista_procesos_esperando.primero=NULL;
		lista_mutex[i].mutex_lock=UNLOCKED;
		lista_mutex[i].num_procesos_esperando=0;
	}
}

/* round-robin */
/* rutina para actualizar el contador de ticks */
void actualizarTick(){
	if (p_proc_actual->estado==LISTO) /* si hay proceso */
	{
		/* si se acaban los ticks se activa interrupcion software */
		if (p_proc_actual->contadorTicks>0)
		{
			/* decrementar contador de ticks */
			p_proc_actual->contadorTicks--;
			printk("Porceso id: %d, contador de ticks restantes: %d\n",p_proc_actual->id,p_proc_actual->contadorTicks);
		}
		if (p_proc_actual->contadorTicks==0)
		{
			printk("Proceso id: %d, activando interrupcion SW\n",p_proc_actual->id);
			activar_int_SW();
		}
	}
}
/* rutina para tratar interrupcion SW en round-robin */
void tratarIntSW(){
	if (lista_listos.primero==lista_listos.ultimo)
	{
		p_proc_actual->contadorTicks=TICKS_POR_RODAJA;
		printk("Proceso id: %d, contador de ticks actualizado\n",p_proc_actual->id);
	} else {
		BCPptr p_proc_expulsado = p_proc_actual;
		int n_interrupcion = fijar_nivel_int(NIVEL_1);
		eliminar_elem(&lista_listos,p_proc_expulsado);
		insertar_ultimo(&lista_listos,p_proc_expulsado);
		fijar_nivel_int(n_interrupcion);
		p_proc_actual = planificador();
		p_proc_actual->contadorTicks=TICKS_POR_RODAJA;
		printk("C.CONTEXTO POR EXPULSION de %d a %d\n",p_proc_expulsado->id,p_proc_actual->id);
		cambio_contexto(&(p_proc_expulsado->contexto_regs),&(p_proc_actual->contexto_regs));
	}
}


/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();			/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* --------cosas añadidas-------- */
	iniciar_lista_mutex();		/* inicia lista_mutex del sistema */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
