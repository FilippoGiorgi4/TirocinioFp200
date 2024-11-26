import tensorflow as tf
import numpy as np
import os

# get mnist data
mnist = tf.keras.datasets.mnist
(x_train, y_train), (x_val, y_val) = mnist.load_data()
x_train, x_val = x_train / 255.0, x_val / 255.0
x_train = np.expand_dims(x_train, axis=-1)
x_val = np.expand_dims(x_val, axis=-1)

np.savetxt("x_test.csv", x_val.reshape(-1, 28 * 28), delimiter=",")
