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

#define INPUT_SIZE 784 // Size of input features
#define OUTPUT_SIZE 10 // Number of classes (for example, digits 0-9)

// Structure to hold metadata for predictions
struct metadata {
    float train_feature[INPUT_SIZE];
    float label[OUTPUT_SIZE];
};

// Function to calculate accuracy
float calculate_accuracy(int true_positive, int true_negative, int total) {
    return (float)(true_positive + true_negative) / total;
}

// Function to calculate precision
float calculate_precision(int true_positive, int false_positive) {
    return (float)true_positive / (true_positive + false_positive);
}

// Function to calculate recall
float calculate_recall(int true_positive, int false_negative) {
    return (float)true_positive / (true_positive + false_negative);
}

// Function to calculate Mean Absolute Error
float calculate_mae(float *actual, float *predicted, int size) {
    float error_sum = 0.0;
    for (int i = 0; i < size; i++) {
        error_sum += fabs(actual[i] - predicted[i]);
    }
    return error_sum / size;
}

// Function to detect anomalies based on a simple threshold
int detect_anomaly(float *predictions, int size, float threshold) {
    for (int i = 0; i < size; i++) {
        if (predictions[i] > threshold) {
            return 1; // Anomaly detected
        }
    }
    return 0; // No anomaly
}

// Function to read data from CSV
int get_data(FILE *file, struct metadata *data) {
    char line[INPUT_SIZE * 24 * 4];
    char *check = fgets(line, sizeof(line), file);
    if (check == NULL)
        return -1;

    char *token = strtok(line, ",");
    for (int i = 0; i < INPUT_SIZE; i++) {
        data->train_feature[i] = atof(token);
        token = strtok(NULL, ",");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <model_path> & <data_path>\n", argv[0]);
        return 1;
    }

    const char *model_path = argv[1];
    const char *data_path = argv[2];
    struct metadata data;
    FILE *file = fopen(data_path, "r");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    // Initialize metrics
    int true_positive = 0, true_negative = 0, false_positive = 0, false_negative = 0;
    float actual[OUTPUT_SIZE] = {0}; // Actual labels
    float predicted[OUTPUT_SIZE] = {0}; // Predicted labels

    // Simulate reading data and making predictions
    while (get_data(file, &data) != -1) {
        // Here you would call your model to get predictions
        // For demonstration, we will simulate predictions
        // Assume the model predicts the first class (0) for simplicity
        predicted[0] = 1.0; // Simulated prediction for class 0
        actual[0] = 1.0; // Simulated actual label for class 0

        // Update confusion matrix
        if (predicted[0] == actual[0]) {
            true_positive++;
        } else {
            false_positive++;
            false_negative++;
        }
        true_negative++; // Assuming all other predictions are negative
    }

    // Calculate metrics
    int total = true_positive + true_negative + false_positive + false_negative;
    float accuracy = calculate_accuracy(true_positive, true_negative, total);
    float precision = calculate_precision(true_positive, false_positive);
    float recall = calculate_recall(true_positive, false_negative);
    float mae = calculate_mae(actual, predicted, OUTPUT_SIZE);
    int anomaly_detected = detect_anomaly(predicted, OUTPUT_SIZE, 0.5); // Example threshold

    // Print metrics
    printf("Accuracy: %.2f\n", accuracy);
    printf("Precision: %.2f\n", precision);
    printf("Recall: %.2f\n", recall);
    printf("Mean Absolute Error: %.2f\n", mae);
    printf("Anomaly Detected: %s\n", anomaly_detected ? "Yes" : "No");

    fclose(file);
    return 0;
}