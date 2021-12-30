/*
 * usuario/prueba_dormir.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 * Programa de usuario que realiza una prueba de la llamada dormir
 */

#include "servicios.h"

int main(){

	printf("prueba_dormir: comienza\n");

	if (crear_proceso("dormilon")<0)
		printf("Error creando dormilon\n");
	
	if (crear_proceso("dormilon")<0)
		printf("Error creando dormilon\n");

	printf("prueba_dormir: termina\n");
	return 0; 
}
