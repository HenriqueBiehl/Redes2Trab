#include <iostream> 
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "lib_network.hpp"

using namespace std;
using namespace std::chrono;


int main(int argc, char *argv[]){
    int sockdescr;
    struct sockaddr_in sa;
    struct hostent *hp;
    char *host;
    unsigned int i;

    if(argc != 3 && argc != 4){
        cout << "Uso correto: client <Nome Serv> <porta> <flag-opcional -c>" << endl;
        exit(1); 
    }

    host = argv[1];

    // Abrindo o socket e obtendo o endereço do servidor
    if((hp = gethostbyname(host)) == NULL){
        cout << "Não Consegui o endereço de IP do Servidor" << endl;
        exit(1);
    }

    bcopy((char*)hp->h_addr, (char*)&sa.sin_addr, hp->h_length);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(argv[2]));
    sa.sin_addr.s_addr = INADDR_ANY;

    if((sockdescr = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        cout << "Não Consegui abrir o Socket" << endl;
        exit(1);
    }

    short opt;
    opt = chose_option();

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));

    // Loop principal
    while(opt != 0){
        switch (opt) {
            case (LIST_FILE):
                packet.op = LIST_FILE;

                // Enviando o pedido
                cout << "Enviando pedido de Lista" << endl;
                // send(sockdescr, &packet, sizeof(struct network_packet), 0);
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                memset(&packet, 0, sizeof(struct network_packet));

                cout << "*** Listando arquivos ****" << endl << endl;

                // Chama a função para receber a lista de arquivos
                if(!receive_file_list(sockdescr, sa, i))
                    cout << "FALHA AO RECEBER LISTA DE ARQUIVOS" << endl;

                cout << endl << "**************************" << endl;

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
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                    memset(&packet, 0, sizeof(struct network_packet));
                    
                    memset(destination, 0 , MAX_ARQ_NAME + 1);
                    strcpy(destination, "received/");
                    strcat(destination, file_name);

                    int packet_count = 0;
                    unsigned int bytes_left;

                    // Resposta inicial, usada para ver o número de bytes a receber
                    recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                    bytes_left = packet.bytes;

                    cout << "Total Bytes: " << packet.bytes << endl;
                
                    // Cria diretório para o arquivo
                    if(!filesystem::exists("received"))
                        system("mkdir received");

                    // Começa a contar tempo e chama a função para receber o arquivo
                    auto start = high_resolution_clock::now();
                    packet_count = receive_file(bytes_left, destination , sockdescr, sa, i);

                    if (packet_count < 0) {
                        cout << "Falha ao receber o arquivo: " << destination << endl;
                    }
                    else {
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

                    // -c flag chama a função para comparar o arquivo original com o recebido(se o original existir)
                    if (argc == 4) {
                        if (strcmp(argv[3], "-c") == 0) {
                            compare_file(file_name);
                        }
                    }
                    cout << endl << "--------------------------" << endl;

                }
                break;

            case (DOWNLOAD_ALL_FILES):
                {
                    packet.op = DOWNLOAD_ALL_FILES; 
                    
                    // Envia pedido inicial
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                    memset(&packet, 0, sizeof(struct network_packet));

                    // Resposta inicial, usada para ver o número de bytes a receber do primeiro arquivo junto com seu nome
                    recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);

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
                        
                        ofstream file;
                        file.open(destination);

                        int packet_count = 0;
                        unsigned int bytes_left = packet.bytes;

                        cout << "Baixando: " << file_name << endl;
                        cout << "Tamanho: " << packet.bytes << endl;

                        // Começa a contar tempo e chama a função para receber o arquivo
                        auto start = high_resolution_clock::now();

                        // Recebe pacotes até o servidor informar que o próximo arquivo será enviado, ou que a operação encerrou
                        while(1){
                            recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                            packet_count++;

                            if (packet.op == NEXT_FILE || packet.more == 0) {
                                break;
                            }

                            // Escreve as informações recebidas em um arquivo de destino
                            file.write(packet.buf, packet.bytes);

                            bytes_left -= packet.bytes; 
                            
                            memset(&packet, 0, sizeof(struct network_packet));
                        }

                        file.close();

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
                        cout << endl << "--------------------------" << endl;

                        bytes_received += filesystem::file_size(destination);

                        recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                    }

                    // Imprime informações das transmissões
                    auto list_stop = high_resolution_clock::now();
                    auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                    auto list_throughput = bytes_received / list_duration.count();
                    
                    cout << "Todos os arquivos foram recebidos com sucesso!" << endl;
                    cout.precision(6);
                    cout << "Tempo de transmissão de todos os arquivos: " << list_duration.count()/1e6 << " segundos" << endl;
                    cout.precision(3);
                    cout << "Taxa de Transmissão: " << list_throughput << " MB/s" << endl; 
                    
                    // -c flag chama a função para comparar os arquivo originais com os recebidos(se os originais existirem)
                    if (argc == 4) {
                        if (strcmp(argv[3], "-c") == 0) {
                            cout << endl;

                            cout << endl << "-- Erros de Transmissão --" << endl;
       
                            usleep(300000);

                            // Obtem a lista de arquivos e faz a comparação deles, um por um
                            const char *command = "find arqs -type f -printf \"%f\n\" > temp";
                            system(command);

                            ifstream files("temp");
                            string filename;

                            while (getline(files, filename)) {
                                compare_file(filename.c_str());
                            }
                            system("rm -f temp");
                        }
                    }
                    
                    cout << endl << "**************************" << endl;

                }
                break;
        }

        // No fim de uma operação, pede para o usuário iniciar outra
        opt = chose_option();

    }
    close(sockdescr); 

    return 0;
}
