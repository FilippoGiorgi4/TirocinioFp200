import numpy as np
import pandas as pd
import argparse

def remove_noise_from_mnist(csv_path, output_path, noise_level):
    # Carica il dataset
    df = pd.read_csv(csv_path)
    
    # Converti in numpy array
    data = df.to_numpy()
    
    # Genera rumore gaussiano
    noise = np.random.normal(loc=0.0, scale=noise_level, size=data.shape)
    
    # Rimuove il rumore e satura i valori negativi a 0
    cleaned_data = np.clip(data - noise, 0, 1)

    # Salva il dataset degradato
    pd.DataFrame(cleaned_data).to_csv(output_path, index=False)
    print(f"Dataset con rumore rimosso salvato in: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Rimuove rumore dal dataset MNIST")
    parser.add_argument("--noise_level", type=float, required=True, help="Livello di rumore da rimuovere")

    args = parser.parse_args()

    remove_noise_from_mnist("/home/fgiorgi/TirocinioFp200/mnist_test/x_test.csv", 
                            "/home/fgiorgi/TirocinioFp200/mnist_test/xdegradato_test.csv", 
                            args.noise_level)
