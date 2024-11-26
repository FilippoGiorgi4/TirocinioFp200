
cd regression_test

gcc -march=native -O3 -o data_generator data_generator.c -lm
./data_generator > data.csv
rm ./data_generator
