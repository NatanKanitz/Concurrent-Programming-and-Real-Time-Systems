#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <ncurses.h>

#define NSEC_POR_SEC	(1000000000)	// Numero de nanosegundos em um segundo (1 bilhao)
#define USEC_POR_SEC	(1000000)	// Numero de microssegundos em um segundo (1 milhao)
#define NSEC_POR_USEC	(1000)		// Numero de nanosegundos em um microsegundo (1 mil)

#define FALHA 1

#define	TAM_MEU_BUFFER	1000




int cria_socket_local(void)
{
	int socket_local;		/* Socket usado na comunicac�o */

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		return -1;
	}
	return socket_local;
}



struct Planta {
	int bomba_coletor;
	int bomba_recirculacao;
	int aquecedor;
	int valvula_entrada;
	int valvula_esgoto;
	float nivel_boiler;
	float temp_boiler;
	float temp_coletor;
	float temp_canos;
};

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino)
{
	struct sockaddr_in servidor; 	/* Endereço do servidor incluindo ip e porta */
	struct hostent *dest_internet;	/* Endereço destino em formato próprio */
	struct in_addr dest_ip;		/* Endereço destino em formato ip numérico */

	if (inet_aton ( destino, &dest_ip ))
		dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
	else
		dest_internet = gethostbyname(destino);

	if (dest_internet == NULL) {
		fprintf(stderr,"Endereco de rede invalido\n");
		exit(FALHA);
	}

	memset((char *) &servidor, 0, sizeof(servidor));
	memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(porta_destino);

	return servidor;
}




void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem)
{
	/* Envia msg ao servidor */

	if (sendto(socket_local, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{
		perror("sendto");
		return;
	}
}


int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER)
{
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}

float leitorNumerico(char mensagem[], char comando[]){
	char *numStr;
	numStr = strchr(mensagem, ' ');
	memmove(numStr, numStr+1, strlen(numStr));
	float num = (float)strtod(numStr,NULL);
	return num;
}

float dialogo(int socket_local, char *argv[], char comando[]) {
    int porta_destino = atoi( argv[2]);

	struct sockaddr_in endereco_destino = cria_endereco_destino(argv[1], porta_destino);

    char msg_enviada[1000];
	char msg_recebida[1000];
	int nrec;

	envia_mensagem(socket_local, endereco_destino, comando);
	nrec = recebe_mensagem(socket_local, msg_recebida, 1000);

	return leitorNumerico(msg_recebida, comando);
}

void atualizaSensores(struct Planta *planta, char *argv[], int socket_local) {
	planta->nivel_boiler = dialogo(socket_local, argv, "nivelboiler");
	planta->temp_boiler = dialogo(socket_local, argv, "tempboiler");
	planta->temp_coletor = dialogo(socket_local, argv, "tempcoletor");
	planta->temp_canos = dialogo(socket_local, argv, "tempcanos");
}

void controle_temperatura(int socket_global, char *argv[], struct Planta *planta) {
    float temp_boiler = planta->temp_boiler; //		leitura da temperatura do boiler
    float temp_coletor = planta->temp_coletor; //    leitura da temperatura do coletor
    float temp_canos = planta->temp_canos;     //    leitura da temperatura dos canos

    float ref = 30.0; //referencia de temperatura
	float tolerancia = 0.5; 

    if (temp_boiler < ref - tolerancia){
        if(temp_coletor < ref - tolerancia){
            int atuador_aquecedor = dialogo(socket_global, argv, "aquecedor 1"); // liga aquecedor 
			planta->aquecedor = atuador_aquecedor;
        } else {
            int atuador_coletor = dialogo(socket_global, argv, "bombacoletor 1"); // liga bomba de coletor 
			planta->bomba_coletor = atuador_coletor;
        }

    } else if (temp_boiler > ref + tolerancia) {
        int atuador_aquecedor = dialogo(socket_global, argv, "aquecedor 0"); // desliga aquecedor
		planta->aquecedor = atuador_aquecedor;
        int atuador_coletor = dialogo(socket_global, argv, "bombacoletor 0"); // desliga bomba de coletor 
		planta->bomba_coletor = atuador_coletor;
    }
    if (temp_canos < ref - tolerancia){ 
        int atuador_recirculacao = dialogo(socket_global, argv, "bombacirculacao 1"); // liga bomba de recirculacao
		planta->bomba_recirculacao = atuador_recirculacao;
    } else if (temp_canos > ref + tolerancia) {
        int atuador_recirculacao = dialogo(socket_global, argv, "bombacirculacao 0"); // desliga bomba de recirculacao
		planta->bomba_recirculacao = atuador_recirculacao;
	}
}

void controle_nivel_boiler(int socket_global, char *argv[], struct Planta *planta) {
    float nivel_boiler = planta->nivel_boiler;
    float nivel_max = 0.55; 
    float nivel_min = 0.25; 
	float tolerancia = 0.10;

    if(nivel_boiler < nivel_min) {
		int atuador_valvula_entrada = dialogo(socket_global, argv, "valvulaentrada 1"); //abre valvula de entrada
        planta->valvula_entrada = atuador_valvula_entrada;
    }
    else if (nivel_boiler > nivel_max) {
        int atuador_valvula_esgoto = dialogo(socket_global, argv, "valvulaesgoto 1"); //valvula de saida
		planta->valvula_esgoto = atuador_valvula_esgoto;
    }
    else if ((nivel_boiler > nivel_min + tolerancia) && (nivel_boiler < nivel_max - tolerancia)) {
		int atuador_valvula_esgoto = dialogo(socket_global, argv, "valvulaesgoto 0"); //fecha valvula de saida
		planta->valvula_esgoto = atuador_valvula_esgoto;
		int atuador_valvula_entrada = dialogo(socket_global, argv, "valvulaentrada 0"); //fecha valvula de entrada
        planta->valvula_entrada = atuador_valvula_entrada;
    }
}

void display_tela(struct Planta *planta) {

    system("clear"); // limpa o terminal

	printf("PAINEL DE CONTROLE\n\n\n\n");

	printf("SENSORES\n\n");
    //mostra os valores na tela
    printf("Temperatura Boiler:     %.3f\n", planta->temp_boiler);
    printf("Temperatura Coletor:    %.3f\n", planta->temp_coletor);
    printf("Temperatura Canos:      %.3f\n", planta->temp_canos);
    printf("Nivel Boiler:           %.3f\n", planta->nivel_boiler);

	printf("\nATUADORES\n\n");
    //mostra os atuadores ativos
    printf("Bomba Coletor:            %d\n", planta->bomba_coletor);
    printf("Bomba Circulacao:         %d\n", planta->bomba_recirculacao);
    printf("Aquecedor:                %d\n", planta->aquecedor);
    printf("Valvula Entrada:          %d\n", planta->valvula_entrada);
    printf("Valvula Esgoto:           %d\n", planta->valvula_esgoto);
    
}


int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr,"Uso: teste-controle <endereco> <porta> \n");
		fprintf(stderr,"<endereco> eh o DNS ou IP do servidor \n");
		fprintf(stderr,"<porta> eh o numero da porta do servidor \n");
		exit(FALHA);
	}

	int socket_local = cria_socket_local();

    struct timespec t;				// Hora atual
	struct timespec tp;				// Hora de inicio para o periodo de interesse

	int periodo_ns = 30000000;			// 300 ms = 300 000 000 ns
	long diferenca_us;				// Diferenca em microsegundos

	int i = 0;

	clock_gettime(CLOCK_MONOTONIC, &t);

	tp.tv_sec = t.tv_sec + 1;
	tp.tv_nsec = t.tv_nsec;

	// Cria uma estrutura de informações da planta

	struct Planta PlantaBoiler;

	PlantaBoiler.bomba_coletor = 0;
	PlantaBoiler.bomba_recirculacao = 0;
	PlantaBoiler.aquecedor = 0;
	PlantaBoiler.valvula_entrada = 0;
	PlantaBoiler.valvula_esgoto = 0;

	atualizaSensores(&PlantaBoiler, argv, socket_local);

	display_tela(&PlantaBoiler);



    while(1){
        // Espera até o inicio do proximo periodo
		clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME, &tp, NULL);

        clock_gettime(CLOCK_MONOTONIC, &tp);

		atualizaSensores(&PlantaBoiler, argv, socket_local);

		controle_temperatura(socket_local, argv, &PlantaBoiler);

        // dialogo(socket_local, argv, "bombacirculacao 1");

		tp.tv_nsec += periodo_ns;

		while (tp.tv_nsec >= NSEC_POR_SEC) {
			controle_nivel_boiler(socket_local, argv, &PlantaBoiler);
			display_tela(&PlantaBoiler);

			tp.tv_nsec -= NSEC_POR_SEC;
			tp.tv_sec++;
		}
    }
}