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

#define PUERTO_LOCAL 1342
int no_conec = 0;

void h(int sig){
    no_conec = 1;
}
void *h_rec(void *p);

pthread_t id_h_rec,id_main;

int main(int argc,char **argv){

    int a=0,b,c=0,d=0,e=0,f=-1,g,fd=-1,r=0;
    struct sockaddr_in addr_local,addr_remoto;
    
    memset((char *)&addr_local,0,sizeof(addr_local));
    memset((char *)&addr_remoto,0,sizeof(addr_remoto));
    
    id_main = pthread_self();

    char n[INET_ADDRSTRLEN]="";
    char j[INET_ADDRSTRLEN]="";
    sigset_t s;
    struct sigaction m;
    sigemptyset(&s);
    sigaddset(&s,SIGALRM);
    sigprocmask(SIG_UNBLOCK,&s,NULL);

    sscanf(argv[1],"%d",&c);
    sscanf(argv[2],"%s",n);

    m.sa_flags = 0;
    m.sa_handler = h;
    sigemptyset(&m.sa_mask);
    sigaction(SIGALRM,&m,NULL); // con SIGALRM voy a romper la espera por teclado de scanf y pongo el flag para acabar el programa porque se ha cerrado la conexión

    addr_local.sin_family = AF_INET;
    addr_local.sin_port = htons(PUERTO_LOCAL);
    addr_local.sin_addr.s_addr = INADDR_ANY;

    addr_remoto.sin_family = AF_INET;
    addr_remoto.sin_port = htons(c);
    addr_remoto.sin_addr.s_addr = inet_addr(n);

    fd = socket(AF_INET,SOCK_STREAM,0);

    if(bind(fd,(struct sockaddr *)&addr_local,sizeof(addr_local))>-1)
        printf("socket vinculado al puerto %d\n",htons(addr_local.sin_port));
    
    while((connect(fd,(struct sockaddr *)&addr_remoto,(socklen_t)sizeof(addr_remoto))<0)){
    printf("conectando a ip %s puerto %d\n",inet_ntoa(addr_remoto.sin_addr),htons(addr_remoto.sin_port));
    sleep(1);
    }

    pthread_create(&id_h_rec,NULL,h_rec,&fd);

    printf("esperando comando del teclado__\n--- 0:DETECTAR ;\n--- 1: ATERRIZAR ;\n--- 2: ACABAR\n");

    while(g!=2 && !no_conec){
        scanf("%d",&g);
        if(g>=0 && g<3){
        printf("La orden introducida es: %d\n",g);        
        send(fd,&g,sizeof(g),MSG_WAITALL);
        }
        else{
            printf("Orden introducida %d no reconocida\n",g);
        }
        printf("esperando comando del teclado__\n--- 0:DETECTAR ;\n--- 1: ATERRIZAR ;\n--- 2: ACABAR\n");
    }
    if(g==2)
    printf("Comando enviado ACABAR --> Esperando a que el servidor cierre su conexión\n");
    
    
    pthread_join(id_h_rec,NULL);
    shutdown(fd,SHUT_RDWR);
    close(fd);


return 0;

}

void *h_rec(void *p){

    int fd = *(int *)p;
    int datos = 0,r = 0;
    while((r=recv(fd,&datos,sizeof(int),0))>0)
        printf("Recibido datos = %d\n",datos);
    if(r==0){
        printf("Cerrado el socket en el servidor tcp\n");
        pthread_kill(id_main,SIGALRM);
    }
    return NULL;
}