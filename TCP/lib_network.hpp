#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <string.h>

#pragma once

#define END_TRANSMISSION 0          //Flag indicando fim de transsmissão
#define LIST_FILE 1                 //Flag indicando operação de lista de arquivos
#define DOWNLOAD_FILE 2             //Flag indicando operação de download
#define DOWNLOAD_ALL_FILES 3        //Flag indicando operação de baixar todos os arquivos
#define ACK 4                       //Flag indicando ack
#define NACK 5                      //Flag indicando nack para erros de leitura de arquivos
#define ERROR 6                     //Flag indicando erro


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
int receive_file_list(int socket);

/*
    Função para Receber um arquivo do servidor e salvar no arquivo destination.
    Recebe o socket de transmissao e o total de bytes do arquivo que será enviado. 
    Retorna o numero de pacotes recebidos. Retorna -1 em caso de erro. 
*/
int receive_file(int socket, char *destination, int total_bytes);

/*
    Função para Enviar um arquivo paa o cliente.
    Recebe o socket de transmissao, o nome do arquivo file_name e a flag de operação op_flag.
    Retorna o numero de pacotes enviados. Retorna -1 em caso de erro. 
*/
int send_file(int socket, char *file_name, short op_flag);
