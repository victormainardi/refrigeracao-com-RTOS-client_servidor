//INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


//MUTEX E VARCOND
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t d = PTHREAD_COND_INITIALIZER;

//DECLARACAO VAR
int newsockfd;
int vet[3];
int x = 0;
static int Temperatura_atual;
static int TemperaturaCritica = 8;
static int sirene = 0;
static int ar_cond = 0;
static int alertaVermelho = 0;
static int alertaVerde = 0;

//FUNCAO SEND
void send_values(int val, char text[]){
  char valores[5], texto[20];
	bzero(valores, sizeof(valores));
	bzero(texto, sizeof(texto));
	sprintf(valores, "%d", val);
	sprintf(texto,"%s:%s", text, valores);
	int n = write(newsockfd,texto,50);
  if (n < 0) {
    printf("Erro escrevendo no socket!\n");
    exit(1);
	}
}

//FUNCAO CLIENT
void *cliente(void *arg){
 
    int i, n;
    char buffer[256];
    
    while (1) {
    	bzero(buffer,sizeof(buffer));
     	n = read(newsockfd,buffer,50);
        
	x = 1; //seta a variavel de condicao

        if(strcmp(buffer, "da\n")==0){
    		pthread_mutex_lock(&m);
    		vet[0] = 0;
    		//desligar alarme
    		printf("\n Desligou o alarme\n");
    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);
    	
    	}else if(strcmp(buffer, "la\n")==0){
    		pthread_mutex_lock(&m);
    		//ligar alarme
    		vet[0] = 1;
    		printf("\n Ligou o alarme\n");
    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);
    	}else if(strcmp(buffer, "va\n")==0){
    		pthread_mutex_lock(&m);
    		//verifica alarme
		if(vet[0] == 0)			
			write(newsockfd,"Alarme Desligado",20);
		if(vet[0] == 1)
			write(newsockfd,"Alarme Ligado",20);
    		printf("\n Verificou o alarme\n");

    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);
    	
    	}else if(strcmp(buffer, "v+\n")==0){
    		pthread_mutex_lock(&m);
    		//incrementa valor de ajuste
    		vet[1]++;
    		printf("\n valor de ajuste: %d\n", vet[1]);
    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);

    	}else if(strcmp(buffer, "v-\n")==0){
    		pthread_mutex_lock(&m);
    		//decrementa valor de ajuste
    		vet[1]--;
    		printf("\n valor de ajuste: %d\n", vet[1]);
    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);

    	}else if(strcmp(buffer, "vl\n")==0){
    		pthread_mutex_lock(&m);
    		//envia a Temperatura_atual para o monitor
    		vet[2] = Temperatura_atual;
    		printf("\n valor lido: %d\n", vet[2]);
    		pthread_cond_signal(&d);
    		pthread_mutex_unlock(&m);

    	}else{
            if(strlen(buffer)>0)
            	printf("Recebeu: %s - %lu\n", buffer,strlen(buffer));
            
            if (n < 0) {
                printf("Erro lendo do socket!\n");
                exit(1);
            }

        }
    }
}
struct periodic_info {
        int sig;
        sigset_t alarm_sig;
};

//MAKE PERIOD

static int make_periodic(int unsigned period, struct periodic_info *info)
{
        static int next_sig;
        int ret;
        unsigned int ns;
        unsigned int sec;
        struct sigevent sigev;
        timer_t timer_id;
        struct itimerspec itval;

        /* Initialise next_sig first time through. We can't use static
           initialisation because SIGRTMIN is a function call, not a constant */
        if (next_sig == 0)
                next_sig = SIGRTMIN;
        /* Check that we have not run out of signals */
        if (next_sig > SIGRTMAX)
                return -1;
        info->sig = next_sig;
        next_sig++;
        /* Create the signal mask that will be used in wait_period */
        sigemptyset(&(info->alarm_sig));
        sigaddset(&(info->alarm_sig), info->sig);

        /* Create a timer that will generate the signal we have chosen */
        sigev.sigev_notify = SIGEV_SIGNAL;
        sigev.sigev_signo = info->sig;
        sigev.sigev_value.sival_ptr = (void *)&timer_id;
        ret = timer_create(CLOCK_MONOTONIC, &sigev, &timer_id);
        if (ret == -1)
                return ret;

        /* Make the timer periodic */
        sec = period / 1000000;
        ns = (period - (sec * 1000000)) * 1000;
        itval.it_interval.tv_sec = sec;
        itval.it_interval.tv_nsec = ns;
        itval.it_value.tv_sec = sec;
        itval.it_value.tv_nsec = ns;
        ret = timer_settime(timer_id, 0, &itval, NULL);
        return ret;
}
//WAIT PERIOD
static void wait_period(struct periodic_info *info)
{
        int sig;
        sigwait(&(info->alarm_sig), &sig);
}


//TAREFA PERIODICA SENSOR
static void *task_leituraSensor(void *arg)
{
        struct periodic_info info;

        printf("\nThread 1 period 500ms\n");
      //  make_periodic(500000, &info);
        make_periodic(5500000, &info);
       
        while (1) {
                pthread_mutex_lock(&m);
                /// INPUT ANALOGICO  
                //LEITURA DE TEMPERATURAS ENTRE 10º C e 50º C
                Temperatura_atual = 10 + rand() % 50 ; // int SensorTempTensao = analogRead(pino_lm);
                printf("\nTemperatura atual: %d\n", Temperatura_atual);
                pthread_mutex_unlock(&m);
                wait_period(&info);
        }
        return NULL;
}

//TAREFA PERIODICA ATUADORES
static void *task_atuadores(void *arg)
{
        struct periodic_info info;

        printf("\nThread 2 period 500ms\n");
        make_periodic(5500000, &info);
        make_periodic(500000, &info);
        while (1) {
                pthread_mutex_lock(&m);
                /// ALERTA DO BUZZER SE ATINGIR TEMPERATURA CRITICA / IMPLEMENTAÇÃO NO MICROCONTROLADOR ATMEGA USANDO FREERTOS E SEMÁFORO
                if(Temperatura_atual > TemperaturaCritica){
                        sirene = 1;
                }
                /// LIGA ATUADOR DE REFRIGERAÇÃO
                if(Temperatura_atual > 32){
                        ar_cond = 1; // digitalWrite(ar_cond, HIGH)
                }
                /// DESLIGA ATUADOR DE REFRIGERAÇÃO
                if(Temperatura_atual < 20){
                        ar_cond = 0; // digitalWrite(ar_cond, LOW)
                }
                /// LIGA SINAL DE ALERTA CRITICO INFORMANDO QUE A TEMPERATURA ESTÁ SUBINDO
                if(Temperatura_atual > 28){
                        alertaVermelho = 1; // digitalWrite(LED_VM, HIGH)
                }
                /// LIGA SINAL DE ALERTA VERDE INFORMANDO QUE A TEMPERATURA ESTÁ REDUZINDO
                if(Temperatura_atual < 24){ // digitalWrite(LED_VD, LOW)
                        alertaVerde = 1;
                }
                pthread_mutex_unlock(&m);
                wait_period(&info);
        }
        return NULL;
}

//TAREFA PERIODICA DISPLAY

//THREAD VAR COND


//VOID SETUP

