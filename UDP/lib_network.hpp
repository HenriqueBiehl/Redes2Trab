#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h> 
#include <chrono>
#include <filesystem>

#define END_TRANSMISSION 0          //Flag indicando fim de transsmissão
#define LIST_FILE 1                 //Flag indicando operação de lista de arquivos
#define DOWNLOAD_FILE 2             //Flag indicando operação de download
#define DOWNLOAD_ALL_FILES 3        //Flag indicando operação de baixar todos os arquivos
#define NEXT_FILE 4                 //Flag indicando pedido para leitura do próximo arquivo para Download all files

#define PACK_ROOF 100               //Limite de pacotes para ser recebido antes de esperar ack

#define BUFF_SIZE 2048              //Tamanho do campo buf do pacote de rede
#define MAX_ARQ_NAME 1024           //Definir o tamanho maximo de um nome de arquivo


//Estrutura de dados para representar um pacote de rede
struct network_packet{
    unsigned short op;          //Campo que indica operação de Rede
    unsigned short more;        //Campo usado para indicar se há mais pacotes ou arquivos sendo transmitidos
    unsigned int bytes;         //Campo usado para indicar tamanho de arquivos ou de bytes transmitidos em buf
    char buf[BUFF_SIZE + 1];   //Campo buf contendo os dados sendo transmitidos
};

/* 
    Função para escolher opção de operação da Rede:
    0 - para sair
    1 - para listar arquivos
    2 - Baixar 1 arquivo
    3 - Baixar todos os arquivos
*/
short chose_option();

/* 
    Função para Receber uma lista de arquivos enviada do servidor e imprimir na tela. 
    Recebe o socket de transmissao. 
    Retorna o numero de pacotes recebidos. Retorna -1 em caso de erro. 
*/
int receive_file_list(int sockdescr, struct sockaddr_in sa, unsigned int i);
/*
    Função para Receber um arquivo do servidor e salvar no arquivo destination.
    Recebe o numero de bytes do arquivo em bytes_left para leitura, o nome do arquivo arq_name e a 
    flag de operação op_flag, o socket do servidor sa e o tamanho do endereço i.
    Retorna o numero de pacotes recebidos. Retorna -1 em caso de erro. 
*/
int receive_file(int bytes_left, char* destination, int sockdescr, sockaddr_in sa, unsigned int i);

/*
    Função para Enviar um arquivo para o cliente.
    Recebe o socket de transmissao, o nome do arquivo arq_name e a flag de operação op_flag, o socket do cliente e o tamanho do endereço i.
    Retorna o numero de pacotes enviados. Retorna -1 em caso de erro. 
*/
int send_file(int sockdescr, char *arq_name, short op_flag, struct sockaddr_in client, int i);

/*
    Função para comparar o arquivo file_name recebido na transmissão com sua versão dos arquivos do servidor.
    Imprime o número o tamanho dos arquivo recebido e original, a  porcentagem de bytes recebidos e a quantidade de bytes que chegaram errado.
*/
void compare_file(const char* filename);