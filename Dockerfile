# Usa una base image di TensorFlow che include già Bazel e altre dipendenze
FROM tensorflow/tensorflow:latest

# Installazione delle dipendenze di sistema
RUN apt-get update && apt-get install -y \
    apt-transport-https \
    gnupg \
    wget \
    curl \
    build-essential \
    python3-dev \
    python3-pip \
    unzip \
    sudo \
    patchelf \
    libjson-c-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Installazione di pip
RUN python3 -m pip install --upgrade pip --break-system-packages

# Copia il codice sorgente nella directory di lavoro
WORKDIR /usr/src/app
COPY /deploy_methods/tensorflow_lite_c/mnist/bin/server /usr/src/app/server
COPY /tflite_models/model_mnist_small.tflite /usr/src/app/model_mnist_small.tflite
COPY /deploy_methods/tensorflow_lite_c/mnist/libtensorflowlite_c.so /usr/local/lib

RUN ldconfig
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Sposta libtensorflowlite_c.so e imposta LD_LIBRARY_PATH
RUN echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> /etc/profile.d/ld_library_path.sh

#Lancia file 
CMD ["/usr/src/app/server", "/usr/src/app/model_mnist_small.tflite"]
