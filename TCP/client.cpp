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

using namespace std;
using namespace std::chrono;

#define END_TRANSMISSION 0
#define LIST_FILE 1
#define DOWNLOAD_FILE 2
#define DOWNLOAD_ALL_FILES 3
#define ACK 4

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


short chose_option(){
    short opt; 

    cout << "Choose your option:" << endl;
    cout << "[0] - Exit" << endl; 
    cout << "[1] - List of Files" << endl; 
    cout << "[2] - Download one File" << endl; 
    cout << "[3] - Dowload all Files" << endl;

    cin >> opt; 
    while(opt < 0 && opt > 2){
        cout << "Choose between 0 and 2" << endl; 
        cin >> opt; 
    }

    return opt;
}



int main(int argc, char *argv[]){
    int sock_desc;
    struct sockaddr_in sa;
    struct hostent *hp;
    char *host; 

    if(argc != 3){
        cout << "Uso correto: client <Nome Serv> <porta>" << endl;
        exit(1); 
    }

    host = argv[1];

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
    opt = chose_option();

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));

    while(opt != END_TRANSMISSION){

        switch (opt) {
            
            case (LIST_FILE):
                {
                    packet.op = LIST_FILE;

                    cout << "Enviando pedido de Lista" << endl;
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));

                    cout << "*** Listando arquivos ****" << endl << endl; ;

                    while((bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0)) >= 0){
                        cout << packet.buf;
                        memset(&packet, 0, sizeof(struct network_packet));

                        if(packet.more == 0)
                            break;
                    }
                    cout << endl << "**************************" << endl;

                    cout << endl;
                }
                break; 

            case (DOWNLOAD_FILE):
                {
                    char file_name[MAX_ARQ_NAME+1];
                    char destination[MAX_ARQ_NAME+1];

                    cout << "Digite nome do arquivo:" << endl; 
                    cin >> file_name;

                    packet.op = DOWNLOAD_FILE; 
                    packet.bytes = strlen(file_name);
                    strcpy(packet.buf, file_name);
                    
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));

                    memset(destination, 0 , MAX_ARQ_NAME + 1);
                    strcpy(destination, "received/");
                    strcat(destination, file_name);
                    
                    ofstream file;
                    file.open(destination);

                    int packet_count = 0;
                    unsigned int bytes_left; 

                    bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0);
                    bytes_left = packet.bytes;
                    short ack_count = 0;

                    while(bytes_left > 0 ){
                        recv(sock_desc, &packet, sizeof(struct network_packet) , 0);
                        packet_count++;
                        ack_count++;

                        file.write(packet.buf, packet.bytes);

                        bytes_left -= packet.bytes; 

                        if(packet.more == 0)
                            break;
                        
                        memset(&packet, 0, sizeof(struct network_packet));

                        if(ack_count == PACK_ROOF){
                            ack_count  = 0; 
                            packet.op = ACK;
                            send(sock_desc, &packet, sizeof(struct network_packet), 0);
                        }
                    }

                    file.close();

                    cout << "Transmissão Concluida! Arquivo " << file_name << endl;
                    cout << "Salvo em: " << destination << endl;
                    cout << packet_count << " packets received" << endl;
                }
                break;
            
            case (DOWNLOAD_ALL_FILES):
                {
                    packet.op = DOWNLOAD_ALL_FILES; 
                    
                    send(sock_desc, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet));

                    bytes_rec = recv(sock_desc, &packet, sizeof(struct network_packet) , 0);

                    auto list_start = high_resolution_clock::now();
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

                        short ack_count = 0;

                        while(bytes_left > 0 ){
                            recv(sock_desc, &packet, sizeof(struct network_packet) , 0);
                            packet_count++;
                            ack_count++;

                            //cout << "Baixando... " << endl;

                            file.write(packet.buf, packet.bytes);

                            bytes_left -= packet.bytes; 

                            /*if(packet.more == 0){
                                cout << "Sem mais arquivos, encerrando execução \n" << endl;
                                break;
                            }*/
                            
                            memset(&packet, 0, sizeof(struct network_packet));

                            if(ack_count == PACK_ROOF){
                                ack_count  = 0; 
                                packet.op = ACK;
                                cout << "100 pacotes recebidos, enviando ACK!" << endl; 
                                send(sock_desc, &packet, sizeof(struct network_packet), 0);
                            }
                        }

                        file.close();

                        cout << "Transmissão Concluida! Arquivo " << file_name << endl;
                        cout << "Salvo em: " << destination << endl;
                        cout << packet_count << " packets received" << endl;

                        cout << "Enviando ACK: " << file_name << " recebido!" << endl;
                        //Envia ACK do arquivo indicando que pode receber o proximo arquivo 
                        packet.op = ACK; 
                        send(sock_desc, &packet, sizeof(struct network_packet), 0);  


                        recv(sock_desc, &packet, sizeof(struct network_packet) , 0);
                    }
                    auto list_stop = high_resolution_clock::now();
                    auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                    
                    cout << "Todos os arquivos foram recebidos com sucesso!" << endl;
                    cout << "Tempo de transmissão de todos os arquivos: " << list_duration.count() << " ms" << endl;

                }
                break; 
        
        }

        opt = chose_option();
    }
         
    packet.op = END_TRANSMISSION;
    send(sock_desc, &packet, sizeof(struct network_packet), 0);
    close(sock_desc); 

    cout << "Conexão encerrada." << endl;

    return 0;
}    
