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


#define END_TRANSMISSION 0
#define LIST_FILE 1
#define DOWNLOAD_FILE 2
#define DOWNLOAD_ALL_FILES 3
#define ACK 4
#define NACK 5
#define ERROR 6

#define PACK_ROOF 100

#define BUFF_SIZE 2048
#define MAX_ARQ_NAME 1024

#define END_OF_FILE "EOF"


struct network_packet{
    unsigned short op; 
    unsigned short more;
    unsigned int bytes; 
    char buf[BUFF_SIZE + 1];
};

using namespace std;
using namespace std::chrono;

int send_file(int socket, char *file_name, short op_flag){
    struct network_packet packet;
    int packet_count = 0;
    short ack_count = 0;

    ifstream file(file_name); 

    if(!file.is_open()){
        cout << "Erro ao abrir arquivo" << endl;
        return -1;
    }
    
    memset(&packet, 0, sizeof(struct network_packet)); 
    streamsize bytes_read; 
    
    while(!file.eof()){
        file.read(packet.buf, BUFF_SIZE);
        bytes_read = file.gcount(); 
        packet.op = op_flag; 
        packet.bytes = bytes_read;

        if(file.eof() && op_flag != DOWNLOAD_ALL_FILES)
            packet.op = 0; 
        else 
            packet.op = 1;

        send(socket, &packet, sizeof(struct network_packet), 0);

        memset(&packet, 0, sizeof(struct network_packet)); 
        packet_count++;
        ack_count++;

        if(ack_count == PACK_ROOF){
            cout << "100 pacotes enviados, aguardando ACK..." << endl;
            ack_count = 0;
            
            if(recv(socket, &packet, sizeof(struct network_packet) , 0) < 0){
                cout << "Erro ao receber pacote! " << endl; 
                exit(1);
            }                   
            
            if(packet.op == ACK)
                cout << "ACK Recebido!" << endl;
            else 
                return -1;
        }
    }

    file.close();

    return packet_count;
}


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

    while(1){
        i = sizeof(end_local); 

        if((sock_answer = accept(sock_listen, (struct sockaddr*)&end_cliente, &i)) < 0){
             cout << "Não consegui estabelecer a conexão" << endl;
             exit(1);              
        }

        bool flag = true;
        while(flag){

            read(sock_answer, &packet, sizeof(struct network_packet));
            packet_count = 0;
            
            switch(packet.op){

                case (LIST_FILE):
                    {
                        cout << "Recebi pedido de Lista" << endl;

                        const char *command = "find arqs -type f -printf \"%f - %s bytes\n\" > temp";
                        char list[] = "temp";

                        system(command);
                        
                        auto start =  high_resolution_clock::now();
                        packet_count = send_file(sock_answer, list, LIST_FILE);

                        if(packet_count > 0){
                            auto stop = high_resolution_clock::now();
                            auto duration = duration_cast<microseconds>(stop-start);
                            double throughput = (double) filesystem::file_size(list)/ duration.count();
                        
                            cout << endl;
                            cout << "Lista enviada com sucesso!" << endl;
                            cout << "Tempo de transmissão de toda a lista: " << duration.count() << " microssegundos" << endl;
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

                        send(sock_answer, &packet, sizeof(struct network_packet), 0);

                        cout << "Enviando..." << endl;

                        auto start = high_resolution_clock::now();
                        packet_count = send_file(sock_answer, arq_name, DOWNLOAD_FILE);
                        if(packet_count > 0){
                            auto stop = high_resolution_clock::now();
                            auto duration = duration_cast<microseconds>(stop-start);
                            double throughput = file_size/ duration.count();
                            
                            cout << "Arquivo enviado com sucesso!" << endl;
                            cout << "Arquivo enviado com sucesso!" << endl;
                            cout << "Tempo de transmissão: " << duration.count() << " ms" << endl;
                            cout << packet_count << " pacotes enviados " << endl;
                            cout.precision(3);
                            cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 
                            cout << endl;
                        }
                        else{
                            cout << "Falha ao transmitir o arquivo: " << arq_name << endl;
                        }

                        cout << endl << "**************************" << endl;
                    }
                    break;
                case (DOWNLOAD_ALL_FILES):
                    {
                        const char *command = "find arqs -type f -printf \"%f\n\" > temp";
                
                        system(command);

                        ifstream list_files("temp"); 

                        if(!list_files.is_open()){
                            cout << "Erro ao abrir arquivo" << endl;
                            packet.op = ERROR;
                            strcpy(packet.buf, "FALHA AO ABRIR LISTA DE ARQUIVOS\n");
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                            break;
                        }

                        string file_name;
                        double bytes_transmitted = 0;
                        auto list_start =  high_resolution_clock::now();
                        while(getline(list_files, file_name)){
                            
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
                            
                            send(sock_answer, &packet, sizeof(struct network_packet), 0);
                            memset(&packet, 0, sizeof(struct network_packet));

                            packet_count = 0;
                            cout << "Enviando..." << endl;

                            auto start = high_resolution_clock::now();
                            packet_count = send_file(sock_answer, arq_name, DOWNLOAD_FILE);
                            if(packet_count > 0){
                                auto stop = high_resolution_clock::now();
                                auto duration = duration_cast<microseconds>(stop-start);
                                double throughput = (double) file_size/ duration.count(); 
                                
                                cout << endl << "--------------------------" << endl;
                                cout << "Arquivo enviado com sucesso!" << endl;
                                cout << "Tempo de transmissão: " << duration.count() << " microssegundos" << endl;
                                cout << packet_count << " pacotes enviados " << endl;
                                cout.precision(3);
                                cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 

                                bytes_transmitted += file_size;
                            }
                            else{
                                cout << "Falha ao transmitir o arquivo: " << arq_name << endl;
                            }
 
                            //Espera receber Ack para o proximo arquivo do cliente
                            if(recv(sock_answer, &packet, sizeof(struct network_packet) , 0) < 0){
                                cout << "Erro ao receber pacote! " << endl; 
                                exit(1);
                            }                                 
                            
                            if(packet.op == ACK)
                                cout << "ACK Recebido: " << file_name << " foi recebido pelo cliente!" << endl;
                            else {
                                cout << "NACK Recebido: Encerrando trasmissão!" << endl; 
                                break;
                            }
                            cout << endl << "--------------------------" << endl;
                        }

                        packet.more = 0; 
                        send(sock_answer, &packet, sizeof(struct network_packet), 0);

                        auto list_stop = high_resolution_clock::now();
                        auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                        auto throughput_list = bytes_transmitted/list_duration.count();

                        cout << "Todos os arquivos foram enviados com sucesso!" << endl;
                        cout << "Tempo de transmissão de toda a lista: " << list_duration.count() << " microssegundos" << endl;
                        cout.precision(3);
                        cout << "Taxa de Transmissão de " << bytes_transmitted << " :" << throughput_list << " MB/s" << endl; 
                        cout << endl << "--------------------------" << endl;
                        list_files.close();
                        system("rm -f temp");


                    }
                    break; 

                case (END_TRANSMISSION):
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

