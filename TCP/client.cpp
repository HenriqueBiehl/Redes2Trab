#include <iostream> 
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <string.h>


using namespace std;

#define LIST_FILE 1
#define DOWNLOAD_FILE 2

#define END_OF_FILE "EOF"

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
    char buf[BUFSIZ+1]; 
    char *host; 
    //char *dados;

    if(argc != 4){
        cout << "Uso correto: client <Nome Serv> <porta> <dados>" << endl;
        exit(1); 
    }

    host = argv[1];
    //dados = argv [3];

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

    /*if(write(sock_desc, dados, strlen(dados)) != (ssize_t) strlen(dados)){
        cout << "Não consegui transmitir" << endl;
        exit(1); 
    }

    cout << "Transmiti: " << dados << endl;*/


    short opt; 
    int bytes_rec; 
    opt = chose_option();

    while(opt != 0){
        
        switch (opt) {
            
            case (LIST_FILE):
                strcpy(buf, "list-files");
                send(sock_desc, buf, strlen(buf), 0);
                memset(buf, 0, strlen(buf));
                
                cout << "Listando arquivos" << endl;
                while((bytes_rec = recv(sock_desc, buf, BUFSIZ, 0)) > 0){
                    /*if (string(buf).find(END_OF_FILE) != string::npos) {
                        break; // Terminou a transmissão
                    }*/
                    cout << buf;
                    memset(buf, 0, BUFSIZ);
                }

                cout << endl;
                break; 

            case (DOWNLOAD_FILE):
                strcpy(buf, "download");
                send(sock_desc, buf, strlen(buf), 0);
                memset(buf, 0, strlen(buf));
                
                ofstream file;
                file.open("received/arq-client.txt");

                while((bytes_rec = recv(sock_desc, buf, BUFSIZ, 0)) > 0){
                    file.write(buf, bytes_rec);
                    memset(buf, 0, BUFSIZ);
                }
                file.close();
                break;  
        }

        opt = chose_option();

    }
         

    //read(sock_desc, buf, BUFSIZ);
    //cout << "Recebi: " << buf << endl;

    close(sock_desc); 

    return 0;
}