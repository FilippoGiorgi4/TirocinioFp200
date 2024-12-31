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
COPY . .

# Sposta libtensorflowlite_c.so e imposta LD_LIBRARY_PATH
RUN mv /usr/src/app/deploy_methods/tensorflow_lite_c/mnist/libtensorflowlite_c.so /usr/local/lib/ \
    && echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> /etc/profile.d/ld_library_path.sh
