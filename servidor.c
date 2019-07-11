//******************************************
//
//executar como ./servidor porta
//
//******************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>//para usar a função bzero()
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <netdb.h>//função gethostbyname()
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>//para poder usar a função inet_addr()
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>//Para poder usar a função isdigit()

//Constante PI para o calculo da distancia
#define PI 3.14159265358979323846

typedef struct{
    //salva o cheksum
    unsigned char cheksum;
    //Salva o indicador atual da mensagem
    int indicador;
    //Salva os dados a serem enviados
    char mensagem[100];
} Pacote;//Struct que envia e recebe dados do cliente

//mata processos zumbis
void sigchld_handler(int);
//função para criar um novo socket
int novoSocket();
//função para ligar (ou associar) o socket a uma porta
int novoBind(int *,struct sockaddr_in);
//pega uma porta aleatoria 
int portaAleatoria(struct sockaddr_in);
//define uma nova porta para um novo cliente
int novaPorta(int ,struct sockaddr_in, socklen_t);
//Armazena os dados no arquivo
void armazenaDados(char *);
//Corta a string para gravar no arquivo so os dados necessarios
char *corta_Texto(char *, const char);
//grava os dados no arquivo
void grava_Arq(char *, const char);
//Verifica se o arquivo está vazio
int arquivoVazio(const char );
//Faz a pesquisa no arquivo
void pesquisa_Arq(char *, const char, char *);
//calcula a distancia entre as coordenadas
double Distancia(double, double, double, double);
//abre o servidor 
int abrindoServidor(int, struct sockaddr_in, struct sockaddr_in);
//recebe e trata os dados enviados pelo cliente
void clienteNovo(Pacote, struct sockaddr_in, struct sockaddr_in);
//Verifica se os dados então corretos; return 0 para falso e return 1 para verdadeiro
int verificaDados(char *);
//Verifica se o pacote tem exatamente 5 partes Ex: D 0 4439 -21.22182 -54.8064
int numPartes(char *);
//Verifica se o pacote atual é igual ao ultimo recebido, pacote duplicado
int pacoteDuplicado(Pacote,Pacote);
//Envia uma struct para o cliente
void enviar(Pacote, int, struct sockaddr_in, socklen_t);
//Calcula o checksum
unsigned char CheckSum(unsigned char *ptr, size_t sz);

int main(int argc, char **argv)
{
    int ServerSocket;//declaração do socket
    int porta;//pega a porta passada na linha de comando
    int valor_setsock;//usado na função setsockopt()
    struct sockaddr_in servidor,cliente;//estrutura que salva o endereço do servidor; salva o endereço do cliente

    if(argc>1){
        porta = atoi(argv[1]);
    }else{
        puts("Sem parametros da linha de comando!!");
        exit(1);
    }

    bzero(&(servidor),sizeof(servidor));//Zera a estrutura sockaddr_in
    servidor.sin_family = AF_INET;//familia de protocolos internet
    servidor.sin_port = htons((__u_short)porta);//numero da porta em que sera feita a conexão
    servidor.sin_addr.s_addr = INADDR_ANY;//pega endereço ip do pc
    //Criando o socket
    if((ServerSocket = novoSocket()) < 0){
        perror("Socket");
        exit(1);
    }/* else puts("Socket criado!!"); */
    if((setsockopt(ServerSocket,SOL_SOCKET,SO_REUSEADDR,&valor_setsock,sizeof(valor_setsock))) < 0){
        perror("Set Socket");
        close(ServerSocket);
        exit(1);
    }
    //Liga (ou associa) o socket a uma porta 
    if(novoBind(&ServerSocket,servidor) < 0){
        perror("Bind");
        close(ServerSocket);
        exit(1);
    }/* else puts("Bind realizado!!"); */

    // FILE *arq = fopen("Dados.txt","r+");
    puts("Servidor aberto!!");

    signal(SIGCHLD, sigchld_handler);//Ativa o manipulador de sinais antes de entrar no loop

    abrindoServidor(ServerSocket,cliente,servidor);

    close(ServerSocket);
    // fclose(arq);
    return 0;
}

//Abre o servidor para receber dados
int abrindoServidor(int ServerSocket, struct sockaddr_in cliente, struct sockaddr_in servidor){
    socklen_t len = sizeof(cliente);//Pega o tamanho da estrutura cliente; coloco o tamanho do cliente para evitar problemas com o sendto
    int n;//Pega o tamanho da mensagem
    Pacote pacote;

    while(1){
        //A função recvfrom() retorna o tamanho da mensagem
        //toda mensagem que for recebida pelo recvfrom abaixo será para definir uma nova porta 
        //para um novo cliente. Essa porta será usada dentro do fork()
        n = recvfrom(ServerSocket,(char *)&pacote,sizeof(Pacote),MSG_WAITALL,(struct sockaddr *)&cliente,&len);
        if(n > 0){
            if(fork() == 0){//Cria um novo processo para atender o cliente novo
                servidor.sin_port = htons((__u_short)novaPorta(ServerSocket,cliente,len));//define uma porta nova
                close(ServerSocket);//fecha o socket atual
                clienteNovo(pacote,cliente,servidor);//chama a função clienteNovo() para se comunicar com o cliente
                exit(0);//sai do processo
            }
        }
    }
}

void clienteNovo(Pacote pacote, struct sockaddr_in cliente, struct sockaddr_in servidor){
    printf("Cliente novo no processo: %i\n", getpid());
    //Crio um novo socket para o cliente
    int clienteSocket = novoSocket();
    //Salva o pacote anterior
    Pacote pacoteAnte;
    //Como o proximo pacote é o primeiro não tem como comparar com um anterior, pois não tem
    //assim pacoteAnte é preenchido com algo para ter diferença com o pacote recebido
    //o indicador recebe -1 so para a primeira comparação depois ele recebe 0 ou 1
    pacoteAnte.indicador = -1;
    sprintf(pacoteAnte.mensagem,"primeiro texto");
    socklen_t len = sizeof(cliente);

    //recebe o pacote para a soma do checksum
    unsigned char aux[75];
    //Associa uma porta para o socket
    //Tenho que usar a struct servidor, pois não tem como dar bind no socket
    //usando o endereço do cliente
    if(novoBind(&clienteSocket,servidor) < 0){
        perror("Bind Cliente Novo");
        return;
    }
    //Troca de dados
    while(1){
        //espera dados do cliente
        recvfrom(clienteSocket,&pacote,sizeof(Pacote),MSG_WAITALL,(struct sockaddr *)&cliente,&len);
        //Exemplo: D 1 0 2540 -22.2218 -54.8064
        pacote.mensagem[strlen(pacote.mensagem)] = '\0';
        //se o pacote for "exit" finaliza esse processo
        if(strncmp(pacote.mensagem,"exit",strlen("exit")) == 0){
            break;
        }
        if(pacoteDuplicado(pacote,pacoteAnte)){
            //entra aqui se os pacotes forem diferentes
            if(verificaDados(pacote.mensagem)){
                //muda a string para unsigned char para fazer o checksum
                strcpy((char *)aux,pacote.mensagem);
                //se for igual, o pacote não foi corrompido
                if(CheckSum(aux,strlen((const char *)aux)) == pacote.cheksum){
                    //salva o pacote atual para comparar com o proximo pacote
                    pacoteAnte.indicador = pacote.indicador;
                    strcpy(pacoteAnte.mensagem,pacote.mensagem);
                    //Nesse ponto os dados estão corretos
                    //Envia o ACK para o cliente
                    enviar(pacote,clienteSocket,cliente,len);
                    //Imprime a mensagem na tela
                    printf("Dados recebidos: %s",pacote.mensagem);
                    //Se o texto começar com 'D' é para gravar no arquivo
                    if(pacote.mensagem[0] == 'D'){
                        armazenaDados(pacote.mensagem);
                        puts("Dados salvos!!");
                        //Se o texto começar com 'P' faz a pesquisa no arquivo
                    }else if(pacote.mensagem[0] == 'P'){
                        //verifica se o arquivo ta vazio
                        if(arquivoVazio(pacote.mensagem[2])){
                            //Arquivo vazio
                            sprintf(pacote.mensagem,"Arquivo sem dados!!");
                            enviar(pacote,clienteSocket,cliente,len);
                        }else{
                            //Arquivo não está vazio
                            char resultado[50];
                            pesquisa_Arq(pacote.mensagem,pacote.mensagem[2],resultado);
                            strcpy(pacote.mensagem,resultado);
                            enviar(pacote,clienteSocket,cliente,len);
                        }
                    }
                }else{
                    //Deu diferença no checksum
                    puts("CheckSum invalido. Pacote Corrompido!!");
                    sprintf(pacote.mensagem,"CheckSum invalido");
                    enviar(pacote,clienteSocket,cliente,len);
                }
            }else{//Se a mensagem não tiver no padrão retorna para o cliente que os dados estão incorretos
                puts("Formato invalido!!");
                sprintf(pacote.mensagem,"Dados incorretos");
                enviar(pacote,clienteSocket,cliente,len);
            }
        }else{
            puts("Pacote duplicado!!");
            sprintf(pacote.mensagem,"Pacote duplicado");
            enviar(pacote,clienteSocket,cliente,len);
        }
    }
    close(clienteSocket);
    printf("Cliente do processo |%i| saiu!!\n", getpid());
}

//pega uma porta aleatoria 
int portaAleatoria(struct sockaddr_in cliente){
    int porta/*  = ntohs(cliente.sin_port) */;
    int Socket_serv = socket(AF_INET,SOCK_DGRAM,17);
    struct sockaddr_in aux_cliente;
    socklen_t len = sizeof(aux_cliente);

    cliente.sin_family = AF_INET;
    cliente.sin_addr.s_addr = INADDR_ANY;
    cliente.sin_port = htons(0);

    //Do um bind no Socket_serv com o cliente.sin_port = htons(0)
    //Com isso o SO retorna uma porta aleatoria para o socket
    if(novoBind(&Socket_serv,cliente) < 0){
        perror("Bind porta aleatoria");
        close(Socket_serv);
        exit(1);
    }
    //copia os dados para o aux_cliente
    getsockname(Socket_serv,(struct sockaddr *)&aux_cliente,&len);
    //pega nova porta
    //ntohs() converte de network byte order para host byte order
    porta = ntohs(aux_cliente.sin_port);
    //Fecho o Socket_serv para liberar a porta que foi pega pelo bind
    close(Socket_serv);
    return porta;
}

//Envia a nova porta para o cliente
int novaPorta(int socket_serv, struct sockaddr_in cliente, socklen_t len){
    Pacote novaporta;
    //variavel int que vai receber a porta nova
    int porta = portaAleatoria(cliente);
    //salvo a porta nova no pacote 
    sprintf(novaporta.mensagem,"%d",porta);
    //manda a porta nova para o cliente
    enviar(novaporta,socket_serv,cliente,len);
    return porta;
}
//Verifica se o pacote é duplicado
int pacoteDuplicado(Pacote pacoteAtual, Pacote pacoteAnte){
    if(strncmp(pacoteAtual.mensagem,pacoteAnte.mensagem,strlen(pacoteAnte.mensagem)) == 0
    && pacoteAtual.indicador == pacoteAnte.indicador){
        return 0;//É duplicado
    }else{
        return 1;//É diferente
    }
}

//Verifica se os dados então corretos
//exemplo de pacote correto -> //P 2 20 -22.2218 -54.8064 //Pesquisa
//                              D 0 2540 -22.2218 -54.8064 //Dados
int verificaDados(char *dado){
    int x = 0;
    //verifica se tem mais de dois espaços na mensagem
    while(dado[x] != '\0'){
        if(dado[x] == ' ' && dado[x+1] == ' '){
            puts("Sequencia de espaços detectada!!");
            return 0;
        }
        x++;
    }
    x = 0;
    //Se o tamanho da string for menor que 2 da erro
    if(strlen(dado) <= 2){
        puts("Erro no tamanho!");
        return 0;
    }
    if(numPartes(dado)){
        puts("Erro na formatação!");
        return 0;
    }
    //divide a string em partes para verificação
    char copia[50];
    strcpy(copia,dado);
    char *parte;
    const char letra[2] = " ";
    parte = strtok(copia,letra);
    //conta os sinais
    int cont_sinais = 0;
    //tipo de mensagem
    if(parte[0] == 'D'){
        //Pega o tipo de gasolina
        parte = strtok(NULL,letra);
        if(parte[0] != '0' && parte[0] != '1' && parte[0] != '2'){
            puts("Erro no tipo de combustivel");
            return 0;
        }
        //Pega o preço do combustivel
        parte = strtok(NULL,letra);
        while(parte[x] != '\0'){
            if(!isdigit(parte[x])){
                puts("Erro no valor");
                return 0;
            }
            x++;
        }
        x = 0;
        //Pega a coordenada X
        parte = strtok(NULL,letra);
        while(parte[x] != '\0'){
            if(parte[x] == '-' || parte[x] == '.'){
                cont_sinais++;
                if(cont_sinais > 2){
                    puts("Mais de dois sinais!!");
                    return 0;
                }
            }
            if(parte[x] != '-' && parte[x] != '.'){
                if(!isdigit(parte[x])){
                    puts("Erro na coordenada X");
                    return 0;
                }
            }
            x++;
        }
        x = 0;
        cont_sinais = 0;
        //Pega a coordenada Y
        parte = strtok(NULL,letra);
        while(parte[x] != '\0'){
            if(parte[x] == '-' || parte[x] == '.'){
                cont_sinais++;
                if(cont_sinais > 2){
                    puts("Mais de dois sinais!!");
                    return 0;
                }
            }
            if(parte[x] != '-' && parte[x] != '.' && parte[x] != '\n'){
                if(!isdigit(parte[x])){
                    puts("Erro na coordenada Y");
                    return 0;
                }
            }
            x++;
        }
        //Se chegou aqui é porque esta certo
        return 1;
    }else{
        if(parte[0] == 'P'){
            //Pega o tipo de gasolina
            parte = strtok(NULL,letra);
            if(parte[0] != '0' && parte[0] != '1' && parte[0] != '2'){
                puts("Erro no tipo de combustivel");
                return 0;
            }
            //Pega o raio de busca
            parte = strtok(NULL,letra);
            while(parte[x] != '\0'){
                if(!isdigit(parte[x])){
                    puts("Erro no raio de busca");
                    return 0;
                }
                x++;
            }
            x = 0;
            //Pega a coordenada X
            parte = strtok(NULL,letra);
            while(parte[x] != '\0'){
                if(parte[x] == '-' || parte[x] == '.'){
                    cont_sinais++;
                    if(cont_sinais > 2){
                        puts("Mais de dois sinais!!");
                        return 0;
                    }
                }
                if(parte[x] != '-' && parte[x] != '.'){
                    if(!isdigit(parte[x])){
                        puts("Erro na coordenada X");
                        return 0;
                    }
                }
                x++;
            }
            x = 0;
            cont_sinais = 0;
            //Pega a coordenada Y
            parte = strtok(NULL,letra);
            while(parte[x] != '\0'){
                if(parte[x] == '-' || parte[x] == '.'){
                    cont_sinais++;
                    if(cont_sinais > 2){
                        puts("Mais de dois sinais!!");
                        return 0;
                    }
                }
                if(parte[x] != '-' && parte[x] != '.' && parte[x] != '\n'){
                    if(!isdigit(parte[x])){
                        puts("Erro na coordenada Y");
                        return 0;
                    }
                }
                x++;
            }
            //Se chegou aqui é porque esta certo
            return 1;
        }else{
            puts("Erro D ou P");
            return 0;
        }
    }
}

//Verifica se o pacote tem exatamente 5 partes Ex: D 0 4439 -21.22182 -54.8064
int numPartes(char *dado){
    char *aux;
    aux = (char *)malloc(strlen(dado)*sizeof(char));
    strcpy(aux,dado);
    char *token;
    const char letra[2] = " ";
    //se o cont > 5, tem erro na formatação do pacote
    int cont = 0;
    //pega a primeira parte
    token = strtok(aux,letra);
    //caminha pelas outras partes
    while(token != NULL){
        token = strtok(NULL,letra);
        cont++;
    }
    //formato errado
    if(cont > 5){
        free(aux);
        return 1;
    }
    //formato correto
    free(aux);
    return 0;
}

//Verifica se o arquivo está vazio
int arquivoVazio(const char tipo){
    char nome[25];
    if(tipo == '0'){
        strcpy(nome,"diesel.txt");
    }else{
        if(tipo == '1'){
            strcpy(nome,"alcool.txt");
        }else{
            if(tipo == '2'){
                strcpy(nome,"gasolina.txt");
            }
        }
    }
    FILE *arquivo = fopen(nome,"a");
	long tamanho = 0;

	if(arquivo != NULL){
		fseek(arquivo,0,SEEK_END);
		tamanho = ftell(arquivo);
		if(tamanho == 0){
			puts("Arquivo sem dados!!");
            fclose(arquivo);
            return 1;
		}
	}
    fclose(arquivo);
    return 0;
}

void armazenaDados(char *texto){
    char aux = texto[2];
    //tirá o D da string para salvar no arquivo
    strcpy(texto,corta_Texto(texto,texto[4]));
    //grava os dados no arquivo, texto[2] para passar o tipo de gasolina
    //e gravar em arquivo separado
    grava_Arq(texto,aux);
}

//Corta a string para manipular os dados
char *corta_Texto(char *texto, const char ch){
    //D 0 2540 -22.2218 -54.8064
    char *aux;//vai salvar a nova string
    aux = strchr(texto,ch);
    // printf("%s\n",aux);
    return aux;
}

//Grava os dados recebidos no arquivo
void grava_Arq(char *dados, const char tipo){
    //Verifica o tipo de combustivel para abrir o arquivo certo e gravar
    char nome[50];
    if(tipo == '0'){
        strcpy(nome,"diesel.txt");
    }else{
        if(tipo == '1'){
            strcpy(nome,"alcool.txt");
        }else{
            if(tipo == '2'){
                strcpy(nome,"gasolina.txt");
            }
        }
    }
    // printf("%s\n",dados);
    FILE *arquivo = fopen(nome,"a");
    fprintf(arquivo,"%s",dados);
    fclose(arquivo);
}

//Realiza a pesquisa no arquivo
void pesquisa_Arq(char *dados, const char tipo, char *resultado){
    //P 0 20 -22.2218 -54.8064
    char nome[25];
    if(tipo == '0'){
        strcpy(nome,"diesel.txt");
    }else{
        if(tipo == '1'){
            strcpy(nome,"alcool.txt");
        }else{
            if(tipo == '2'){
                strcpy(nome,"gasolina.txt");
            }
        }
    }
	FILE *arquivo = fopen(nome,"r");
    int raio;
    char *aux;
    double latCli,longCli;
    aux = strtok(dados," ");//corta o P
    aux = strtok(NULL," ");//corta o tipo de gasolina
    aux = strtok(NULL," ");//pega o raio
    raio = atoi(aux);//converte o raio para inteiro
    aux = strtok(NULL," ");//pega a latitude
    latCli = atof(aux);//muda de string para double
    aux = strtok(NULL," ");///pega a longitude
    longCli = atof(aux);//muda de string para double
    //dados do arquivo
	float precom = 9999.0, preco;
    double lat,longi;
    //dados do menor preço
    double latm,longim;
    char texto[50];
    //verifica se tem postos na area de pesquisa
    int cont=0;
    //pega a distancia
    double dist;
    //salva a distancia para mandar para o cliente
    double menorDist;
    while(fgets(texto,sizeof(texto),arquivo) != NULL){
        //pega o preço 
       	aux = strtok(texto," ");
		preco = atof(aux);
		aux = strtok(NULL," ");
		lat = atof(aux);
		aux = strtok(NULL," ");
		longi = atof(aux);
        dist = Distancia(latCli,longCli,lat,longi);
        if(dist < raio){
            cont++;
			if(preco < precom){
				precom = preco;//salva o preço
				latm = lat;//salva a latitude
				longim = longi;//salva a longitude
                menorDist = dist;
			}
		}
    }
    //se cont = 0, então nenhum posto foi encontrado na área
    if(cont == 0){
        puts("Nenhum posto na area!!");
        sprintf(resultado,"Nenhum posto na area!!");
        //limpa o buffer de saida
        fflush(stdout);
        fclose(arquivo);
        return;
    }
    puts("======================================");
    if(tipo == '0'){
        puts("Resultado para diesel: ");
    }else{
        if(tipo == '1'){
            puts("Resultado para alcool: ");
        }else{
            if(tipo == '2'){
                puts("Resultado para gasolina: ");
            }
        }
    }
    printf("Preço: R$%0.4f\n",precom/1000);
    printf("Latitude: %0.7f\n",latm);
    printf("Longitude: %0.7f\n",longim);
    printf("Distancia: %0.2fKM\n",menorDist);
    puts("======================================");
    //usa o fprintf para gravar o retorno na string
    //sprintf(str, "Value of Pi = %f", M_PI);
    sprintf(resultado,"%c %0.4f %0.7f %0.7f %0.2f \n",tipo,precom/1000,latm,longim,menorDist);
    fclose(arquivo);
}

//calcula a distancia entre as coordenadas
double Distancia(double latitude1, double longitude1, double latitude2, double longitude2){
    int RaioDaTerra = 6371; //Raio da terra em kilometros
	//Pega a diferença entre dois pontos 
	//e converte a diferença em radianos
    double nDLat = (latitude2 - latitude1) * (PI/180);
    double nDLon = (longitude2 - longitude1) * (PI/180);
    double nA = pow ( sin(nDLat/2), 2 ) + cos(latitude1) * cos(latitude2) * pow ( sin(nDLon/2), 2 );
 
    double nC = 2 * atan2( sqrt(nA), sqrt( 1 - nA ));
    double nD = RaioDaTerra * nC;
 
    return nD; //Retorna a distância entre os pontos
}

//Calcula o checksum
unsigned char CheckSum(unsigned char *ptr, size_t sz){
    unsigned char chk = 0;
    while (sz-- != 0)
        chk -= *ptr++;
    return chk;
}

//Envia o pacote para o cliente
void enviar(Pacote pacote, int Socket, struct sockaddr_in cliente, socklen_t len){
    if(sendto(Socket,&pacote,sizeof(Pacote),0,(const struct sockaddr *)&cliente,len) < 0){
        perror("Send");
        close(Socket);
        exit(1);
    }
}

//função para ligar (ou associar) o socket a uma porta 
int novoBind(int *socket,struct sockaddr_in servidor){
    return bind(*socket,(struct sockaddr *)&servidor,sizeof(servidor));
}

//função para criar um novo socket
int novoSocket(){
    return socket(AF_INET,SOCK_DGRAM,17);
}

//chama a função waitpid() para qualquer filho que foi desconectado
void sigchld_handler(int signo) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}
