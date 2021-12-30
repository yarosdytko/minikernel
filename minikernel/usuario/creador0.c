#include "servicios.h"
#include <stdio.h>
#include <stdlib.h>

int main(){

	printf("creador00 comienza\n");
	
	for (int i = 18; i < 22; i++)
	{
		char *n = malloc(20);
		int d;
		sprintf(n,"m_%d",i);
		d=crear_mutex(n,NO_RECURSIVO);
		if (d<0)
		{
			printf("\nError en crear m_%d\n",i);
		} else {
			printf("\nm_%d OK\n",i);
		}
	}
	/*
	c = abrir_mutex("m_1");

	if (c!=-1)
	{
		printf("Mutex abierto\n");
	} else {
		printf("Error en apertura de mutex\n");
	}
	*/
	
	

	printf("creador00 duerme 1 segundo\n");
	dormir(1);

	printf("creador00 termina\n");

	return 0;
}