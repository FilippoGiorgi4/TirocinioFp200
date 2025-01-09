#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <json-c/json.h>

#define PORT 9090
#define OUTPUT_SIZE 10

// Funzione per calcolare l'accuracy
float calculate_accuracy(float error_rate) {
    return (float)(1 - error_rate);
}

//Funzione per calcolare l'error rate
float calculate_error_rate( int false_positive, int false_negative, int total) {
    return (float)(false_positive + false_negative) / total;
}

// Funzione per calcolare la precision
float calculate_precision(int true_positive, int false_positive) {
    return (float)true_positive / (true_positive + false_positive);
}

// Funzione per calcolare la recall
float calculate_recall(int true_positive, int false_negative) {
    return (float)true_positive / (true_positive + false_negative);
}

// Funzione per calcolare F1-Score
float calculate_f1_score(float precision, float recall) {
    return 2 * (precision * recall) / (precision + recall);
}

void get_labels(const char *filename, int *labels, int num_samples) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    char line[4]; // Una singola etichetta per esempio (max 3 carattere più terminatore)
    for (int i = 0; i < num_samples; i++) {
        if (fgets(line, sizeof(line), file) == NULL) {
            break;
        }
        labels[i] = atoi(line);
        printf("Labels %d: %d\n", i, labels[i]);
    }
    fclose(file);
}

void calculate_confusion_matrix(int *true_labels, int *predicted_labels, int num_samples, int confusion_matrix[OUTPUT_SIZE][OUTPUT_SIZE]) {
    for (int i = 0; i < num_samples; i++) {
        int true_label = true_labels[i];
        int predicted_label = predicted_labels[i];
        if (true_label < OUTPUT_SIZE && predicted_label < OUTPUT_SIZE) {
            confusion_matrix[true_label][predicted_label]++;
        }
    }
}

void print_confusion_matrix(int confusion_matrix[OUTPUT_SIZE][OUTPUT_SIZE]) {
    printf("Confusion Matrix:\n");
    for (int i = 0; i < OUTPUT_SIZE; i++) {
        for (int j = 0; j < OUTPUT_SIZE; j++) {
            printf("%d ", confusion_matrix[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <y_test.csv>\n", argv[0]);
        return 1;
    }

    const char *y_test_path = argv[1];
    int num_samples = 10000;  // Supponiamo di avere 10000 campioni

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Creazione del socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Binding del socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_fd);
        return 1;
    }

    // Ascolto del socket
    if (listen(server_fd, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_fd);
        return 1;
    }

    printf("Controller listening on port %d...\n", PORT);

    while (1) {
        // Accettare la connessione dal server
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Failed to accept connection");
            return 1;
        }

        // Ricevere la dimensione del JSON
        size_t json_size;
        if (recv(client_fd, &json_size, sizeof(json_size), 0) < 0) {
            perror("Failed to receive JSON size");
            close(client_fd);
            return 1;
        }

        // Ricevere il JSON
        char buffer[json_size + 1];
        ssize_t bytes_received = recv(client_fd, buffer, json_size, 0);
        if (bytes_received < 0) {
            perror("Failed to receive JSON data");
            close(client_fd);
            return 1;
        }
        buffer[bytes_received] = '\0';

        // Parsare il file JSON
        struct json_object *parsed_json = json_tokener_parse(buffer);
        struct json_object *json_predictions;
        json_object_object_get_ex(parsed_json, "Labels", &json_predictions);

        int predicted_labels[num_samples];
        for (int i = 0; i < num_samples; i++) {
            predicted_labels[i] = json_object_get_int(json_object_array_get_idx(json_predictions, i));
        }

        // Leggere il file y_test.csv
        int true_labels[num_samples];
        get_labels(y_test_path, true_labels, num_samples);

        // Calcolare la matrice di confusione
        int confusione_matrix[OUTPUT_SIZE][OUTPUT_SIZE] = {0};
        calculate_confusion_matrix(true_labels, predicted_labels, num_samples, confusione_matrix);

        // Stampare la matrice di confusione
        print_confusion_matrix(confusione_matrix);

        // Aggiorna valori TP, TN, FP, FN
        int total = 0;
        int true_positive[OUTPUT_SIZE] = {0}, true_negative[OUTPUT_SIZE] = {0}, false_positive[OUTPUT_SIZE] = {0},
            false_negative[OUTPUT_SIZE] = {0}, totalRow [OUTPUT_SIZE] = {0}, totalCol [OUTPUT_SIZE] = {0};
        for(int i=0; i<OUTPUT_SIZE; i++){ //row
            true_positive[i] += confusione_matrix[i][i];
            for(int j=0; j<OUTPUT_SIZE; j++){ //col
                if(i != j){
                    false_positive[j] += confusione_matrix[j][i];
                    false_negative[i] += confusione_matrix[i][j];
                }
                totalRow[i] += confusione_matrix[i][j];
            }
            total += totalRow[i];
        }
        for(int i=0; i<OUTPUT_SIZE; i++){
            true_negative[i] = total - false_positive[i] - false_negative[i] - true_positive[i];
        }

        // Calcola e stampa le metriche per ciascuna classe
        for (int i = 0; i < OUTPUT_SIZE; i++) {
            int tp = true_positive[i];
            int fp = false_positive[i];
            int fn = false_negative[i];
            int tn = true_negative[i];
            
            // Calcola le metriche
            float precision = calculate_precision(tp, fp);
            float recall = calculate_recall(tp, fn);
            float f1_score = calculate_f1_score(precision, recall);
            float error_rate = calculate_error_rate(fp, fn, totalRow[i]);
            float accuracy_class = 1-error_rate;

            printf("Classe %d: TP: %d, FP: %d, TN: %d, FN: %d\n", i, tp, fp, tn, fn);
            printf("Classe %d - Error: %.2f, Accuracy: %.2f, Precision: %.2f, Recall: %.2f, F1-Score: %.2f\n", 
                i, error_rate, accuracy_class, precision, recall, f1_score);
        }

        // Calcola le metriche complessive
        int total_tp = 0, total_tn = 0, total_fp = 0, total_fn = 0;
        for (int i = 0; i < OUTPUT_SIZE; i++) {
            total_tp += true_positive[i];
            total_tn += true_negative[i];
            total_fp += false_positive[i];
            total_fn += false_negative[i];
        }

        float overall_error_rate = calculate_error_rate(total_fp, total_fn, total);
        printf("\nError Rate Complessivo: %.2f\n", overall_error_rate);

        float overall_accuracy = calculate_accuracy(overall_error_rate);
        printf("Accuracy Complessiva: %.2f\n", overall_accuracy);

        float overall_precision = calculate_precision(total_tp, total_fp);
        printf("Precision Complessiva: %.2f\n", overall_precision);

        float overall_recall = calculate_recall(total_tp, total_fn);
        printf("Recall Complessiva: %.2f\n", overall_recall);

        float overall_f1_score = calculate_f1_score(overall_precision, overall_recall);
        printf("F1-Score Complessivo: %.2f\n", overall_f1_score);

        // Pulire
        json_object_put(parsed_json);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
