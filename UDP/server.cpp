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
    int sockdescr;
    unsigned int i;
	struct sockaddr_in local, client;  /* local: servidor, client: cliente */
	struct hostent *hp;
	char localhost [MAX_HOST_NAME];

    if (argc != 2) {
        cout << "Uso correto: Servidor <porta>" << endl;
        exit(1);
    }

	gethostname(localhost, MAX_HOST_NAME);

	if ((hp = gethostbyname(localhost)) == NULL){
		cout << "Não consegui meu proprio IP" << endl;
		exit (1);
	}	
	
	local.sin_port = htons(atoi(argv[1]));
	bcopy((char *)hp->h_addr, (char *)&local.sin_addr, hp->h_length);

	local.sin_family = AF_INET;		
    local.sin_addr.s_addr = INADDR_ANY;

	if ((sockdescr = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        cout << "Nao consegui abrir o socket" << endl;
        exit(1);
	}

	if (bind(sockdescr, (struct sockaddr *)&local, sizeof(local)) < 0){
		cout << "Não consegui fazer o bind" << endl;
        exit(1);
	}

    struct network_packet packet;

    int packet_count;

    while (1) {
        i = sizeof(client);
        packet_count = 0;

        recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, &i);

        switch(packet.op) {
            case (LIST_FILE):
                {
                    cout << "Recebi pedido de Lista" << endl;

                    const char *command = "find arqs -type f -printf \"%f %s bytes\n\" > temp";
            
                    system(command);

                    char temp[] = "temp";

                    auto start =  high_resolution_clock::now();
                    packet_count = send_file(sockdescr, temp, LIST_FILE, client, i);

                    if (packet_count > 0) {
                        auto stop = high_resolution_clock::now();
                        auto duration = duration_cast<microseconds>(stop-start);
                        double throughput = (double) filesystem::file_size(temp)/ duration.count();

                        cout << endl;
                        cout << "Lista enviada com sucesso!" << endl;
                        cout << "Tempo de transmissão de toda a lista: " << duration.count() << " microssegundos" << endl;
                        cout << packet_count << " pacotes enviados " << endl;
                        cout.precision(3);
                        cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl;
                    }
                    else
                        cout << "Falha ao transmitir a lista de arquivos!";

                    system("rm -f temp");
                }
                break;

            case (DOWNLOAD_FILE):
                {
                    char arq_name[MAX_ARQ_NAME + 1];
                    memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                    strcpy(arq_name, "arqs/");
                    strcat(arq_name, packet.buf);

                    unsigned int file_size = filesystem::file_size(arq_name);

                    packet.op = DOWNLOAD_FILE;
                    packet.bytes = file_size;

                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                    
                    cout << "Enviando..." << endl;

                    auto start = high_resolution_clock::now();
                    packet_count = send_file(sockdescr, arq_name, DOWNLOAD_FILE, client, i);

                    if(packet_count > 0){
                        auto stop = high_resolution_clock::now();
                        auto duration = duration_cast<microseconds>(stop-start);
                        double throughput = file_size/ duration.count();
                        
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
                    else {
                        cout << "Falha ao transmitir o arquivo: " << arq_name << endl;
                    }

                    cout << endl << "******************************" << endl;

                    break;
                }

            case (DOWNLOAD_ALL_FILES):
                {
                const char *command = "find arqs -type f -printf \"%f\n\" > temp";

                system(command);

                ifstream list_files("temp"); 

                if(!list_files.is_open()){
                    cout << "Erro ao abrir arquivo" << endl;
                }

                bool success;
                string file_name;
                double bytes_transmitted = 0;
                auto list_start =  high_resolution_clock::now();
                while(getline(list_files, file_name)){
                    success = false;
                    cout << "Enviando: " << file_name << endl;

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
                    
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                    memset(&packet, 0, sizeof(struct network_packet));

                    packet_count = 0;
                    cout << "Enviando..." << endl;

                    auto start = high_resolution_clock::now();

                    packet_count = send_file(sockdescr, arq_name, DOWNLOAD_ALL_FILES, client, i);

                    if(packet_count > 0){
                        auto stop = high_resolution_clock::now();
                        auto duration = duration_cast<microseconds>(stop-start);
                        double throughput = (double) file_size/ duration.count(); 
                        
                        cout << endl << "-- Relatório de Transmissão --" << endl;

                        cout << "Arquivo enviado com sucesso!" << endl;
                        cout << "Tempo de transmissão: " << duration.count() << " microssegundos" << endl;
                        cout << packet_count << " pacotes enviados " << endl;
                        cout.precision(3);
                        cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 

                        bytes_transmitted += file_size;
                        success = true;
                    }
                    else{
                        cout << "ERRO: Falha ao transmitir o arquivo: " << arq_name << endl;
                    }

                    // Espera pro proximo arquivo
                    usleep(6000);

                    packet.op = NEXT_FILE;
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                }

                packet.more = 0; 
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);

                if(success){
                    auto list_stop = high_resolution_clock::now();
                    auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                    auto throughput_list = bytes_transmitted/list_duration.count();

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
                system("rm -f temp");

                }
                break;
        }
	}
    close(sockdescr);

    return 1;
}