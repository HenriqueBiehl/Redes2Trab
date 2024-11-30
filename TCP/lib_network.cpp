#include "lib_network.hpp"

// Opções do usuário
short chose_option(){
    short opt; 

    std::cout << "Choose your option:" << std::endl;
    std::cout << "[0] - Exit" << std::endl; 
    std::cout << "[1] - List of Files" << std::endl; 
    std::cout << "[2] - Download one File" << std::endl; 
    std::cout << "[3] - Dowload all Files" << std::endl;

    std::cin >> opt; 
    while(opt < 0 && opt > 2){
        std::cout << "Choose between 0 and 2" << std::endl; 
        std::cin >> opt; 
    }

    return opt;
}

// Recebe uma lista de nomes de arquivos do servidor
int receive_file_list(int socket){
    struct network_packet packet;
    int ack_count;
    int packet_count = 0;

    // Recebendo do servidor
    while(recv(socket, &packet, sizeof(struct network_packet) , 0) >= 0){

        if(packet.op == ERROR){
            std::cout << packet.buf;
            return -1;
        }

        // Imprime o nome dos arquivos
        std::cout << packet.buf;
        memset(&packet, 0, sizeof(struct network_packet));
        ack_count++;
        packet_count++;

        // Se a mensagem é marcado como a última, encerra a operação
        if(packet.more == 0)
            break;

        // A cada 100 mensagens, envia um ACK
        if(ack_count == PACK_ROOF){
            ack_count  = 0; 
            packet.op = ACK;
            send(socket, &packet, sizeof(struct network_packet), 0);
        }
    }

    return packet_count;
}

// Recebe um arquivo do servidor
int receive_file(int socket, char *destination, int total_bytes){
    struct network_packet packet;
    short ack_count = 0;
    int packet_count = 0;

    std::ofstream file(destination);
    
    if(!file.is_open()){
        std::cout << "FALHA AO ABRIR ARQUIVO!" << std::endl;
        return -1; 
    }             

    // Recebe até todos os bytes terem sido recebidos
    while(total_bytes > 0 ){
        
        if(recv(socket, &packet, sizeof(struct network_packet) , 0) < 0){
            std::cout << "Erro ao receber pacote! " << std::endl; 
            exit(1);
        }        
        
        packet_count++;
        ack_count++;

        // Escreve em um arquivo de destino
        file.write(packet.buf, packet.bytes);

        total_bytes -= packet.bytes; 
        
        // A cada 100 mensagens, envia um ACK
        if(ack_count == PACK_ROOF){
            ack_count  = 0; 
            packet.op = ACK;
            send(socket, &packet, sizeof(struct network_packet), 0);
        }
        
        memset(&packet, 0, sizeof(struct network_packet));
    }

    file.close();

    return packet_count;
}

// Envia um arquivo
int send_file(int socket, char *file_name, short op_flag){
    struct network_packet packet;
    int packet_count = 0;
    short ack_count = 0;

    // Abre o arquivo
    std::ifstream file(file_name); 

    if(!file.is_open()){
        std::cout << "Erro ao abrir arquivo" << std::endl;
        return -1;
    }
    
    memset(&packet, 0, sizeof(struct network_packet)); 
    std::streamsize bytes_read; 
    
    // Envia todo o arquivo
    while(!file.eof()){
        file.read(packet.buf, BUFF_SIZE);
        bytes_read = file.gcount(); 
        packet.op = op_flag; 
        packet.bytes = bytes_read;

        if(file.eof() && op_flag != DOWNLOAD_ALL_FILES)
            packet.op = 0; 
        else 
            packet.op = 1;

        // Faz o envio
        send(socket, &packet, sizeof(struct network_packet), 0);

        memset(&packet, 0, sizeof(struct network_packet)); 
        packet_count++;
        ack_count++;

        // A cada 100 envios, espera um ACK
        if(ack_count == PACK_ROOF){
            std::cout << "100 pacotes enviados, aguardando ACK..." << std::endl;
            ack_count = 0;
            
            if(recv(socket, &packet, sizeof(struct network_packet) , 0) < 0){
                std::cout << "Erro ao receber pacote! " << std::endl; 
                exit(1);
            }                   
            
            if(packet.op == ACK)
                std::cout << "ACK Recebido!" << std::endl;
            else 
                return -1;
        }
    }

    file.close();

    return packet_count;
}