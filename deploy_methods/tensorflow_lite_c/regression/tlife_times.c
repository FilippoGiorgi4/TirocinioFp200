#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>

// include tensorflow lite
#include "tensorflow/lite/c/c_api.h"

#define INPUT_SIZE 10
#define THRESHOLD 0.1 // Threshold for anomaly detection

struct times_data
{
    int64_t timestamp;
    int64_t run_time;
} times;

struct metadata
{
    double train_feature[INPUT_SIZE];
    double label[1];
    double predicted[1]; // Store predicted values
};

// Function to calculate Mean Absolute Error
double mean_absolute_error(double *actual, double *predicted, int size)
{
    double error = 0.0;
    for (int i = 0; i < size; i++)
    {
        error += fabs(actual[i] - predicted[i]);
    }
    return error / size;
}

// Function to detect anomalies based on a threshold
int detect_anomaly(double actual, double predicted)
{
    return fabs(actual - predicted) > THRESHOLD;
}

// training from streaming data coming from dataset data.csv
int get_data(FILE *file, struct metadata *data)
{
    char line[1024];
    char *check = fgets(line, 1024, file);
    if (check == NULL)
        return -1;

    char *token = strtok(line, ",");
    for (int i = 0; i < INPUT_SIZE; i++)
    {
        data->train_feature[i] = atof(token);
        token = strtok(NULL, ",");
    }

    data->label[0] = atof(token);

    return 0;
}

#define NSEC_PER_SEC 1000000000LL

long long gettimens()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

int main()
{
    uint64_t start, end;

    times.timestamp = gettimens();
    times.run_time = 0;

    printf("timestamp[ns], inference_time[ns], actual, predicted, mae, anomaly_detected\n");

    // load tflite model
    TfLiteModel *model = TfLiteModelCreateFromFile("./model.tflite");
    TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
    // TfLiteInterpreterOptionsSetNumThreads(options, 2);

    // create interpreter
    TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);

    // get input tensor
    struct metadata data;
    FILE *file = fopen("./../data.csv", "r");
    // skip header
    char line[1024];
    char *check = fgets(line, 1024, file);

    for (;;)
    {
        if (get_data(file, &data) == -1)
            break;
        
        times.timestamp = gettimens();
        
        // set input as data.train_feature
        TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
        TfLiteTensorCopyFromBuffer(input_tensor, data.train_feature, sizeof(data.train_feature));
        
        // run inference
        TfLiteInterpreterInvoke(interpreter);
        
        // extract output
        const TfLiteTensor *output_tensor = TfLiteInterpreterGetOutputTensor(interpreter, 0);
        TfLiteTensorCopyToBuffer(output_tensor, data.predicted, sizeof(data.predicted));
        
        // Calculate metrics
        double mae = mean_absolute_error(data.label, data.predicted, 1);
        int anomaly_detected = detect_anomaly(data.label[0], data.predicted[0]);

        times.run_time = gettimens() - times.timestamp;

        // Print the results
        printf("%ld,%ld,%f,%f,%f,%d\n", times.timestamp, times.run_time, data.label[0], data.predicted[0], mae, anomaly_detected);
        
        // sleeping for 1ms minus the time taken to train the model
        // usleep(2000000 - times.run_time);
    }

    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);
    fclose(file); // Close the file

    return 0;
}