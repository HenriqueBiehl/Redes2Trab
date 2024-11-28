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

#define TAM_LINE 5
#define MAX_HOST_NAME 30


#define LIST_FILE 1
#define DOWNLOAD_FILE 2
#define DOWNLOAD_ALL_FILES 3
#define NEXT_FILE 4

#define BUFF_SIZE 2048
#define MAX_ARQ_NAME 1024
#define PACK_ROOF 100

#define END_OF_FILE "EOF"


struct network_packet{
    unsigned short op; 
    unsigned short more;
    unsigned int bytes; 
    char buf[BUFF_SIZE + 1];
};

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

                const char *command = "find server_arqs -type f -printf \"%f %s bytes\n\" > temp";
        
                system(command);

                ifstream file("temp"); 

                if(!file.is_open()){
                    cout << "Erro ao abrir arquivo" << endl;
                }
                
                memset(&packet, 0, sizeof(struct network_packet)); 
                streamsize bytes_read; 
                
                while(!file.eof()){

                    file.read(packet.buf, BUFF_SIZE);
                    bytes_read = file.gcount(); 
                    packet.op = LIST_FILE; 
                    packet.bytes = bytes_read;

                    if(file.eof()){
                        packet.more = 0;
                    }
                    else{
                        packet.more = 1;
                    }

                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                    memset(&packet, 0, sizeof(struct network_packet)); 
                }

                file.close();
                cout << "Lista enviada com sucesso!" << endl;
                system("rm -f temp");
            }
            break;

            case (DOWNLOAD_FILE):
                {
                char arq_name[MAX_ARQ_NAME + 1];
                memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                strcpy(arq_name, "server_arqs/");
                strcat(arq_name, packet.buf);

                ifstream file(arq_name);

                if(!file.is_open()){
                    cout << "Erro ao abrir arquivo" << endl;
                }
                
                memset(&packet, 0, sizeof(struct network_packet));
                streamsize bytes_read;

                unsigned int file_size = filesystem::file_size(arq_name);

                packet.op = DOWNLOAD_FILE;
                packet.bytes = file_size;

                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);

                unsigned int checkpoint_size = file_size / 20;
                unsigned int checkpoint = checkpoint_size;

                cout << file_size << endl;
                cout << checkpoint_size << endl;
                cout << checkpoint << endl;

                auto start = high_resolution_clock::now();
                while(!file.eof()){
                    file.read(packet.buf, BUFF_SIZE);
                    bytes_read = file.gcount();
                    packet.op = DOWNLOAD_FILE;
                    packet.bytes = bytes_read;

                    if(file.eof()){
                        packet.more = 0;
                    }
                    else{
                        packet.more = 1;
                    }

                    // send(sockdescr, &packet, sizeof(struct network_packet), 0);
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                    memset(&packet, 0, sizeof(struct network_packet));

                    packet_count++;
                }
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(stop-start);

                cout << "Arquivo enviado com sucesso!" << endl;
                cout << "Tempo de transmissão: " << duration.count() << " ms" << endl;
                cout << packet_count << " pacotes enviados " << endl;

                break;
                }

            case (DOWNLOAD_ALL_FILES):
                {
                const char *command = "find server_arqs -type f -printf \"%f\n\" > temp";
        
                system(command);

                ifstream list_files("temp"); 

                if(!list_files.is_open()){
                    cout << "Erro ao abrir arquivo" << endl;
                }
                string file_name;
                auto list_start =  high_resolution_clock::now();
                while(getline(list_files, file_name)){
                    
                    cout << "Enviando: " << file_name << endl;

                    char arq_name[MAX_ARQ_NAME + 1];
                    memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                    strcpy(arq_name, "server_arqs/");
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

                    ifstream file(arq_name);
                    
                    if(!list_files.is_open()){
                        cout << "Erro ao abrir arquivo" << endl;
                    }

                    packet_count = 0;
                    cout << "Enviando..." << endl;
                    unsigned int bytes_read;

                    auto start = high_resolution_clock::now();
                    while(!file.eof()){

                        file.read(packet.buf, BUFF_SIZE);
                        bytes_read = file.gcount(); 
                        packet.op = DOWNLOAD_ALL_FILES; 
                        packet.bytes = bytes_read;
                        packet.more = 1;
                        
                        sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
                        memset(&packet, 0, sizeof(struct network_packet)); 

                        packet_count++;
                    }
                    auto stop = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(stop-start);

                    cout << "Arquivo enviado com sucesso!" << endl;
                    cout << "Tempo de transmissão: " << duration.count() << " ms" << endl;
                    cout << packet_count << " pacotes enviados " << endl;

                    // Espera pro proximo arquivo
                    usleep(150);

                    packet.op = NEXT_FILE;
                    sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);

                    file.close();
                }

                packet.more = 0; 
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);

                auto list_stop = high_resolution_clock::now();
                auto list_duration = duration_cast<microseconds>(list_stop-list_start);

                cout << "Todos os arquivos foram enviados com sucesso!" << endl;
                cout << "Tempo de transmissão de toda a lista: " << list_duration.count() << " ms" << endl;

                list_files.close();
                system("rm -f temp");

                }
                break;
        }
	}
    close(sockdescr);

    return 1;
}