# Usa una base image di gcc
FROM gcc:latest

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
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Installazione di pip
RUN python3 -m pip install --upgrade pip --break-system-packages

# Installazione di Bazel
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel-archive-keyring.gpg && \
    mv bazel-archive-keyring.gpg /usr/share/keyrings && \
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list && \
    apt update && apt install -y bazel-6.5.0 && \
    apt update && apt full-upgrade -y && \
    #apt install bazel-6.5.0
    ln -s /usr/bin/bazel-6.5.0 /usr/bin/bazel

# Installazione di Clang
RUN wget https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.2/clang+llvm-17.0.2-x86_64-linux-gnu-ubuntu-22.04.tar.xz && \
    tar -xvf clang+llvm-17.0.2-x86_64-linux-gnu-ubuntu-22.04.tar.xz && \
    cp -r clang+llvm-17.0.2-x86_64-linux-gnu-ubuntu-22.04/* /usr

# Clonazione del repository TensorFlow
RUN git clone https://github.com/tensorflow/tensorflow.git 
# Copia lo script di configurazione
COPY configure_tensorflow.sh /tensorflow/configure_tensorflow.sh

# Esegui lo script di configurazione
RUN cd /tensorflow && chmod +x configure_tensorflow.sh && ./configure_tensorflow.sh && \
    bazel build //tensorflow/tools/pip_package:wheel --repo_env=WHEEL_NAME=tensorflow_cpu

# Installazione di TensorFlow
RUN pip install /tensorflow/bazel-bin/tensorflow/tools/pip_package/wheel_house/tensorflow_cpu-2.19.0-cp310-cp310-linux_x86_64.whl

# Copia il codice sorgente nella directory di lavoro
WORKDIR /usr/src/app
COPY . .

# Compila il programma, modificare e capire se necessario
RUN gcc -o tlife_inference tlife_inference.c -I/tensorflow -L/tensorflow/bazel-bin/tensorflow/lite -ltensorflowlite -lm

# Definisce il comando per eseguire il programma, scrivere
CMD ["./tlife_inference", "model.tflite", "x_test.csv"]
