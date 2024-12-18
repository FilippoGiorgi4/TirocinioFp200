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

#define PORT 8080

int leggiLinea(int fd, char *linea) {
    char c;
    int  letti, i = 0;
    while ((letti = read(fd, &c, 1) != 0) && (c != '\n')) {
        linea[i] = c;
        i++;
    }
    linea[i] = '\0';
    return letti;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <csv_file_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    int sock, length;
    char linea[19600];
    struct sockaddr_in server_addr;

    // Creazione del socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connessione al server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        close(sock);
        return 1;
    }

    printf("Apro file\n");
    // Apertura del file CSV
    int fd = open(file_path, O_RDONLY, 0777);
    if (fd < 0) {
        perror("Failed to open file");
        close(sock);
        return 1;
    }

    printf("Inizio lettura\n");
    /* lettura da file e invio delle linee*/
    while (leggiLinea(fd, linea) > 0) {
        length = strlen(linea) + 1;
        // invio lunghezza linea e linea
        if (write(sock, &length, sizeof(int)) < 0) {
            perror("Errore invio lunghezza");
            return 1;
        }
        if (write(sock, linea, strlen(linea) + 1) < 0) {
            perror("Errore invio linea");
            return 1;
        }
    }
    // il file e' terminato, lo segnalo al server
    length = -1;
    printf("Invio fine file\n");
    if (write(sock, &length, sizeof(int)) < 0) {
        perror("write");
        return -1;
    }
    printf("Fine invio");
    close(fd);

    // Ricezione delle etichette predette dal server
    // Ricezione della dimensione del JSON
    size_t json_size = 0;
    if (read(sock, &json_size, sizeof(json_size)) < 0) {
        perror("Error receiving size");
        return 1;
    }
    char buff[json_size];
    if(read(sock, buff, sizeof(buff))<0){
        perror("Error receiving labels");
        return 1;
    }

    printf("%s", buff);

    // Chiusura della connessione
    close(sock);
    return 0;
}
