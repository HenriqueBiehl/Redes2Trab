#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h> 

#define TAM_LINE 5
#define MAX_HOST_NAME 30


#define LIST_FILE 1
#define DOWNLOAD_FILE 2
#define BUFF_SIZE 2048
#define MAX_ARQ_NAME 1024

#define END_OF_FILE "EOF"


struct network_packet{
    unsigned short op; 
    unsigned short more;
    unsigned long int bytes; 
    char buf[BUFF_SIZE + 1];
};

using namespace std;

int main(int argc, char *argv[]){
    int sock_listen, sock_answer;
    unsigned int i; 
    char buf[BUFSIZ+1];
    struct sockaddr_in end_local;
    struct sockaddr_in end_cliente;
    struct hostent *hp;
    char localhost[MAX_HOST_NAME+1];

    if(argc != 2){
        cout << "Uso correto: Servidor <porta>" << endl;
        exit(1);
    }

    cout << "BUFSIZ: " << BUFSIZ << endl;

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

    i = sizeof(end_local); 

    if((sock_answer = accept(sock_listen, (struct sockaddr*)&end_cliente, &i)) < 0){
        cout << "Não consegui estabelecer a conexão" << endl;
        exit(1);              
    }

    struct network_packet packet;
    memset(&packet, 0, sizeof(struct network_packet));

    while(1){
        // i = sizeof(end_local); 

        // if((sock_answer = accept(sock_listen, (struct sockaddr*)&end_cliente, &i)) < 0){
        //     cout << "Não consegui estabelecer a conexão" << endl;
        //     exit(1);              
        // }


        read(sock_answer, &packet, sizeof(struct network_packet));
        
        switch(packet.op){

            case (LIST_FILE):
            {
                cout << "Recebi pedido de Lista" << endl;

                const char *command = "find arqs -type f -printf \"%f %s bytes\n\" > temp";
        
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

                    cout << "Enviando : " << buf << endl;

                    if(file.eof()){
                        packet.more = 0;
                    }
                    else{
                        packet.more = 1;
                    }

                    send(sock_answer, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet)); 
                }

                cout << "Lista enviada com sucesso!" << endl;
                system("rm -f temp");
            }
            break;

            case(DOWNLOAD_FILE):
            {
                char arq_name[MAX_ARQ_NAME + 1];

                memset(arq_name, 0 , MAX_ARQ_NAME + 1);
                strcpy(arq_name, "arqs/");
                strcat(arq_name, packet.buf);

                ifstream file(arq_name); 

                if(!file.is_open()){
                    cout << "Erro ao abrir arquivo" << endl;
                }
                
                memset(&packet, 0, sizeof(struct network_packet)); 
                streamsize bytes_read; 

                while(!file.eof()){

                    file.read(packet.buf, BUFF_SIZE);
                    bytes_read = file.gcount(); 
                    packet.op = DOWNLOAD_FILE; 
                    packet.bytes = bytes_read;

                    cout << "Enviando " << bytes_read << "bytes" << endl;

                    if(file.eof()){
                        packet.more = 0;
                    }
                    else{
                        packet.more = 1;
                    }

                    send(sock_answer, &packet, sizeof(struct network_packet), 0);
                    memset(&packet, 0, sizeof(struct network_packet)); 
                }

                cout << "Arquivo enviado com sucesso!" << endl;
            }    

        }

        

        //read(sock_answer, buf, BUFSIZ);
        //cout << "Recebi: " << buf << endl;
        //write(sock_answer, buf, BUFSIZ);
        //close(sock_answer);
    }
    
    close(sock_answer);

    return 1;
}

