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
int receive_file_list(int sockdescr, struct sockaddr_in sa, unsigned int i) {
    int bytes_rec;
    struct network_packet packet;
    int packet_count = 0;

    // Recebendo do servidor
    while((bytes_rec = recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i)) >= 0){
        packet_count++;

        // Imprime o nome dos arquivos
        std::cout << packet.buf;
        memset(&packet, 0, sizeof(struct network_packet));

        // Se a mensagem é marcada como a última, encerra a operação
        if(packet.more == 0 || packet.op == NEXT_FILE)
            break;
    }
    return packet_count;
}

// Recebe um arquivo do servidor
int receive_file(int bytes_left, char* destination, int sockdescr, sockaddr_in sa, unsigned int i) {
    struct network_packet packet;
    int packet_count = 0;

    std::ofstream file;
    file.open(destination);

    if(!file.is_open()){
        std::cout << "Erro ao abrir arquivo" << std::endl;
        return -1;
    }

    // Recebe até todos os bytes terem sido recebidos ou até o servidor notificar que o arquivo terminou
    while(bytes_left > 0 ){
        packet_count++;
        recvfrom(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &sa, &i);

        // Escreve em um arquivo de destino
        file.write(packet.buf, packet.bytes);

        bytes_left -= packet.bytes; 

        if(packet.more == 0)
            break;
        
        memset(&packet, 0, sizeof(struct network_packet));
    }

    file.close();
    return packet_count++;
}

// Envia um arquivo
int send_file(int sockdescr, char *arq_name, short op_flag, struct sockaddr_in client, int i) {
    struct network_packet packet;
    int packet_count = 0;

    // Abre o arquivo
    std::ifstream file(arq_name);

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

        // No fim do arquivo, espera um tempo para o cliente processar o arquivo e envia uma notificação que a transmissão terminou
        if(file.eof() && op_flag != DOWNLOAD_ALL_FILES){
            usleep(1000);
            packet.more = 0;
        }
        else{
            packet.more = 1;
        }

        // Faz o envio
        sendto(sockdescr, &packet, sizeof(struct network_packet), 0, (struct sockaddr *) &client, i);
        memset(&packet, 0, sizeof(struct network_packet));

        packet_count++;
    }

    file.close();

    return packet_count;
}

// Compara um arquivo do cliente com um do servidor, se os dois existirem localmente
void compare_file(const char* filename) {
    // Abre dois arquivos, o do servidor(original) e o do cliente(received)
    std::cout << "No arquivo " << filename << ":" << std::endl;
    char original_name[MAX_ARQ_NAME+1];
    char received_name[MAX_ARQ_NAME+1];

    memset(original_name, 0 , MAX_ARQ_NAME + 1);
    strcpy(original_name, "arqs/");
    strcat(original_name, filename);

    memset(received_name, 0 , MAX_ARQ_NAME + 1);
    strcpy(received_name, "received/");
    strcat(received_name, filename);

    std::ifstream original;
    std::ifstream received;

    original.open(original_name);
    received.open(received_name);

    std::filesystem::path p_original{original_name};
    std::filesystem::path p_received{received_name};

    // Se o arquivo tiver sido recebido, inicia a comparação
    if (!received.fail()) {
        long int original_size = std::filesystem::file_size(p_original);
        long int received_size = std::filesystem::file_size(p_received);

        // Imprime os tamanhos
        std::cout << "Tamanho do arquivo original: " << original_size << std::endl; 
        std::cout << "Tamanho do arquivo recebido: " << received_size << std::endl;

        // Compara para ver quantos bytes não chegaram ao decorrer da transmissão
        long int diff = abs(original_size - received_size);
        long int percentage = 100 * received_size / original_size;

        char c1[2], c2[2];
        long int diff_bytes = 0;

        std::cout << "Um total de " << diff << " bytes não chegaram, sendo que cerca de " << percentage << "\% dos bytes foram transmitidos" << std::endl;

        // Conta o número de bytes que chegaram na posição errada, ou com o conteúdo errado
        while (!original.eof() && !received.eof()) {
            original.read(c1, 1);
            received.read(c2, 1);
            // std::cout << "c1 = " << c1[0] << " c2 = " << c2[0] << std::endl;
            if (c1[0] != c2[0]) {
                diff_bytes++;
            }
        }

        long int percentage_diff = 100 * diff_bytes / received_size;

        // Imprime a quantidade de bytes que chegou de forma errada
        std::cout << "Um total de " << diff_bytes << " bytes chegaram errado, sendo que cerca de "
            << percentage_diff << "\% dos bytes chegaram com algum problema(Ex. Ordem errada, valor incorreto, etc.)"
            << std::endl;
        std::cout << std::endl;
    }
}
