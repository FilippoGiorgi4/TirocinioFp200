# gcc -march=native -O3 -o tflite_times tflite_times.c -L ~/tflite_build -I ~/tensorflow_src -ltensorflow-lite -lm
# ./tflite_times #> tflite_times.csv
# rm ./tflite_times

# TYPE=$1
# BUILDDIR=build-$TYPE

# if [ ! -d $BUILDDIR ]; then
#     mkdir $BUILDDIR
# fi

# cd $BUILDDIR
cd build
make -j$(nproc-1)
#cmake -DCMAKE_BUILD_TYPE=Debug
cd ..

./bin/tflite_times > tflite_times.csv
