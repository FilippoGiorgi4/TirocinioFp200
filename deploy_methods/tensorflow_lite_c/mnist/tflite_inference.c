#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

// include tensorflow lite
#include "tensorflow/lite/c/c_api.h"
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>

#define INPUT_SIZE 784 // Dimensioni delle funzionalità di input
#define OUTPUT_SIZE 10 // Numero di classi (ad esempio, cifre 0-9)

// Struttura per contenere metadati per le previsioni
struct metadata {
    float train_feature[INPUT_SIZE];
    int label;
    float prediction[OUTPUT_SIZE];
};

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

// Funzione per leggere il file CSV contenente i dati di input
int get_data(FILE *file, struct metadata *data) {
    char line[INPUT_SIZE * 24 * 4];
    char *check = fgets(line, INPUT_SIZE * 24 * 4, file);
    if (check == NULL)
        return -1;

    char *token = strtok(line, ",");
    for (int i = 0; i < INPUT_SIZE; i++) {
        data->train_feature[i] = atof(token);
        token = strtok(NULL, ",");
    }
    return 0;
}

// Funzione per leggere il file CSV contenente le etichette
int get_label(FILE *file, struct metadata *data) {
    char line[4]; // Una singola etichetta per esempio (max 3 carattere più terminatore)
    char *check = fgets(line, sizeof(line), file); // Leggi una riga dal file
    if (check == NULL)
        return -1; // Se non c'è più dati nel file, esci

    // Assegna l'etichetta letta a data.label, che rappresenta la classe corretta
    data->label = atoi(line); // contiene il numero della classe
    return 0; // Successo
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model_path>\n", argv[0]);
        return 1;
    }

    const char *model_path = argv[1];
    struct metadata data;
    FILE *data_file = fopen("../../../mnist_test/x_test.csv", "r");
    FILE *labels_file = fopen("../../../mnist_test/y_test.csv", "r");
    if (!data_file && !labels_file) {
        perror("Failed to open file");
        return 1;
    }

    // Carica il modello TensorFlow Lite
    TfLiteModel *model = TfLiteModelCreateFromFile(model_path);
    if (model == NULL) {
        fprintf(stderr, "Failed to load model\n");
        return 1;
    }

    TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, 1); //parametro thread utilizzati
    TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
    // crea xnnpackdelegate -> acceleratore per CPU (parametro utilizzabile)
    TfLiteDelegate *xnnpack_delegate = TfLiteXNNPackDelegateCreate(&xnnpack_options);

    // enable XNNPACK
    TfLiteInterpreterOptionsAddDelegate(options, xnnpack_delegate);

    // Crea l'interprete del modello
    TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
    if (interpreter == NULL) {
        fprintf(stderr, "Failed to create interpreter\n");
        return 1;
    }

    // Alloca i tensori dell'interprete
    if (TfLiteInterpreterAllocateTensors(interpreter) != kTfLiteOk) {
        fprintf(stderr, "Failed to allocate tensors\n");
        return 1;
    }

    // Ottieni i tensori di input e output
    TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
    if (input_tensor == NULL) {
        fprintf(stderr, "Failed to get input tensor\n");
        return 1;
    }

    // Inizializzazione metriche
    const int input_size = sizeof(data.train_feature);
    const int output_size = sizeof(data.prediction);
    int predicted_label = 0;
    int confusione_matrix[OUTPUT_SIZE][OUTPUT_SIZE] = {0}; //righe = reali, colonne = predetti

    // Lettura dei dati e esecuzione delle previsioni
    for(;;){

        if (get_data(data_file, &data) == -1 || get_label(labels_file, &data) == -1)
            break;

        float max = 0; 
        // Copia i dati di input nel tensore di input
        TfLiteTensorCopyFromBuffer(input_tensor, data.train_feature, input_size);

        // Esegui l'interprete per ottenere le previsioni
        TfLiteInterpreterInvoke(interpreter);
        // Estrai output
        const TfLiteTensor *output_tensor = TfLiteInterpreterGetOutputTensor(interpreter, 0);
        // Copia i risultati delle previsioni
        TfLiteTensorCopyToBuffer(output_tensor, data.prediction, output_size);

        // Ottieni la label predetta
        for(int i=0; i<OUTPUT_SIZE; i++){
            if(data.prediction[i] > max){
                max = data.prediction[i];
                predicted_label = i;
            }
        }

        // Aggiornamento della matrice di confusione
        for(int i=0; i<OUTPUT_SIZE; i++){
            for(int j=0; j<OUTPUT_SIZE; j++){
                if(data.label == j && predicted_label == i){
                    confusione_matrix[i][j]++;
                }
            }
        }
	}
    //stampo matrice confusione
    for(int i=0; i<OUTPUT_SIZE; i++){
        for(int j=0; j<OUTPUT_SIZE; j++){
            printf("%d ", confusione_matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");

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

    // Chiudi il file e pulisci le risorse
    fclose(data_file);
    fclose(labels_file);
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);

    return 0;
}
