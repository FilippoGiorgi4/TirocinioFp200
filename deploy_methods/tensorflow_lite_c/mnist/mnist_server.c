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
// include tensorflow lite
#include "tensorflow/lite/c/c_api.h"
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>
#include <json-c/json.h>

#define INPUT_SIZE 784 // Dimensioni delle funzionalità di input
#define OUTPUT_SIZE 10 // Numero di classi (ad esempio, cifre 0-9)

// Struttura per contenere metadati per le previsioni
struct metadata {
    float train_feature[INPUT_SIZE];
    int label;
    float prediction[OUTPUT_SIZE];
};

int get_data(FILE *file, struct metadata *data) {
    char line[INPUT_SIZE * 24 * 4];
    char *check = fgets(line, INPUT_SIZE * 24 * 4, file);
    if (check == NULL) {
        printf("Fine file\n");
        return -1;
    }

    char *token = strtok(line, ",");
    for (int i = 0; i < INPUT_SIZE; i++) {
        data->train_feature[i] = atof(token);
        token = strtok(NULL, ",");
    }
    return 0;
}

/********************************************************/
void gestore(int signo) {
    int stato;
    printf("esecuzione gestore di SIGCHLD\n");
    wait(&stato);
}
/********************************************************/

int main(int argc, char **argv) {
    /* CONTROLLO ARGOMENTI ---------------------------------- */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model_path>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    int                port, nread, server_fd, client_fd, length;
    const int          on = 1;
    const char *model_path = argv[1];
    char a_capo='\n';
    struct metadata data;
    socklen_t addr_len = sizeof(client_addr);

    /* INIZIALIZZAZIONE INDIRIZZO SERVER ----------------------------------------- */
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* CREAZIONE E SETTAGGI SOCKET TCP --------------------------------------- */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Creazione socket ");
        exit(3);
    }
    printf("Server: creata la socket d'ascolto per le richieste di ordinamento, fd=%d\n", server_fd);

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("set opzioni socket d'ascolto");
        exit(3);
    }
    printf("Server: set opzioni socket d'ascolto ok\n");

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind socket d'ascolto");
        exit(3);
    }
    printf("Server: bind socket d'ascolto ok\n");

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(3);
    }
    printf("Server: listen ok\n");
    signal(SIGCHLD, gestore);

    /* CICLO DI RICEZIONE RICHIESTE --------------------------------------------- */
    struct hostent    *hostTcp, *hostUdp;

    for (;;) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            if (errno == EINTR) {
                perror("Forzo la continuazione della accept");
                continue;
            } else
                exit(6);
        }
        if (fork() == 0) {
            hostTcp = gethostbyaddr((char *)&client_addr.sin_addr, sizeof(client_addr.sin_addr), AF_INET);
            if (hostTcp == NULL) {
                printf("client host information not found\n");
                close(client_fd);
                exit(6);
            } else
                printf("Server (figlio): host client e' %s \n", hostTcp->h_name);

                // Carica il modello TensorFlow Lite
                TfLiteModel *model = TfLiteModelCreateFromFile(model_path);
                if (model == NULL) {
                    fprintf(stderr, "Failed to load model\n");
                    return 1;
                }
                printf("Modello caricato correttamente\n");

                TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
                TfLiteInterpreterOptionsSetNumThreads(options, 1); // Parametro thread utilizzati
                TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
                TfLiteDelegate *xnnpack_delegate = TfLiteXNNPackDelegateCreate(&xnnpack_options);

                // Enable XNNPACK
                TfLiteInterpreterOptionsAddDelegate(options, xnnpack_delegate);

                // Crea l'interprete del modello
                TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
                if (interpreter == NULL) {
                    fprintf(stderr, "Failed to create interpreter\n");
                    return 1;
                }
                printf("Interpreter creato correttamente\n");

                // Alloca i tensori dell'interprete
                if (TfLiteInterpreterAllocateTensors(interpreter) != kTfLiteOk) {
                    fprintf(stderr, "Failed to allocate tensors\n");
                    return 1;
                }
                printf("Tensori allocati correttamente\n");

                // Ottieni i tensori di input e output
                TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
                if (input_tensor == NULL) {
                    fprintf(stderr, "Failed to get input tensor\n");
                    return 1;
                }
                printf("Input tensor ottenuto\n");

                // Leggi il file x_test.csv inviato dal client
                int x_test_file = open("x_test.csv", O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (x_test_file < 0) {
                    perror("Failed to open file");
                    return 1;
                }
                printf("File x_test.csv aperto correttamente\n");

                while (read(client_fd, &length, sizeof(int)) > 0) {
                    if (length == -1) {
                        // Esco dal ciclo di gestione di questo file
                        // senza però chiudere la socket.
                        break;
                    }

                    char buffer[length];
                    if (read(client_fd, &buffer, length) < 0) {
                        perror("read");
                        return 1;
                    }
                    // Scrivi il contenuto del buffer nel file
                    write(x_test_file, &buffer, length);
                    write(x_test_file, &a_capo, 1);
                }
                close(x_test_file);
                printf("Dati ricevuti e scritti su x_test.csv\n");

                // Riapri il file x_test.csv per leggere i dati
                FILE *data_file = fopen("x_test.csv", "r");
                if (!data_file) {
                    perror("Failed to open file");
                    return 1;
                }
                printf("File x_test.csv riaperto per la lettura\n");

                // Simulazione della lettura dei dati (per TensorFlow Lite)
                int num_samples = 10000;  // Supponiamo di avere 10000 campioni
                int count = 0;
                int *predictions = (int *)malloc(num_samples * sizeof(int));

                // Inizializzazione delle metriche
                const int input_size = sizeof(data.train_feature);
                const int output_size = sizeof(data.prediction);
                int predicted_label = 0;

                printf("Previsioni in corso...\n");
                // Lettura dei dati e esecuzione delle previsioni
                for (;;) {
                    if (get_data(data_file, &data) == -1){
                        break;
                    }

                    float max = 0;
                    // Copia i dati di input nel tensore di input
                    TfLiteTensorCopyFromBuffer(input_tensor, data.train_feature, input_size);

                    // Esegui l'interprete per ottenere le previsioni
                    TfLiteInterpreterInvoke(interpreter);
                    // Estrai l'output
                    const TfLiteTensor *output_tensor = TfLiteInterpreterGetOutputTensor(interpreter, 0);
                    // Copia i risultati delle previsioni
                    TfLiteTensorCopyToBuffer(output_tensor, data.prediction, output_size);

                    // Ottieni la label predetta
                    for (int i = 0; i < OUTPUT_SIZE; i++) {
                        if (data.prediction[i] > max) {
                            max = data.prediction[i];
                            predicted_label = i;
                        }
                    }

                    predictions[count] = predicted_label;
                    count++;
                }

                for(int i=0; i<num_samples; i++){
                    printf("Labels %d-esima: %d\n", i, predictions[i]);
                }

                printf("Previsioni completate\n");
                // Invia le etichette predette al client in json
                //creazione oggetto json
                struct json_object *json_obj = json_object_new_object();
                struct json_object *json_predictions = json_object_new_array();
                //popolo array Json con interi
                for (int i = 0; i < num_samples; i++) {
                    json_object_array_add(json_predictions, json_object_new_int(predictions[i]));
                }
                //aggiungo array json all'oggetto principale
                json_object_object_add(json_obj, "Labels", json_predictions);
                //converto in stringa e lo invio
                const char *json_str = json_object_to_json_string(json_obj);
                // Calcola la dimensione del JSON
                size_t json_size = strlen(json_str);
                //Invio dimensione al client
                write(client_fd, &json_size, sizeof(json_size));
                //invio la stringa json al client
                write(client_fd, json_str, strlen(json_str) + 1);
                printf("Etichette inviate al client\n");

                // Invia le etichette predette al controller
                int controller_fd = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in controller_addr;
                controller_addr.sin_family = AF_INET;
                controller_addr.sin_port = htons(9090);  // Supponiamo che il controller ascolti sulla porta 9090
                controller_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

                if (connect(controller_fd, (struct sockaddr *)&controller_addr, sizeof(controller_addr)) < 0) {
                    perror("Errore nella connessione al controller");
                    return 1;
                }
                printf("Connessione al controller avvenuta\n");

                // Invia le etichette predette al controller
                //Invio dimensione al client
                write(controller_fd, &json_size, sizeof(json_size));
                //invio la stringa json al client
                write(controller_fd, json_str, strlen(json_str) + 1);
                printf("Etichette inviate al controller\n");
                close(controller_fd);

                free(predictions);
                fclose(data_file);
                // Libera la memoria dell'interprete e del modello
                TfLiteInterpreterDelete(interpreter);
                TfLiteInterpreterOptionsDelete(options);
                TfLiteModelDelete(model);
            // Libero risorse
            printf("Figlio TCP terminato, libero risorse e chiudo. \n");
            close(client_fd);
            exit(0);
        }               // if fork
        close(client_fd);
    } // for
    return 0;
} // main