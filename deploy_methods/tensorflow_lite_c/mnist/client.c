#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <json-c/json.h>

#define PORT 30080 //porta del nodeport

int leggiLinea(int fd, char *linea) {
    char c;
    int  letti, i = 0;
    memset(linea, 0, sizeof(char) * 19600);
    while ((letti = read(fd, &c, 1)) > 0 && c != '\n') {
        linea[i] = c;
        i++;
    }
    printf("%s\n", linea);
    linea[i]='\0';
    return letti;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <csv_file_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    int sock, length, b;
    char sendbuffer[8192];
    char linea[19600];
    struct sockaddr_in server_addr;

    // Creazione del socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }

    int sndbuf, rcvbuf;
    socklen_t optlen = sizeof(int);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen);
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen);
    printf("Buffer invio: %d, Buffer ricezione: %d\n", sndbuf, rcvbuf);

    // Risoluzione del nome del servizio in indirizzo IP
    struct hostent *server = gethostbyname("deployment-mnist-service.crossplane-system.svc.cluster.local");
    if (server == NULL) {
        fprintf(stderr, "ERROR: No such host\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connessione al server
    b = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (b < 0) {
        perror("Failed to connect to server");
        close(sock);
        return 1;
    }

    printf("Connesso a server\n");
    printf("Apro file\n");
    // Apertura del file CSV
    // lettura da file e invio delle linee
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        close(sock);
        return 1;
    }

    printf("Inizio lettura\n");

    while( (b = fread(sendbuffer, 1, sizeof(sendbuffer), fp))>0 ){
        send(sock, sendbuffer, b, 0);
    }
    fclose(fp);
    shutdown(sock, SHUT_WR);
    
    // Ricezione delle etichette predette dal server
    // Ricezione della dimensione del JSON
    size_t json_size = 0;
    if (recv(sock, &json_size, sizeof(json_size), 0) < 0) {
        perror("Error receiving size");
        return 1;
    }
    char buff[json_size];
    if(recv(sock, buff, sizeof(buff), 0)<0){
        perror("Error receiving labels");
        return 1;
    }

    printf("%s", buff);

    // Chiusura della connessione
    close(sock);
    return 0;
}
