//******************************************
//
//executar como ./servidor ip porta
//
//******************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>//para usar a função bzero()
#include <unistd.h>
#include <time.h>
#include <netdb.h>//função gethostbyname()
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>//para poder usar a função inet_addr()
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct{
    int ACK;
    char mensagem[100];
} Pacote, Resposta;//Struct para recebar o novaporta do novaporta

void sigchld_handler(int);//mata processos zumbis
int novoSocket();//função para criar um novo socket
int novoBind(int *,struct sockaddr_in);//função para ligar (ou associar) o socket a uma porta
int portaAleatoria(struct sockaddr_in);//pega uma porta aleatoria 
int novaPorta(int ,struct sockaddr_in, socklen_t);//define uma nova porta para um novo cliente
void enviar(Pacote, int, struct sockaddr_in, socklen_t);
void grava_Arq(FILE *,char *);//grava os dados no arquivo
char* pesquisa_Arq(FILE *,char *);//Faz a pesquisa no arquivo
void corta_Texto(char *,FILE *);//Corta a string para gravar no arquivo so os dados necessarios
int abrindoServidor(int, struct sockaddr_in, FILE*);//abre o servidor 
void armazenaDados(Pacote, FILE *);//Armazena os dados no arquivo
void clienteNovo(Pacote, struct sockaddr_in, FILE *);

int main(int argc, char **argv)
{
    int ServerSocket;//declaração do socket
    int porta;//pega a porta passada na linha de comando
    int valor_setsock;//usado na função setsockopt()
    int pid;//usado no fork()
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

    FILE *arq = fopen("Dados.txt","r+");
    puts("Aguardando Dados!!");

    signal(SIGCHLD, sigchld_handler);//Ativa o manipulador de sinais antes de entrar no loop

    abrindoServidor(ServerSocket,cliente,arq);

    close(ServerSocket);
    fclose(arq);
    return 0;
}

//Abre o servidor para receber dados
int abrindoServidor(int ServerSocket, struct sockaddr_in cliente, FILE *arq){
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
                cliente.sin_port = htons((__u_short)novaPorta(ServerSocket,cliente,len));//define uma porta nova
                close(ServerSocket);//fecha o socket atual
                clienteNovo(pacote,cliente,arq);//chama a função clienteNovo() para se comunicar com o cliente
                exit(0);//sai do processo
            }
        }
    }
}

void clienteNovo(Pacote pacote, struct sockaddr_in cliente, FILE *arq){
    printf("PID do processo: %i\n", getpid());
    //Crio um novo socket para o cliente
    int clienteSocket = novoSocket();
    socklen_t len = sizeof(cliente);
    if(novoBind(&clienteSocket,cliente) < 0){
        perror("Bind");
        return;
    }
    //Troca de dados
    while(1){
        recvfrom(clienteSocket,(char *)&pacote,sizeof(Pacote),MSG_WAITALL,(struct sockaddr *)&cliente,&len);
        pacote.mensagem[strlen(pacote.mensagem)] = '\0';
        if(pacote.mensagem[0] == 'D' || pacote.mensagem[0] == 'P'){
            //Envia o ACK para o cliente
            enviar(pacote,clienteSocket,cliente,len);
            // if(sendto(clienteSocket,&pacote,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&cliente,sizeof(cliente)) < 0){
            //     perror("Send ACK");
            //     return;
            // }
            //Imprime a mensagem na tela
            printf("Dados recebidos >> %s\n",pacote.mensagem);
            if(pacote.mensagem[0] == 'D'){//Se o texto começar com 'D' é para gravar no arquivo
                armazenaDados(pacote,arq);
            }else if(pacote.mensagem[0] == 'P'){

            }
        }/* else{//Se o texto não começar com 'D' ou 'P' ele retorna pro cliente erro
            puts("Formato invalido!!");
            if(sendto(ServerSocket,&pacote,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&cliente,sizeof(cliente)) < 0){
                perror("Send formato invalido");
            }
        } */
    }
    close(clienteSocket);
}

void armazenaDados(Pacote pacote, FILE *arq){
    corta_Texto(pacote.mensagem,arq);//tirá o D da string para salvar no arquivo
}

//Corta a string a partir do primeiro espaço " "
void corta_Texto(char *texto,FILE *arq){
    //D 1 0 2540 -22.2218 -54.8064
    char ch = texto[4];//pega a posição onde começa o tipo de gasolina
    char *aux;//vai salvar a nova string
    aux = strchr(texto,ch);
    strcpy(texto,aux);
    grava_Arq(arq,texto);//grava os dados no arquivo
;}

//Realiza a pesquisa no arquivo
char* pesquisa_Arq(FILE * arq,char *dados){

}

//Grava os dados recebidos no arquivo
void grava_Arq(FILE *arq,char *dados){
    fprintf(arq,"%s",dados);
}

//pega uma porta aleatoria 
int portaAleatoria(struct sockaddr_in cliente){
    int porta/*  = ntohs(cliente.sin_port) */;
    int Socket_serv = socket(AF_INET,SOCK_DGRAM,17);
    struct sockaddr_in velho_cliente;
    int len = sizeof(velho_cliente);

    cliente.sin_port = htons(0);
    //Do um bind no Socket_serv com o cliente.sin_port = htons(0)
    //Com isso o SO retorna uma porta aleatoria para o socket
    if(novoBind(&Socket_serv,cliente) < 0){
        perror("Bind");
        close(Socket_serv);
        exit(1);
    }
    //copia os dados para o velho_cliente
    getsockname(Socket_serv,(struct sockaddr *)&velho_cliente,&len);
    //pega nova porta
    //ntohs() converte de network byte order para host byte order
    porta = ntohs(velho_cliente.sin_port);
    printf("Nova porta >> %d\n",porta);
    //Fecho o Socket_serv para liberar a porta que foi pega pelo bind
    close(Socket_serv);
    return porta;
}

//Envia a nova porta para o cliente
int novaPorta(int socket_serv, struct sockaddr_in cliente, socklen_t len){
    puts("Esperando porta nova!!");
    Pacote novaporta;
    //variavel int que vai receber a porta nova
    int porta = portaAleatoria(cliente);
    //salvo a porta nova no pacote 
    sprintf(novaporta.mensagem,"%d",porta);
    //manda a porta nova para o cliente
    enviar(novaporta,socket_serv,cliente,len);
    puts("Porta trocada!!");
    return porta;
}

//Envia o pacote para o cliente
void enviar(Pacote pacote, int Socket, struct sockaddr_in cliente, socklen_t len){
    if(sendto(Socket,&pacote,sizeof(Pacote),0,(const struct sockaddr *)&cliente,len) < 0){
        perror("Send porta");
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