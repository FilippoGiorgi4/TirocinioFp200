import tensorflow as tf
import numpy as np

# get mnist data
mnist = tf.keras.datasets.mnist
(x_train, y_train), (x_val, y_val) = mnist.load_data()

# Normalize the x data to be between 0 and 1
x_train, x_val = x_train / 255.0, x_val / 255.0

# Expand the dimensions of x data to include the channel dimension
x_train = np.expand_dims(x_train, axis=-1)
x_val = np.expand_dims(x_val, axis=-1)

# Save the validation images to x_test.csv
np.savetxt("x_test.csv", x_val.reshape(-1, 28 * 28), delimiter=",")

# Save the validation labels to y_test.csv
np.savetxt("y_test.csv", y_val, delimiter=",", fmt='%d')