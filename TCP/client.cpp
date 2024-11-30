#include <iostream> 
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <string.h>
#include <chrono>
#include <filesystem>

#include "lib_network.hpp"

using namespace std;
using namespace std::chrono;
 
int main(int argc, char *argv[]){
    int sock_desc;
    struct sockaddr_in sa;
    struct hostent *hp;
    char *host; 

    if(argc != 3){
        cout << "Uso correto: client <Ip Serv> <porta>" << endl;
        exit(1); 
    }

    host = argv[1];

    //Abrindo o socket e conectando com o servidor

    if((hp = gethostbyname(host)) == NULL){
        cout << "Não Consegui o endereço de IP do Servidor" << endl;
        exit(1); 
    }

    bcopy((char*)hp->h_addr, (char*)&sa.sin_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(atoi(argv[2]));
    sa.sin_addr.s_addr = INADDR_ANY;

    if( (sock_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        cout << "Não Consegui abrir o Socket" << endl;
        exit(1); 
    }

    if(connect(sock_desc, (struct sockaddr*) &sa, sizeof(sa)) < 0){
        cout << "Não consegui conectar no server" << endl;
        exit(1); 
    }

    short opt; 
    int bytes_rec; 
    unsigned int packet_count;
    opt = chose_option();

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));

    // Loop principal
    while(opt != END_TRANSMISSION){

        switch (opt) {
            
            case (LIST_FILE):
                //Lista de arquivos
                {
                    packet.op = LIST_FILE;

                    //Enviando o pedido
                    cout << "Enviando pedido de Lista" << endl;
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));


                    cout << endl << "*** Listando arquivos ****" << endl << endl; ;
                    
                    // Chama a função para receber a lista
                    packet_count = receive_file_list(sock_desc);
                    if(packet_count < 0)
                        cout << "FALHA AO RECEBER LISTA DE ARQUIVOS" << endl;
                    
                    cout << endl << "**************************" << endl;
                    
                    cout << endl;
                }
                break; 

            case (DOWNLOAD_FILE):
                {
                    char file_name[MAX_ARQ_NAME+1];
                    char destination[MAX_ARQ_NAME+1];
                    cout << endl << "--------------------------" << endl;
                    cout << "Digite nome do arquivo:" << endl; 
                    cin >> file_name;

                    packet.op = DOWNLOAD_FILE; 
                    packet.bytes = strlen(file_name);
                    strcpy(packet.buf, file_name);
                    
                    // Enviando o pedido
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));

                    unsigned int total_bytes; 

                    // Resposta inicial, usada para ver o número de bytes a receber
                    if((bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0)) < 0){
                        cout << "Erro ao receber pacote! " << endl; 
                        exit(1);
                    }

                    if(packet.op == ERROR){
                        cout << packet.buf; 
                        break;
                    }

                    // Cria diretório para o arquivo
                    if(!filesystem::exists("received"))
                        system("mkdir received");

                    memset(destination, 0 , MAX_ARQ_NAME + 1);
                    strcpy(destination, "received/");
                    strcat(destination, file_name);

                    total_bytes = packet.bytes;
                    cout << endl << "Total Bytes: " << total_bytes << endl;

                    // Começa a contar tempo e chama a função para receber o arquivo
                    auto start = high_resolution_clock::now();
                    packet_count = receive_file(sock_desc, destination, total_bytes);
                    if(packet_count > 0){
                        auto stop = high_resolution_clock::now();
                        auto duration = duration_cast<microseconds>(stop-start);
                        auto throughput = filesystem::file_size(destination) / duration.count();
                        
                        // Imprime informações da transmissão
                        cout << "Transmissão Concluida! Arquivo " << file_name << endl;
                        cout << "Salvo em: " << destination << endl;
                        cout << packet_count << " pacotes recebidos!" << endl;
                        cout.precision(6);
                        cout << "Tempo de transmissão: " << duration.count()/1e6 << " segundos" << endl;
                        cout.precision(3);
                        cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 
                        
                    }
                    else{
                        cout << "Falha ao receber o arquivo: " << destination << endl;
                    }
                    
                    cout << endl << "--------------------------" << endl;
                  
                }
                break;
            
            case (DOWNLOAD_ALL_FILES):
                {
                    packet.op = DOWNLOAD_ALL_FILES; 
                    
                    // Envia pedido inicial
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));

                    // Resposta inicial, usada para ver o número de bytes a receber do primeiro arquivo junto com seu nome
                    if((bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0)) < 0){
                        cout << "Erro ao receber pacote! " << endl; 
                        exit(1);
                    };

                    if(packet.op == ERROR){
                        cout << packet.buf; 
                        break;
                    }

                    // Cria diretório para o arquivo
                    if(!filesystem::exists("received"))
                        system("mkdir received");

                    unsigned int bytes_received = 0;
                    auto list_start = high_resolution_clock::now();

                    // Recebe até o servidor notificar que terminou de enviar
                    while(packet.more != 0){ 
                        
                        char file_name[MAX_ARQ_NAME+1];
                        char destination[MAX_ARQ_NAME+1];

                        strcpy(destination, "received/");
                        strcpy(file_name, packet.buf);
                        strcat(destination, packet.buf);
                    
                        int packet_count = 0;
                        unsigned int total_bytes = packet.bytes;

                        cout << endl;
                        cout << "Baixando: " << file_name << endl;
                        cout << "Tamanho: " << packet.bytes << endl;

                        // Começa a contar tempo e chama a função para receber o arquivo
                        auto start = high_resolution_clock::now();
                        packet_count = receive_file(sock_desc, destination, total_bytes);
                        if(packet_count > 0){
                            auto stop = high_resolution_clock::now();
                            auto duration = duration_cast<microseconds>(stop-start);
                            auto throughput = filesystem::file_size(destination) / duration.count();

                            // Imprime informações da transmissão
                            cout << "Transmissão Concluida! Arquivo " << file_name << endl;
                            cout << "Salvo em: " << destination << endl;
                            cout << packet_count << " packets received" << endl;
                            cout.precision(6);
                            cout << "Tempo de transmissão: " << duration.count()/1e6 << " segundos" << endl;
                            cout.precision(3);
                            cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 

                            bytes_received += filesystem::file_size(destination);

                            cout << endl << "Enviando ACK: " << file_name << " recebido!" << endl;
                            //Envia ACK do arquivo indicando que pode receber o proximo arquivo 
                            packet.op = ACK; 
                        }
                        else{
                            cout << "Falha ao receber o arquivo: " << destination << endl;
                            cout << "Enviando NACK!" << endl;
                            packet.op = NACK;
                        }

                        cout << endl << "--------------------------" << endl;

                        send(sock_desc, &packet, sizeof(struct network_packet), 0);  

                        // Recebe o nome e tamanho do próximo arquivo
                        if((bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0)) < 0){
                            cout << "Erro ao receber pacote! " << endl; 
                            exit(1);
                        }
                    }
                    auto list_stop = high_resolution_clock::now();
                    auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                    auto list_throughput = bytes_received / list_duration.count();
                    
                    // Imprime informações de todas as transmissões
                    cout << "Todos os arquivos foram recebidos com sucesso!" << endl;
                    cout.precision(6);
                    cout << "Tempo de transmissão de todos os arquivos: " << list_duration.count()/1e6 << " segundos" << endl;
                    cout.precision(3);
                    cout << "Taxa de Transmissão: " << list_throughput << " MB/s" << endl; 
                    
                    cout << endl << "**************************" << endl;


                }
                break; 
        
        }

        opt = chose_option();
    }
         
    // No fim da transmissão, envia uma mensagem final para o servidor de modo a encerrar a conexão
    packet.op = END_TRANSMISSION;
    send(sock_desc, &packet, sizeof(struct network_packet), 0);
    close(sock_desc); 

    cout << "Conexão encerrada." << endl;

    return 0;
}    
