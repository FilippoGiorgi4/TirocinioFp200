# mkdir Release
# cd Release
# cmake -DCMAKE_BUILD_TYPE=Release ..


model_path=$1
type=$2
echo "Running the C++ TensorFlow Lite inference for the MNIST dataset with model $model_path and type $type"

cd ${HOME}/Tirocinio/tensorflow_lite_c/Release-New
make -j$(nproc)
#cmake -DCMAKE_BUILD_TYPE=Debug
cd ${HOME}/Tirocinio/

./tensorflow_lite_c/bin/tflite_inference $model_path > ${HOME}/Tirocinio/test/tflite_c_inftime_mnist_$type.csv
