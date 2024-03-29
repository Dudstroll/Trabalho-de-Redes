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
    //salva o cheksum
    unsigned char cheksum;
    //Salva o indicador atual da mensagem
    int indicador;
    //Salva os dados a serem enviados
    char mensagem[100];
} Pacote;//Struct que envia e recebe dados do servidor

//Envia os dados para o servidor
void transmissao(int, struct sockaddr_in);
//Espera o ACK do servidor
void esperaACK(int, struct sockaddr_in, socklen_t, Pacote, Pacote,int);
//Espera uma nova porta do servidor
int novaPorta(int, struct sockaddr_in );
//Envia os dados para o servidor
void enviar(Pacote, int, struct sockaddr_in, socklen_t);
//Calcula o checksum
unsigned char CheckSum(unsigned char *ptr, size_t sz);
//Mostra as informações sobre o posto recebido pelo servidor
void mostraPosto(char *);
//Receebe o resultado da pesquisa no servidor
void recebePesquisa(Pacote, int, struct sockaddr_in, socklen_t);

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
    //Pega o tamanho da struct servidor
    socklen_t len = sizeof(servidor);
    int numRet = 0;//Define o numero de retransmissões
    //A primeira mensagem tem indicador 0
    pacote.indicador = 0;
    //usado no cheksum
    unsigned char aux[75];

    while(1){
        printf("> ");
        fgets(pacote.mensagem,sizeof(pacote.mensagem),stdin);
        //finaliza a conexão com o servidor
        if(strncmp(pacote.mensagem,"exit",strlen("exit")) == 0){
            sendto(clienteSocket,&pacote,sizeof(Pacote),MSG_CONFIRM,(const struct sockaddr *)&servidor,sizeof(servidor));
            break;
        }
        strcpy((char *)aux,pacote.mensagem);
        pacote.cheksum = CheckSum(aux,strlen((const char *)aux));
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
    timeout.tv_usec = 250000;//250000 microsegundos = 250 milisegundos
    numRet++;
    if(numRet < 5){
        puts("Esperando ACK!!");
        if(setsockopt(clienteSocket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0){
            perror("Erro");
        }
        //Espera o ACK do servidor
        if(recvfrom(clienteSocket,&resposta,sizeof(Pacote),0,(struct sockaddr *)&servidor,&len) < 0){
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
                //dados estão em um formato invalido, por isso não há retransmissão
                puts("Dados no formato incorreto");
            }else{
                //Com ACKs diferentes, reenvia o pacote
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
                    //ACK está correto, mas falta verificar o checksum
                    if(strncmp(resposta.mensagem,"CheckSum invalido",strlen("CheckSum invalido")) == 0){
                        puts("Pacote foi corrompido. Enviando os dados novamente!!");
                        if((numRet+1) == 5){
                            puts("Retransmissão limite alcançada. Envio cancelado!!");
                        }else{
                            enviar(pacote,clienteSocket,servidor,len);
                            esperaACK(clienteSocket,servidor,len,pacote,resposta,numRet);
                        }
                    }else{
                        //checksum está correto
                        //recebe que há pacotes duplicados no servidor e para a retransmissão
                        if(strncmp(resposta.mensagem,"Pacote duplicado",strlen("Pacote duplicado")) == 0){
                            puts("Pacotes duplicados no servidor. Parando retransmissão!!");
                            puts("Pacote entregue!!");
                            if(pacote.mensagem[0] == 'P'){
                                recebePesquisa(resposta,clienteSocket,servidor,len);
                            }
                        }else{
                            //O servidor recebeu o pacote sem erros
                            puts("Pacote entregue!!");
                            //espera os dados do servidor se for uma pesquisa
                            if(pacote.mensagem[0] == 'P'){
                                recebePesquisa(resposta,clienteSocket,servidor,len);
                            }
                        }
                    }
                }
            }
        }
    }
}

void recebePesquisa(Pacote resposta, int clienteSocket, struct sockaddr_in servidor, socklen_t len){
    recvfrom(clienteSocket,&resposta,sizeof(Pacote),MSG_WAITALL,(struct sockaddr *)&servidor,&len);
    //verifica a resposta do servidor 
    if(strncmp(resposta.mensagem,"Arquivo sem dados!!",strlen("Arquivo sem dados!!")) == 0){
        puts("======================================");
        puts("Arquivo sem dados!!");
        puts("======================================");
    }else{
        //verifica se o servidor retorno um posto na area
        if(strncmp(resposta.mensagem,"Nenhum posto na area!!",strlen("Nenhum posto na area!!")) == 0){
            puts("======================================");
            puts("Nenhum posto na area!!");
            puts("======================================");
        }else{
            //foi encontrado um posto 
            mostraPosto(resposta.mensagem);
        }
    }
}

//Mostra as informações sobre o posto recebido pelo servidor
void mostraPosto(char *dado){
    char *resposta;
    resposta = (char *)malloc(strlen(dado) * sizeof(char));

    strcpy(resposta,dado);
    //token para receber as partes
    char *token;
    //compara o tipo de combustivel
    const char tipo = resposta[0];
    //delimitador
    const char letra[2] = " ";
    //pega o tipo de combustivel
    token = strtok(resposta,letra);
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
    //pega o preço
    token = strtok(NULL,letra);
    printf("Preço: R$%s\n",token);
    //pega latitude
    token = strtok(NULL,letra);
    printf("Latitude: %s\n",token);
    //pega longitude
    token = strtok(NULL,letra);
    printf("Longitude: %s\n",token);
    //pega a distancia
    token = strtok(NULL,letra);
    printf("Distancia: %sKM\n",token);
    puts("======================================");
    free(resposta);
}

//Calcula o checksum
unsigned char CheckSum(unsigned char *ptr, size_t sz){
    unsigned char chk = 0;
    while (sz-- != 0)
        chk -= *ptr++;
    return chk;
}

//Envia o pacote para o servidor
void enviar(Pacote pacote, int Socket, struct sockaddr_in servidor, socklen_t len){
    if(sendto(Socket,&pacote,sizeof(Pacote),0,(const struct sockaddr *)&servidor,len) < 0){
        perror("Send porta");
        close(Socket);
        exit(1);
    }
}