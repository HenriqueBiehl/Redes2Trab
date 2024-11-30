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

#include "lib_network.hpp"

#define TAM_LINE 5
#define MAX_HOST_NAME 30

using namespace std;
using namespace std::chrono;


int main(int argc, char *argv[]){
    int sock_listen, sock_answer;
    unsigned int i; 
    struct sockaddr_in end_local;
    struct sockaddr_in end_cliente;
    struct hostent *hp;
    char localhost[MAX_HOST_NAME+1];

    if(argc != 2){
        cout << "Uso correto: Servidor <porta>" << endl;
        exit(1);
    }

    // Abrindo socket para escutar requisições

    gethostname(localhost, MAX_HOST_NAME);

    if((hp = gethostbyname(localhost)) == NULL){
        cout << "Não consegui o proprio IP" << endl;
        exit(1);
    }

    end_local.sin_port = htons(atoi(argv[1]));
    bcopy((char *)hp->h_addr, (char *)&end_local.sin_addr,hp->h_length);

    end_local.sin_family = AF_INET;
    end_local.sin_addr.s_addr = INADDR_ANY;


    if((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        cout << "Falha ao abrir o socket" << endl;
        exit(1);       
    }

    if(bind(sock_listen, (struct sockaddr*)&end_local, sizeof(end_local)) < 0){
        cout << "Não foi possivel fazer o bind" << endl;
        exit(1);               
    }
    
    listen(sock_listen, TAM_LINE);

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));
    int packet_count;

    // Loop principal, espera requisições
    while(1){
        i = sizeof(end_local); 

        // Aceita pedido de conexão
        if((sock_answer = accept(sock_listen, (struct sockaddr*)&end_cliente, &i)) < 0){
             cout << "Não consegui estabelecer a conexão" << endl;
             exit(1);              
        }

        bool flag = true;
        // Enquanto a conexão estiver ativa, interage com o cliente
        while(flag){

            read(sock_answer, &packet, sizeof(struct network_packet));
            packet_count = 0;
            
            switch(packet.op){

                case (LIST_FILE):
                    {
                        cout << "Recebi pedido de Lista" << endl;

                        // Obtém lista e insere em um arquivo temporário
                        const char *command = "find arqs -type f -printf \"%f - %s bytes\n\" > temp";
                        char list[] = "temp";

                        system(command);
                        
                        // Chama função para enviar o arquivo temporário ao cliente
                        auto start =  high_resolution_clock::now();
                        packet_count = send_file(sock_answer, list, LIST_FILE);

                        if(packet_count > 0){
                            auto stop = high_resolution_clock::now();
                            auto duration = duration_cast<microseconds>(stop-start);
                            double throughput = (double) filesystem::file_size(list)/ duration.count();
                        
                            // Imprime informações da transmissão
                            cout << endl;
                            cout << "Lista enviada com sucesso!" << endl;
                            cout.precision(6);
                            cout << "Tempo de transmissão: " << duration.count()/1e6 << " segundos" << endl;                            
                            cout << packet_count << " pacotes enviados " << endl;
                            cout.precision(3);
                            cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 
                        }
                        else{
                            cout << "Falha ao transmitir a lista de arquivos!" << endl;
                            packet.op = ERROR;
                            strcpy(packet.buf, "FALHA AO TRANSMITIR LISTA ARQUIVOS\n");
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                        }
                        cout << endl << "**************************" << endl;

                        // Remove o arquivo temporário
                        system("rm -f temp");
                    }
                    break;

                case(DOWNLOAD_FILE):
                    {
                        char arq_name[MAX_ARQ_NAME + 1];
                        memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                        strcpy(arq_name, "arqs/");
                        strcat(arq_name, packet.buf);
                        
                        memset(&packet, 0, sizeof(struct network_packet)); 
                        
                        // Envia mensagem de erro se o arquivo requisitado não existe
                        if(!filesystem::exists(arq_name)){
                            cout << "Erro! Arquivo: " << arq_name << " inexistente!" << endl;
                            packet.op = ERROR;
                            strcpy(packet.buf, "ARQUIVO INEXISTENTE\n");
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                            break; 
                        }

                        unsigned int file_size = filesystem::file_size(arq_name);

                        packet.op = DOWNLOAD_FILE;
                        packet.bytes = file_size;

                        // Envia tamanho do arquivo em bytes
                        send(sock_answer, &packet, sizeof(struct network_packet), 0);

                        cout << "Enviando..." << endl;

                        // Chama a função para enviar o arquivo requisitado
                        auto start = high_resolution_clock::now();
                        packet_count = send_file(sock_answer, arq_name, DOWNLOAD_FILE);
                        if(packet_count > 0){
                            auto stop = high_resolution_clock::now();
                            auto duration = duration_cast<microseconds>(stop-start);
                            double throughput = file_size/ duration.count();

                            // Imprime informações da transmissão
                            cout << endl << "** Relatório de Transmissão **" << endl;

                            cout << "Arquivo " << arq_name << " enviado com sucesso!" << endl;
                            cout << "Arquivo enviado com sucesso!" << endl;
                            cout.precision(6);
                            cout << "Tempo de transmissão: " << duration.count()/1e6 << " segundos" << endl;                            
                            cout << packet_count << " pacotes enviados " << endl;
                            cout.precision(3);
                            cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 
                            cout << endl;
                        }
                        else{
                            cout << "Falha ao transmitir o arquivo: " << arq_name << endl;
                        }

                        cout << endl << "******************************" << endl;
                    }
                    break;
                case (DOWNLOAD_ALL_FILES):
                    {
                        // Obtém lista e insere em um arquivo temporário
                        const char *command = "find arqs -type f -printf \"%f\n\" > temp";
                        
                        system(command);

                        ifstream list_files("temp"); 

                        // Envia mensagem de erros se não conseguiu obter a lista
                        if(!list_files.is_open()){
                            cout << "Erro ao abrir arquivo" << endl;
                            packet.op = ERROR;
                            strcpy(packet.buf, "FALHA AO ABRIR LISTA DE ARQUIVOS\n");
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                            break;
                        }

                        string file_name;
                        double bytes_transmitted = 0;
                        bool success;

                        // Inicia o processo de enviar todos os arquivos
                        auto list_start =  high_resolution_clock::now();
                        while(getline(list_files, file_name)){
                            success = false;

                            cout << "Enviando: " << file_name << endl;

                            // Obtém o nome do arquivo atual e prepara o pacote inicial com seu tamanho
                            char arq_name[MAX_ARQ_NAME + 1];
                            memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                            strcpy(arq_name, "arqs/");
                            memset(&packet, 0, sizeof(struct network_packet));

                            file_name.copy(packet.buf, file_name.size());

                            strcat(arq_name, packet.buf);

                            unsigned int file_size = filesystem::file_size(arq_name);

                            packet.op = DOWNLOAD_ALL_FILES;
                            packet.bytes = file_size; 
                            packet.more = 1;
                            file_name.copy(packet.buf, file_name.size());
                            
                            // Envia o pacote inicial com o nome e tamanho do arquivo
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                            memset(&packet, 0, sizeof(struct network_packet));

                            packet_count = 0;
                            cout << "Enviando..." << endl;

                            // Chama a função para enviar o arquivo atual
                            auto start = high_resolution_clock::now();
                            packet_count = send_file(sock_answer, arq_name, DOWNLOAD_FILE);
                            if(packet_count > 0){
                                auto stop = high_resolution_clock::now();
                                auto duration = duration_cast<microseconds>(stop-start);
                                double throughput = (double) file_size/ duration.count(); 

                                // Imprime informações da transmissão                                
                                cout << endl << "-- Relatório de Transmissão --" << endl;

                                cout << "Arquivo enviado com sucesso!" << endl;
                                cout.precision(6);
                                cout << "Tempo de transmissão: " << duration.count()/1e6 << " segundos" << endl;                                
                                cout << packet_count << " pacotes enviados " << endl;
                                cout.precision(3);
                                cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 

                                bytes_transmitted += file_size;
                                success = true;
                            }
                            else{
                                cout << "ERRO: Falha ao transmitir o arquivo: " << arq_name << endl;
                            }
 
                            //Espera receber Ack para o proximo arquivo do cliente
                            if(recv(sock_answer, &packet, sizeof(struct network_packet) , 0) < 0){
                                cout << "ERRO ao receber pacote! " << endl; 
                                exit(1);
                            }                                 
                            
                            if(packet.op == ACK)
                                cout << "ACK Recebido: " << file_name << " foi recebido pelo cliente!" << endl;
                            else {
                                cout << "NACK Recebido: Encerrando trasmissão!" << endl; 
                                success = false;
                                break;
                            }
                            cout << endl << "------------------------------" << endl;
                        }

                        // Ao enviar todos os arquivos, notifica o cliente que a transmissão terminou
                        packet.more = 0; 
                        send(sock_answer, &packet, sizeof(struct network_packet), 0);

                        if(success){
                            auto list_stop = high_resolution_clock::now();
                            auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                            auto throughput_list = bytes_transmitted/list_duration.count();

                            // Imprime informações das transmissões
                            cout << endl << "-- Relatório de Transmissão Final --" << endl;

                            cout << "Todos os arquivos foram enviados com sucesso!" << endl;
                            cout.precision(6);
                            cout << "Tempo de transmissão: " << list_duration.count()/1e6 << " segundos" << endl;                            
                            cout.precision(4);
                            cout << "Taxa de Transmissão de " << bytes_transmitted << " bytes: " << throughput_list << " MB/s" << endl; 
                        }
                        else{
                            cout << "ERRO: Falha em transmitir todos os arquivos!" << endl;
                        }

                        cout << endl << "------------------------------------" << endl;
                        list_files.close();

                        // Remove o arquivo temporário
                        system("rm -f temp");


                    }
                    break; 

                case (END_TRANSMISSION):
                    // Passa a esperar uma nova requisição quando o cliente encerra a conexão
                    {
                        cout << "Conexão Encerrada." << endl;
                        flag = false;
                    }
                    break;
            }

            memset(&packet, 0, sizeof(struct network_packet));
        }
        
        cout << "Socket fechado." << endl;
        close(sock_answer);
    }
    
    close(sock_answer);

    return 1;
}

