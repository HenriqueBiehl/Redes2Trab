#include <iostream> 
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

using namespace std;

#define LIST_FILE 1
#define DOWNLOAD_FILE 2

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
    int sockdescr;
    struct sockaddr_in sa;
    struct hostent *hp;
    char *host;
    unsigned int i;

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
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(argv[2]));
    sa.sin_addr.s_addr = INADDR_ANY;

    if((sockdescr = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        cout << "Não Consegui abrir o Socket" << endl;
        exit(1);
    }

    short opt;
    int bytes_rec;
    opt = chose_option();

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));

    while(opt != 0){
        switch (opt) {
            case (LIST_FILE):
                packet.op = LIST_FILE;

                cout << "Enviando pedido de Lista" << endl;
                // send(sockdescr, &packet, sizeof(struct network_packet), 0);
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                memset(&packet, 0, sizeof(struct network_packet));

                cout << "*** Listando arquivos ****" << endl << endl;

                while((bytes_rec = recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i)) >= 0){
                    cout << packet.buf;
                    memset(&packet, 0, sizeof(struct network_packet));

                    if(packet.more == 0)
                        break;
                }
                cout << endl << "**************************" << endl;

                break;

            case (DOWNLOAD_FILE):
                char name[MAX_ARQ_NAME+1];
                char newfile[MAX_ARQ_NAME+1];

                cout << "Digite nome do arquivo:" << endl;
                cin >> name;

                packet.op = DOWNLOAD_FILE;
                packet.bytes = strlen(name);
                strcpy(packet.buf, name);

                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                memset(&packet, 0, sizeof(struct network_packet));

                memset(newfile, 0 , MAX_ARQ_NAME + 1);
                strcpy(newfile, "client_arqs/");
                strcat(newfile, name);
                
                ofstream file;
                file.open(newfile);

                int packet_count = 0;
                unsigned int bytes_left;

                bytes_rec = recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                bytes_left = packet.bytes;

                cout << "packet = " << packet.bytes << endl;

                while(bytes_left > 0 ){
                    packet_count++;
                    recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);

                    file.write(packet.buf, packet.bytes);

                    bytes_left -= packet.bytes; 

                    if(packet.more == 0)
                        break;
                    
                    memset(&packet, 0, sizeof(struct network_packet));
                }

                file.close();

                cout << "Transmissão Concluida! Arquivo " << name << "salvo em: " << newfile << endl;
                cout << packet_count << " packets received" << endl;
                break;  
        }

        opt = chose_option();

    }
    close(sockdescr); 

    return 0;
}
