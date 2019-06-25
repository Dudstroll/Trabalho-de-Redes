//******************************************
//
//executar como ./cliente ip porta
//
//******************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>//para usar a função bzero()
#include <unistd.h>
#include <netdb.h>//função gethostbyname()
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>//para poder usar a função inet_addr()
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/time.h>

#define h_addr h_addr_list[0]//para tirar aviso de que "h_addr" não está declarado

typedef struct{
    //Salva o indicador atual da mensagem
    int indicador;
    //Salva os dados a serem enviados
    char mensagem[100];
} Pacote;//Struct que envia e recebe dados do servidor

void transmissao(int, struct sockaddr_in);
void esperaACK(int, struct sockaddr_in, socklen_t, Pacote, Pacote,int);
int novaPorta(int, struct sockaddr_in );
void enviar(Pacote, int, struct sockaddr_in, socklen_t);

int main(int argc, char **argv)
{
    int clienteSocket;//declaração do socket
    int porta;//pega a porta passada na linha de comando
    char *ip;//pega o endereço ip da linha de comando
    struct hostent *ptrh;//ponteiro para a tabela do host
    struct sockaddr_in servidor;//estrutura que salva o endereço do servidor

    if(argc>1){
        ip = (char *)malloc(sizeof(argv[1]));
        strcpy(ip,argv[1]);
        ptrh = gethostbyname(ip);
        porta = atoi(argv[2]);
    }else{
        puts("Sem parametros da linha de comando!!");
        exit(1);
    }

    bzero(&(servidor),sizeof(servidor));//Zera a estrutura sockaddr_in
    servidor.sin_family = AF_INET;//familia de protocolos internet
    servidor.sin_port = htons((__u_short)porta);//numero da porta em que sera feita a conexão
    // servidor.sin_addr = *((struct in_addr *)ptrh->h_addr);//usa o endereço passado na linha de comando
    memcpy(&servidor.sin_addr, ptrh->h_addr, ptrh->h_length);//usa o endereço passado na linha de comando
    //Criando o socket
    if((clienteSocket = socket(AF_INET,SOCK_DGRAM,17)) < 0){
        perror("Socket");
        free(ip);
        exit(1);
    }
    //Mesmo podendo usar a função connect(), em UDP nao muda nada, pois não ha conexão
    //Em UDP não precisa usar a função accept(), pois o cliente não estabelece uma conexão com o servidor
    //e o mesmo com o cliente, apenas mandam datadramas. Com isso eles nunca precisarão aceitar uma conexão, mas apenas
    //enviar e esperar os dados chegar

    //Manda uma nova porta para o servidor e assim começar uma conexão nova
    servidor.sin_port = htons((__u_short)novaPorta(clienteSocket,servidor));
    puts("Troca feita!!");
    //Inicia a transmissão de dados para o servidor
    transmissao(clienteSocket,servidor);

    free(ip);
    close(clienteSocket);

    return 0;
}

void transmissao(int clienteSocket, struct sockaddr_in servidor){
    //Pacote com os dados
    Pacote pacote;
    //Recebe os dados enviados pelo servidor
    Pacote resposta;
    int n;
    //Pega o tamanho da struct servidor
    socklen_t len = sizeof(servidor);
    int numRet = 0;//Define o numero de retransmissões
    //A primeira mensagem tem indicador 0
    pacote.indicador = 0;
    while(1){
        printf("> ");
        fgets(pacote.mensagem,99,stdin);
        if(strncmp(pacote.mensagem,"exit",strlen("exit")) == 0){
            break;
        }
        if(sendto(clienteSocket,&pacote,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&servidor,sizeof(servidor)) < 0){
            perror("Send");
        }else{
            esperaACK(clienteSocket,servidor,len,pacote,resposta,numRet);
            pacote.indicador = 1 - pacote.indicador;
        }
        // bzero(&(pacote.mensagem),strlen(pacote.mensagem));
    }
}

//Manda uma mensagem "Nova porta" para o servidor e 
//em seguida recebe uma nova porta para se comunicar com o servidor
int novaPorta(int socket_serv, struct sockaddr_in servidor){
    puts("Trocando portas!!");
    Pacote portaNova;
    //Tamanho da struct servidor
    socklen_t len = sizeof(servidor);
    sprintf(portaNova.mensagem,"Nova porta");
    //Faz uma primeira conexão para trocar as portas
    enviar(portaNova,socket_serv,servidor,len);
    // if(sendto(socket_serv,&portaNova,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&servidor,sizeof(servidor)) < 0){
    //     perror("Send porta");
    //     close(socket_serv);
    //     exit(1);
    // }
    //Recebe uma nova porta do servidor
    if(recvfrom(socket_serv,(char *)&portaNova,sizeof(Pacote),MSG_WAITALL,(struct sockaddr *)&servidor,&len) < 0){
        perror("Recvfrom porta");
        close(socket_serv);
        exit(1);
    }
    //converte a porta de string para inteiro
    int porta = atoi(portaNova.mensagem);
    return porta;
}

//Se nao receber o ACK dentro do tempo limite manda denovo os dados
void esperaACK(int clienteSocket, struct sockaddr_in servidor,socklen_t len,Pacote pacote, Pacote resposta,int numRet){
    struct timeval timeout;
    timeout.tv_sec = 1;//1 Segundo
    timeout.tv_usec = 250;//250 microsegundos = 25 milisegundos
    numRet++;
    if(numRet < 5){
        puts("Esperando ACK!!");
        if(setsockopt(clienteSocket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0){
            perror("Erro");
        }
        if(recvfrom(clienteSocket,(char *)&resposta,sizeof(Pacote),0,(struct sockaddr *)&servidor,&len) < 0){
            //Se entra aqui é porque não recebeu resposta do servidor
            if((numRet+1) == 5){
                puts("Retransmissão limite alcançada. Envio cancelado!!");
            }else{
                puts("Tempo esgotado. Enviando outro pacote!!");
                // sendto(clienteSocket,&pacote,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&servidor,sizeof(servidor));
                enviar(pacote,clienteSocket,servidor,len);
                esperaACK(clienteSocket,servidor,len,pacote,resposta,numRet);
            }
        }else{
            //Se entrar aqui é porque recebeu o ACK do servidor
            if(strncmp(resposta.mensagem,"Dados incorretos",strlen("Dados incorretos")) == 0){
                puts("Dados no formato incorreto");
            }else{
                //ACK diferentes, reenvia o pacote
                if(resposta.indicador != pacote.indicador){
                    puts("ACK diferente do esperado!!");
                    if((numRet+1) == 5){
                        puts("Retransmissão limite alcançada. Envio cancelado!!");
                    }else{
                        puts("Enviando os dados novamente!!");
                        enviar(pacote,clienteSocket,servidor,len);
                        esperaACK(clienteSocket,servidor,len,pacote,resposta,numRet);
                    }
                }else{
                    if(strncmp(resposta.mensagem,"Pacote duplicado",strlen("Pacote duplicado")) == 0){
                        puts("Pacotes duplicados no servidor. Parando retransmissão!!");
                        puts("Pacote entregue!!");
                    }else{
                        //O servidor recebeu o pacote sem erros
                        puts("Pacote entregue!!");
                    }
                }
            }
        }
    }
}

//Envia o pacote para o servidor
void enviar(Pacote pacote, int Socket, struct sockaddr_in servidor, socklen_t len){
    if(sendto(Socket,&pacote,sizeof(Pacote),0,(const struct sockaddr *)&servidor,len) < 0){
        perror("Send porta");
        close(Socket);
        exit(1);
    }
}
//IP do pc: 192.168.25.32
//D 0 2540 -22.2218 -54.8064