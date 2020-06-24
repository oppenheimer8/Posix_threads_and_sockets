#define _POSIX_C_SOURCE 199506L

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdlib.h>

#include "team.h"

//defines
#define N_VAA 10
#define P_CL 15300

// Datos compartidos,mutex y condiciones
//**
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t recibe_ordenes = PTHREAD_COND_INITIALIZER; // Condicion para recibir ordenes en el hilo de recepcion de ordenes
pthread_cond_t envia_ordenes = PTHREAD_COND_INITIALIZER; // Condicion para enviar la orden a los vaas

struct st_res resultados[N_VAA]; // Resultados recibidos de los VAA
struct st_ord orden,last_orden; // Ultima orden

int orden_ok = 0; // El hilo de recpción de ordenes incrementa su valor cuando recibe una orden nueva y el hilo de transmisión lo decrementa
int fin = 0; // fin del programa
int tierra = 0;
int cack = 0;
struct sockaddr_in vaa_addr[N_VAA],client_tcp,server_tcp,server_udp; // Estructuras sockaddr_in para todos los extremos de los sockets
//**


// Variables para controlar el temporizador que interrumpe la espera de recv y finaliza el programa tras x tiempo
timer_t timeout_temp;
struct timespec zero = {0, 0};
struct timespec temp_tout = {0,0};
struct itimerspec on;
struct itimerspec off;
int ttout;
int tout_flag = 0;

// Manejador para interrumpir la espera de recv
void h(int sig){
    tout_flag = 1; // flag para saber que el fallo en recv ha sido por timeout
}

// Definicion de los hilos

void *th_rec_ord(void *p); // hilo de recepción de órdenes por tcp

void *th_trans(void *p); // hilo de transmisión de órdenes a los vaa por udp

// main -> es el hilo de recepción

int main(int argc, char **argv){


        int puerto_tcp; // Puerto del socket tcp
        int puerto_udp; // Puerto del socket udp
        char vaa_ip[INET_ADDRSTRLEN]="";   // ip que se lee por pantalla en formato decimal con puntos
        memset((char *)&orden,0,sizeof(orden));
        last_orden = orden;
        last_orden.orden = ATERRIZAR; // Supuesto de que los VAA han aterrizado

        int all_ack = 0; // Cuando all_ack == N_VAA --> se genera la condicion para enviar la orden
        int all_tierra = 0; // Cuando orden.orden == ATERRIZAR espero a que all_tierra == N_VAA entonces genero la condición recibe_ordenes
        int sz = sizeof(struct sockaddr_in); // entero con tamaño estructura sockaddr_in para aceptar conexión tcp
        int i,j;
        sigset_t s;
        socklen_t tam;
        struct sigaction a;
        struct sigevent ev;
        int socktcp,sockudp,conn_fd;

        pthread_t id_th_rec_ord,id_th_trans;

        struct st_res local_res;

        // inicializo las estructuras sockaddr_in a 0 --> el resto las igualo a la que he inicializado
        memset((char*)&client_tcp,0,sizeof(client_tcp));
        memset((char*)&server_udp,0,sizeof(server_udp));
        memset((char*)&server_tcp,0,sizeof(server_tcp));

        for(i=0;i<N_VAA;i++)
            vaa_addr[i] = server_udp;
        
        //Leo los argumentos y los paso a las variables

        sscanf(argv[1],"%d",&puerto_tcp);
        sscanf(argv[2],"%d",&puerto_udp);
        sscanf(argv[3],"%s",vaa_ip);
        sscanf(argv[4],"%d",&ttout);


        // paso ttout segundos a la estructura timespec --> ttout es el timeout en segundos
        temp_tout.tv_sec = ttout;
        printf("Timeout en seg %d\n",ttout);

        // defino el evento que utiliza el temporizador --> va a saltar con SIGALRM
        ev.sigev_signo = SIGALRM;
        ev.sigev_notify = SIGEV_SIGNAL;

        // Creo el temporizador como reloj de tiempo real
        timer_create(CLOCK_REALTIME,&ev,&timeout_temp);

        // defino on y off para encender y apagar el temporizador
        on.it_interval = zero;
        on.it_value = temp_tout;

        off.it_interval = zero;
        off.it_value = zero;

        // defino la estructura acción para que ejecute h al llegar SIGALRM
        a.sa_flags = 0;
        a.sa_handler = h;
        sigemptyset(&a.sa_mask);
        sigaction(SIGALRM,&a,NULL);

        // bloqueo SIGALRM en la máscara y la desbloquearé en el hilo que recibe las ordenes con recv para que solo ese se vea interrumpido
        sigemptyset(&s);
        sigaddset(&s,SIGALRM);
        pthread_sigmask(SIG_BLOCK,&s,NULL);        

        // defino las estructuras sockaddr_in

        client_tcp.sin_family = AF_INET;
        client_tcp.sin_addr.s_addr = INADDR_ANY;
        client_tcp.sin_port = htons(puerto_tcp);

        server_udp.sin_family = AF_INET;
        server_udp.sin_addr.s_addr = INADDR_ANY;
        server_udp.sin_port = htons(P_CL);

        // defino cada uno de las estructuras sockaddr_in de los vaa
        for(i=0;i<N_VAA;i++){
            // el VAA i tendrá ip:vaa_ip  y puerto: puerto_udp + i
            vaa_addr[i] = server_udp;
            vaa_addr[i].sin_addr.s_addr = inet_addr(vaa_ip); // convierto el inet_addr a formato numérico con ntohl -> le sumo i -> vuelvo a pasarlo a formato de red con htonl
            vaa_addr[i].sin_port = htons(puerto_udp+i);
            printf("Ip y puerto del VAA %d: %d %s\n",i,htons(vaa_addr[i].sin_port),inet_ntoa(vaa_addr[i].sin_addr));
        }

        // creo los dos sockets

        socktcp = socket(AF_INET,SOCK_STREAM,0);
        sockudp = socket(AF_INET,SOCK_DGRAM,0);

        // Hago bind
        if(bind(sockudp,(struct sockaddr *)&server_udp,sizeof(server_udp)) == 0)
            printf("Socket udp binded in port %d\n",htons(server_udp.sin_port));

        if(bind(socktcp,(struct sockaddr *)&client_tcp,sizeof(client_tcp)) == 0)
        printf("Socket tcp binded in port %d\n",htons(client_tcp.sin_port));        

        if(listen(socktcp,1)>-1)
            printf("Ejecutado el listen con éxito\n"); // preparo el socket para aceptar N=1 conexiones
       
        if((conn_fd = accept(socktcp,(struct sockaddr*)&server_tcp,(socklen_t *)&sz) ) >-1){
            sleep(1);
            printf("Conexión aceptada al cliente: IP = %s ,Puerto = %d  --> socket = %d\n",inet_ntoa(server_tcp.sin_addr),htons(server_tcp.sin_port),conn_fd);
        } // conn_fd es el socket ya conectado
        
        printf("Se crean los hilos\n");
        // creo el hilo de recepción de órdenes y le paso el socket ya escuchando al puerto tcp
        pthread_create(&id_th_rec_ord,NULL,th_rec_ord,&conn_fd);

        // creo el hilo de transmisión de órdenes a los vaa y le paso el socket ya vinculado al puerto udp
        pthread_create(&id_th_trans,NULL,th_trans,&sockudp);

        // Comienzo la rutina del hilo de recepción en Main

        while(!fin || (fin && (cack || tierra))){ // cuando fin == 1 sigo iterando hasta que todos los vaa estén en tierra y todos hayan contestado

            if(recv(sockudp,&local_res,sizeof(local_res),MSG_WAITALL)<0){
            printf("Fallo en recepción por udp\n");
            }
            
            resultados[local_res.n_vaa] = local_res; //paso los resultados de la variable local a los datos globales
            
            switch (local_res.result)
            {
            case ACK:
                    printf("Llega ACK desde el VAA nº %d\n",local_res.n_vaa);
                    pthread_mutex_lock(&mut);
                    if(++all_ack >= N_VAA){ // aumento all_ack y si = N_VAA mando genero condición para enviar órdenes
                        pthread_cond_signal(&envia_ordenes);
                        printf("Todos los VAA(%d) han respondido con ACK, se puede enviar otra orden\n",all_ack);
                        all_ack = 0;
                        cack--;
                    }
                pthread_mutex_unlock(&mut);
                break;

            case EN_TIERRA:

                    printf("Llega EN_TIERRA desde VAA\n");
                    pthread_mutex_lock(&mut);
                    if(all_tierra < N_VAA && last_orden.orden != EN_TIERRA)
                    if(++all_tierra >= N_VAA){ // aumento all_tierra y si = N_VAA mando genero condición para recibir ordenes
                        
                        printf("Todos los VAA(%d) en tierra\n",all_tierra);
                        all_tierra = 0;
                        
                        tierra--;

                        pthread_cond_signal(&recibe_ordenes);
                    }
                    pthread_mutex_unlock(&mut);
                
                break;

            case DATOS:
                printf("Llega DATOS desde VAA(%d) = %d\n",local_res.n_vaa,local_res.datos.res);
                send(conn_fd,&local_res.datos.res,sizeof(local_res.datos.res),MSG_WAITALL); //mando por tcp la estructura que he recibido por udp
                break;
            }
        }
    sleep(1);
    printf("Main sale de la rutina de recepción\n");

    // espero a que terminen los hilos con join
    pthread_join(id_th_rec_ord,NULL);

    pthread_join(id_th_trans,NULL);

    // llegado a este punto han acabado los hilos y cierro la recepción y transmisión por los sockets
    shutdown(socktcp,SHUT_RDWR);
    shutdown(sockudp,SHUT_RDWR);

    // destruyo los sockets

    close(socktcp);
    close(sockudp);

    return 0;
}

void *th_rec_ord(void *p){
    
    int connfd = *(int *)p;
    int comando;
    sigset_t s2;
    sigemptyset(&s2);
    sigaddset(&s2,SIGALRM);
    pthread_sigmask(SIG_UNBLOCK,&s2,NULL);  

    while(!fin){
        
        // primero recibo del socket tcp y espero el sobretiempo
        // si hay sobretiempo o cierre del socket mando un aterrizar + fin
        // si no hay sobretiempo
            // bloqueo el mutex y actualizo las variables compartidas
            // si la orden es aterrizar y la anterior no lo era --> espero a todos tierra con la variable "tierra"
            // en cualquier caso orden_ok++ para pasarle la orden a la transmision
            // cond_signal al envío
            // while tierra>0 || orden_ok>0 cond_wait
            // desbloqueo mutex

        timer_settime(timeout_temp,0,&on,NULL);
        
        if((recv(connfd,&comando,sizeof(comando),MSG_WAITALL))<0){
            if(tout_flag)
                printf("Timeout en recepción --> acabar\n");
            else
                printf("Error en recv\n");
            comando = ATERRIZAR;
            fin = 1;
        }else{
            printf("Ha llegado comando al hilo de recepción de ordenes\n");
        }
        timer_settime(timeout_temp,0,&off,NULL);
        if(comando == ACABAR){
            comando = ATERRIZAR;
            fin = 1;
        }
        pthread_mutex_lock(&mut);

            orden.orden = comando;
            if(orden.orden == (ATERRIZAR || ACABAR) && last_orden.orden != ATERRIZAR)
                tierra++;
            
            orden_ok++;

            pthread_cond_signal(&envia_ordenes);
            if(!fin)
            while(tierra>0 || orden_ok >0)
                pthread_cond_wait(&recibe_ordenes,&mut);

        last_orden = orden;

        pthread_mutex_unlock(&mut);

    }

    printf("Fin del hilo de recepción de órdenes\n");
    return NULL;
}

void *th_trans(void *p){
    
    int sockudp = * (int *)p;
    int i;
    while (!fin)
    {
        sleep(1);
        pthread_mutex_lock(&mut);
        while(cack > 0 || orden_ok < 1){ // mientras estén llegando los ack se espera la condición de que all_ack = N_VAA
            printf("Hilo de transmisión de órdenes libera mutex esperando condición || cack = %d ; flag orden = %d\n",cack,orden_ok);            
            pthread_cond_wait(&envia_ordenes,&mut);
        }

        printf("La orden a enviar es: %d\n",orden.orden);

        for(i=0;i<N_VAA;i++)
            sendto(sockudp,&orden,sizeof(orden),MSG_WAITALL,(struct sockaddr *)&vaa_addr[i],sizeof(vaa_addr[i]));

        orden_ok--;

        cack++;

        printf("Orden enviada a los vaa || flag de orden = %d\n",orden_ok);
        pthread_cond_signal(&recibe_ordenes);
        pthread_mutex_unlock(&mut);
    }
    printf("Fin del hilo de transmisión de órdenes\n");    
    return NULL;
}
