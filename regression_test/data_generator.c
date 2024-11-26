#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define INPUT_SIZE 10
#define DATASET_SIZE 1e5
#define MAX_VALUE 8
#define MIN_VALUE 6

double synthetic_function(double *x)
{
    for (int k = 0; k < INPUT_SIZE; k++)
    {
        x[k] = MIN_VALUE + (MAX_VALUE - MIN_VALUE) * ((double)rand() / RAND_MAX);
    }
    double exps[INPUT_SIZE] = {1.5, 1.1, 1, 1.9, 1, 3, 2, 1.7, 1.2, 1};
    double as[INPUT_SIZE] = {5, 3, 1, 1, 0, 0, 5, 4, 7, 2};
    double result = 0;
    for (int i = 0; i < INPUT_SIZE; i++)
    {
        result += as[i] * pow(x[i], exps[i]);
    }
    return result;
}

int main()
{
    double train_feature[INPUT_SIZE];
    double train_label;
    printf("x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,y\n");
    for (int i = 0; i < DATASET_SIZE; i++)
    {
        train_label = synthetic_function(train_feature);
        printf("%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n", train_feature[0], train_feature[1], train_feature[2], train_feature[3], train_feature[4], train_feature[5], train_feature[6], train_feature[7], train_feature[8], train_feature[9], train_label);
    }
    return 0;
}
