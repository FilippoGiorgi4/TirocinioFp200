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
#include "tensorflow/lite/c/c_api.h"
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>
#include <json-c/json.h>

#define INPUT_SIZE 784
#define OUTPUT_SIZE 10

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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model_path>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    int port, nread, server_fd, client_fd, length;
    const int on = 1;
    const char *model_path = argv[1];
    char a_capo = '\n';
    struct metadata data;
    socklen_t addr_len = sizeof(client_addr);

    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(30080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

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

    for (;;) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            if (errno == EINTR) {
                perror("Forzo la continuazione della accept");
                continue;
            } else
                exit(6);
        }

        printf("Connessione accettata\n");

        TfLiteModel *model = TfLiteModelCreateFromFile(model_path);
        if (model == NULL) {
            fprintf(stderr, "Failed to load model\n");
            close(client_fd);
            continue;
        }
        printf("Modello caricato correttamente\n");

        TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
        TfLiteInterpreterOptionsSetNumThreads(options, 1);
        TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
        TfLiteDelegate *xnnpack_delegate = TfLiteXNNPackDelegateCreate(&xnnpack_options);

        TfLiteInterpreterOptionsAddDelegate(options, xnnpack_delegate);

        TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
        if (interpreter == NULL) {
            fprintf(stderr, "Failed to create interpreter\n");
            TfLiteModelDelete(model);
            close(client_fd);
            continue;
        }
        printf("Interpreter creato correttamente\n");

        if (TfLiteInterpreterAllocateTensors(interpreter) != kTfLiteOk) {
            fprintf(stderr, "Failed to allocate tensors\n");
            TfLiteInterpreterDelete(interpreter);
            TfLiteModelDelete(model);
            close(client_fd);
            continue;
        }
        printf("Tensori allocati correttamente\n");

        TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
        if (input_tensor == NULL) {
            fprintf(stderr, "Failed to get input tensor\n");
            TfLiteInterpreterDelete(interpreter);
            TfLiteModelDelete(model);
            close(client_fd);
            continue;
        }
        printf("Input tensor ottenuto\n");

        int x_test_file = open("x_test.csv", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (x_test_file < 0) {
            perror("Failed to open file");
            TfLiteInterpreterDelete(interpreter);
            TfLiteModelDelete(model);
            close(client_fd);
            continue;
        }
        printf("File x_test.csv aperto correttamente\n");

        while (read(client_fd, &length, sizeof(int)) > 0) {
            if (length == -1) {
                break;
            }

            char buffer[length];
            if (read(client_fd, &buffer, length) < 0) {
                perror("read");
                close(x_test_file);
                TfLiteInterpreterDelete(interpreter);
                TfLiteModelDelete(model);
                close(client_fd);
                continue;
            }
            write(x_test_file, &buffer, length);
            write(x_test_file, &a_capo, 1);
        }
        close(x_test_file);
        printf("Dati ricevuti e scritti su x_test.csv\n");

        FILE *data_file = fopen("x_test.csv", "r");
        if (!data_file) {
            perror("Failed to open file");
            TfLiteInterpreterDelete(interpreter);
            TfLiteModelDelete(model);
            close(client_fd);
            continue;
        }
        printf("File x_test.csv riaperto per la lettura\n");

        int num_samples = 10000;
        int count = 0;
        int *predictions = (int *)malloc(num_samples * sizeof(int));

        const int input_size = sizeof(data.train_feature);
        const int output_size = sizeof(data.prediction);
        int predicted_label = 0;

        printf("Previsioni in corso...\n");
        for (;;) {
            if (get_data(data_file, &data) == -1) {
                break;
            }

            float max = 0;
            TfLiteTensorCopyFromBuffer(input_tensor, data.train_feature, input_size);

            TfLiteInterpreterInvoke(interpreter);

            const TfLiteTensor *output_tensor = TfLiteInterpreterGetOutputTensor(interpreter, 0);
            TfLiteTensorCopyToBuffer(output_tensor, data.prediction, output_size);

            for (int i = 0; i < OUTPUT_SIZE; i++) {
                if (data.prediction[i] > max) {
                    max = data.prediction[i];
                    predicted_label = i;
                }
            }

            predictions[count] = predicted_label;
            count++;
        }

        for (int i = 0; i < num_samples; i++) {
            printf("Labels %d-esima: %d\n", i, predictions[i]);
        }

        printf("Previsioni completate\n");

        struct json_object *json_obj = json_object_new_object();
        struct json_object *json_predictions = json_object_new_array();
        for (int i = 0; i < num_samples; i++) {
            struct json_object *json_label = json_object_new_int(predictions[i]);
            json_object_array_add(json_predictions, json_label);
        }
        json_object_object_add(json_obj, "predictions", json_predictions);
        const char *json_string = json_object_to_json_string(json_obj);
        length = strlen(json_string) + 1;
        write(client_fd, &length, sizeof(int));
        write(client_fd, json_string, length);

        printf("Dati inviati al client\n");

        struct json_object *json_labels = json_object_new_array();
        for (int i = 0; i < num_samples; i++) {
            struct json_object *json_label = json_object_new_int(predictions[i]);
            json_object_array_add(json_labels, json_label);
        }
        struct json_object *json_final_obj = json_object_new_object();
        json_object_object_add(json_final_obj, "labels", json_labels);
        const char *json_final_string = json_object_to_json_string(json_final_obj);
        length = strlen(json_final_string) + 1;
        write(client_fd, &length, sizeof(int));
        write(client_fd, json_final_string, length);

        json_object_put(json_obj);
        json_object_put(json_final_obj);

        printf("Dati inviati al controller\n");

        fclose(data_file);
        free(predictions);
        TfLiteInterpreterDelete(interpreter);
        TfLiteModelDelete(model);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
