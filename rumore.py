import numpy as np
import pandas as pd
import argparse

def add_noise_to_mnist(csv_path, output_path, noise_level):
    # Carica il dataset
    df = pd.read_csv(csv_path)
    
    # Converti in numpy array
    data = df.to_numpy()
    
    # Aggiungi rumore gaussiano
    noise = np.random.normal(loc=0.0, scale=noise_level, size=data.shape)
    noisy_data = np.clip(data + noise, 0, 1)  # Mantieni i valori nel range [0,1]
    
    # Salva il dataset con rumore
    pd.DataFrame(noisy_data).to_csv(output_path, index=False)
    print(f"Dataset con rumore salvato in: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Aggiunge rumore gaussiano al dataset MNIST")
    parser.add_argument("--noise_level", type=float, required=True, help="Livello di rumore da applicare")
    
    args = parser.parse_args()
    
    add_noise_to_mnist("/home/fgiorgi/TirocinioFp200/mnist_test/x_test.csv", 
                        "/home/fgiorgi/TirocinioFp200/mnist_test/xrumore_test.csv", 
                        args.noise_level)
