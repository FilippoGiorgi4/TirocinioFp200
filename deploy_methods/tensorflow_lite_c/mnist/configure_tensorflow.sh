#!/bin/bash

cd /tensorflow

# Utilizza here document per fornire risposte predefinite a ./configure
#Risposte alle domande di ./configure per TensorFlow
#1. Python location?
#2. Python library path to use, riga vuota==invio==default
#3. TensorFlow with ROCm support? [Y/n]
#4. CUDA support? [Y/n]
#5. Clang to build TensorFlow? [Y/n]
#6. specify the path to clang executable. [Default is /usr/bin/clang]
#7. optimization flags to use during compilation when bazel option "--config=opt", riga vuota==invio==default
#8. configure ./WORKSPACE for Android builds? [y/N]
#9. TensorFlow with iOS support? [y/N]

./configure <<EOF
/usr/bin/python3
/usr/lib/python3/dist-packages
n
n
Y
/usr/bin/clang
-Wno-sign-compare
n
n
EOF