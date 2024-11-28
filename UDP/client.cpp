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

using namespace std;
using namespace std::chrono;

#define LIST_FILE 1
#define DOWNLOAD_FILE 2
#define DOWNLOAD_ALL_FILES 3
#define NEXT_FILE 5

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

int receive_file_list(int sockdescr, struct sockaddr_in sa, unsigned int i) {
    int bytes_rec;
    struct network_packet packet;
    int packet_count = 0;

    while((bytes_rec = recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i)) >= 0){
        packet_count++;
        cout << packet.buf;
        memset(&packet, 0, sizeof(struct network_packet));

        if(packet.more == 0 || packet.op == NEXT_FILE)
            break;
    }
    return packet_count;
}

int receive_file(int bytes_left, char* newfile, int sockdescr, sockaddr_in sa, unsigned int i) {
    struct network_packet packet;
    int packet_count = 0;

    ofstream file;
    file.open(newfile);

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
    return packet_count++;
}

void compare_file(const char* filename) {
    cout << "No arquivo " << filename << ":" << endl;
    char original_name[MAX_ARQ_NAME+1];
    char received_name[MAX_ARQ_NAME+1];

    memset(original_name, 0 , MAX_ARQ_NAME + 1);
    strcpy(original_name, "server_arqs/");
    strcat(original_name, filename);

    memset(received_name, 0 , MAX_ARQ_NAME + 1);
    strcpy(received_name, "client_arqs/");
    strcat(received_name, filename);

    ifstream original;
    ifstream received;

    original.open(original_name);
    received.open(received_name);

    std::filesystem::path p_original{original_name};
    std::filesystem::path p_received{received_name};

    if (!received.fail()) {
        long int original_size = std::filesystem::file_size(p_original);
        long int received_size = std::filesystem::file_size(p_received);
        long int diff = original_size - received_size;
        int percentage = 100 * received_size / original_size;

        char c1[2], c2[2];
        int diff_bytes = 0;

        cout << "Um total de " << diff << " bytes não chegaram, sendo que cerca de " << percentage << "\% dos bytes foram transmitidos" << endl;

        while (!original.eof() && !received.eof()) {
            original.read(c1, 1);
            received.read(c2, 1);
            // cout << "c1 = " << c1[0] << " c2 = " << c2[0] << endl;
            if (c1[0] != c2[0]) {
                diff_bytes++;
            }
        }

        int percentage_diff = 100 * diff_bytes / received_size;

        cout << "Um total de " << diff_bytes << " bytes chegaram errado, sendo que cerca de "
            << percentage_diff << "\% dos bytes chegaram com algum problema(Ex. Ordem errada, valor incorreto, etc.)"
            << endl;
        cout << endl;
    }
}

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

    while(opt != 0){
        switch (opt) {
            case (LIST_FILE):
                packet.op = LIST_FILE;

                cout << "Enviando pedido de Lista" << endl;
                // send(sockdescr, &packet, sizeof(struct network_packet), 0);
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                memset(&packet, 0, sizeof(struct network_packet));

                cout << "*** Listando arquivos ****" << endl << endl;

                if(!receive_file_list(sockdescr, sa, i))
                    cout << "FALHA AO RECEBER LISTA DE ARQUIVOS" << endl;

                cout << endl << "**************************" << endl;

                break;

            case (DOWNLOAD_FILE):
            {
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

                int packet_count = 0;
                unsigned int bytes_left;

                recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                bytes_left = packet.bytes;

                cout << "Total Bytes: " << packet.bytes << endl;

                auto start = high_resolution_clock::now();
                packet_count = receive_file(bytes_left, newfile, sockdescr, sa, i);

                if (packet_count < 0) {
                    cout << "Falha ao receber o arquivo: " << newfile << endl;
                }
                else {
                    auto stop = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(stop-start);
                    auto throughput = filesystem::file_size(newfile) / duration.count();
                    
                    cout << "Transmissão Concluida! Arquivo " << name << endl;
                    cout << "Salvo em: " << newfile << endl;
                    cout << packet_count << " pacotes recebidos!" << endl;
                    cout << "Tempo de transmissão: " << duration.count() << " microssegundos" << endl;
                    cout.precision(3);
                    cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 
                }

                cout << endl << "--------------------------" << endl;

                // -c flag compara o arquivo original com o recebido(se o original existir)
                if (argc == 4) {
                    if (strcmp(argv[3], "-c") == 0) {
                        compare_file(name);
                        cout << endl << "--------------------------" << endl;
                    }
                }
            }
            break;

            case (DOWNLOAD_ALL_FILES):
                {
                packet.op = DOWNLOAD_ALL_FILES; 
                
                sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, sizeof(sa));
                memset(&packet, 0, sizeof(struct network_packet));

                recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);

                unsigned int bytes_received = 0;
                auto list_start = high_resolution_clock::now();
                while(packet.more != 0){ 
                    
                    char name[MAX_ARQ_NAME+1];
                    char newfile[MAX_ARQ_NAME+1];

                    strcpy(newfile, "client_arqs/");
                    strcpy(name, packet.buf);
                    strcat(newfile, packet.buf);
                    
                    ofstream file;
                    file.open(newfile);

                    int packet_count = 0;
                    unsigned int bytes_left = packet.bytes;

                    cout << "Baixando: " << name << endl;
                    cout << "Tamanho: " << packet.bytes << endl;

                    // packet_count = receive_file(bytes_left, newfile, sockdescr, sa, i);

                    auto start = high_resolution_clock::now();

                    while(1){
                        recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                        packet_count++;

                        if (packet.op == NEXT_FILE || packet.more == 0) {
                            break;
                        }

                        file.write(packet.buf, packet.bytes);

                        bytes_left -= packet.bytes; 
                        
                        memset(&packet, 0, sizeof(struct network_packet));
                    }

                    file.close();

                    if (packet_count == 0) {
                        auto stop = high_resolution_clock::now();
                        auto duration = duration_cast<microseconds>(stop-start);
                        auto throughput = filesystem::file_size(newfile) / duration.count();

                        cout << "Transmissão Concluida! Arquivo " << name << endl;
                        cout << "Salvo em: " << newfile << endl;
                        cout << packet_count << " packets received" << endl;
                        cout << "Tempo de transmissão: " << duration.count() << " microssegundos" << endl;
                        cout.precision(3);
                        cout << "Taxa de Transmissão: " << throughput << " MB/s" << endl; 

                        bytes_received += filesystem::file_size(newfile);
                    }

                    recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);
                }
                auto list_stop = high_resolution_clock::now();
                auto list_duration = duration_cast<microseconds>(list_stop-list_start);
                auto list_throughput = bytes_received / list_duration.count();
                
                cout << "Todos os arquivos foram recebidos!" << endl;
                cout << "Tempo de transmissão de todos os arquivos: " << list_duration.count() << " microssegundos" << endl;
                cout.precision(3);
                cout << "Taxa de Transmissão: " << list_throughput << " MB/s" << endl; 
                
                cout << endl << "**************************" << endl;

                if (argc == 4) {
                    if (strcmp(argv[3], "-c") == 0) {
                        cout << endl << endl;
                        usleep(300000);
                        const char *command = "find server_arqs -type f -printf \"%f\n\" > temp";
                        system(command);

                        ifstream files("temp");
                        string filename;

                        while (getline(files, filename)) {
                            compare_file(filename.c_str());
                        }
                        system("rm -f temp");
                        cout << endl << "--------------------------" << endl;
                    }
                }

                }
                break;
        }

        opt = chose_option();

    }
    close(sockdescr); 

    return 0;
}
