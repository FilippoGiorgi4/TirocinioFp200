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

