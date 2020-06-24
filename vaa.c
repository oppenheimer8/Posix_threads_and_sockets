#define _POSIX_C_SOURCE 199506L

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "team.h"

#define N_VAA 10

#define PUERTO_LOCAL 6666


struct to_vaa { // nueva estructura con la que se facilita la creación de los hilos vaa, en la que le paso la estructura de resultados y el sockaddr_in de cada vaa
	struct st_res res; // por simplificar la identificacion de cada vaa le mando la estructura con el índice de este: res.n_vaa
	struct sockaddr_in a;
};

struct to_datos { // estructura a pasarle al hilo de datos de modo que se pueda actualizar la estructura to_vaa en concreto los datos y se utilice el socket vinculado
	struct to_vaa v;
	int sock_udp;
};


int puerto_remoto = 0;
char ip_remoto[INET_ADDRSTRLEN]="";
struct sockaddr_in addr_remoto;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
int z = 0;

pthread_t id_h_local,id_h_vaa[N_VAA];

void handler (int sig){}

void *h_vaa(void *p);

void *h_datos(void *p);

int main(int argc, char **argv){

memset((char*)&addr_remoto,0,sizeof(addr_remoto));

struct to_vaa v[N_VAA];
int i;

sscanf(argv[1],"%d",&puerto_remoto);
sscanf(argv[2],"%s",ip_remoto);

sigset_t s,y;
sigemptyset(&s);
sigaddset(&s,SIGRTMIN);
sigaddset(&s,SIGRTMIN+1);
sigprocmask(SIG_BLOCK,&s,NULL);

id_h_local = pthread_self();

addr_remoto.sin_family = AF_INET;
addr_remoto.sin_port = htons(puerto_remoto);
addr_remoto.sin_addr.s_addr = inet_addr(ip_remoto);

for(i=0;i<N_VAA;i++){

memset((char *)&v[i],0,sizeof(v));
v[i].a.sin_family = AF_INET;
v[i].a.sin_port = htons(PUERTO_LOCAL+i);
v[i].a.sin_addr.s_addr = INADDR_ANY;
v[i].res.n_vaa = i;

pthread_create(&id_h_vaa[i],NULL,h_vaa,&v[i]);
}

sigemptyset(&y);
sigaddset(&y,SIGRTMIN);

sigwaitinfo(&y,NULL);
printf("Ha llegado SIGRTMIN del último VAA en terminar, por lo que todos los VAA han acabado su rutina, acaba main\n");


for(i=0;i<N_VAA;i++)
	pthread_join(id_h_vaa[i],NULL);
}


void *h_vaa(void *p){

sigset_t s;
pthread_t id_h_datos;

int r;
int fin = 0, inhdatos = 0;
int udp_sock;

struct to_vaa v = *(struct to_vaa *)p;
struct sockaddr_in vaa_addr = v.a;

struct st_ord orden,last_orden;
memset((char*)&orden,0,sizeof(orden));
memset((char*)&last_orden,0,sizeof(last_orden));

last_orden.orden = ATERRIZAR;

struct to_datos x;
memset((char*)&x,0,sizeof(x));

udp_sock = socket(AF_INET,SOCK_DGRAM,0);

if(bind(udp_sock,(struct sockaddr *)&vaa_addr,sizeof(vaa_addr))>-1)
	printf("VAA %d binded in port %d success\n",v.res.n_vaa,htons(vaa_addr.sin_port));

x.sock_udp = udp_sock; // igualo al socket de la estructura que le voy a pasar al hilo de recogida de datos
x.v = v; 
sleep(1);
while(!fin){
	v.res.datos.res = x.v.res.datos.res; // acutalizo los datos recogidos por el hilo de recogida y envio de datos h_datos
		
	if((r=recv(udp_sock,&orden,sizeof(orden),0)<0)){
		printf("TIMEOUT en la recepción de órden --> ACABAR\n");
		
		orden.orden = ATERRIZAR;
        fin = 1;
	}
	else{
		switch(orden.orden){
			
		case (DETECTAR):
			printf("Llega orden DETECTAR al VAA %d\n",v.res.n_vaa);
			v.res.result = ACK;
			sendto(udp_sock,&v.res,sizeof(v.res),0,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
			//sleep(1);
			if(!inhdatos){ // si no había un hilo de recogida y envío de datos lo creo
				pthread_create(&id_h_datos,NULL,h_datos,&x);
				inhdatos++;
			}
			else{ // si ya había un hilo de recogida de datos, lo acabo y comienzo otro
				pthread_kill(id_h_datos,SIGRTMIN+1);
				pthread_join(id_h_datos,NULL);
				pthread_create(&id_h_datos,NULL,h_datos,&x);
			}
			break;

		case (ATERRIZAR):
			if(inhdatos){ // si había un hilo de recogida de datos lo acabo y aterrizo
				pthread_kill(id_h_datos,SIGRTMIN+1);
				pthread_join(id_h_datos,NULL);
				inhdatos--;
			}
			if(last_orden.orden != ATERRIZAR){
			printf("Llega orden ATERRIZAR al VAA %d\n",v.res.n_vaa);
			v.res.result = ACK;
			sendto(udp_sock,&v.res,sizeof(v.res),0,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
			sleep(5); // tiempo que se tarda en aterrizar
			v.res.result = EN_TIERRA;
			sendto(udp_sock,&v.res,sizeof(v.res),0,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
			}
			else{
			printf("Llega comando ATERRIZAR y ya estaba aterrizado\n");
			v.res.result = ACK;
			sendto(udp_sock,&v.res,sizeof(v.res),0,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
			}
			break;
		//case(ACABAR):
		//	printf("Llega orden ACABAR al VAA %d\n",res.n_vaa);
		//	res.result = ACK;
		//	sendto(udp_sock,&res,sizeof(res),MSG_WAITALL,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
		//	sleep(1);
		//	if(last_orden.orden != ATERRIZAR){
		//	res.result = EN_TIERRA;
		//	sendto(udp_sock,&res,sizeof(res),MSG_WAITALL,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
		//	}
		//	fin = 1;
		}
	}
	last_orden = orden;

}

pthread_mutex_lock(&mut);

printf("VAA nº %d saliendo de la rutina\n",++z);

if(z==N_VAA){
	printf("Además es el último VAA\n");
	pthread_kill(id_h_local,SIGRTMIN);
}

pthread_mutex_unlock(&mut);

shutdown(udp_sock,SHUT_RDWR);
close(udp_sock);

printf("VAA %d acaba su rutina\n",v.res.n_vaa);

return NULL;
}

void *h_datos(void *p){

	struct to_datos x = *(struct to_datos *)p;

	struct timespec ciclo = {2,0}; // ciclo de envio de datos por socket udp

	sigset_t s;
	sigemptyset(&s);
	sigaddset(&s,SIGRTMIN+1);
	sleep(3); // tiempo que se tarda en empezar a enviar datos
	printf("Ciclo de recogida de datos comenzado, enviando datos cada 2 segundos\n");
	while(sigtimedwait(&s,NULL,&ciclo)<0){
		x.v.res.result = DATOS;
		x.v.res.datos.res += htons(x.v.a.sin_port)/x.v.res.n_vaa; 
		sendto(x.sock_udp,&x.v.res,sizeof(x.v.res),0,(struct sockaddr *)&addr_remoto,sizeof(addr_remoto));
	}
	printf("Ciclo de recogida de datos terminado\n");

	return NULL;
}
