import numpy as np
import pandas as pd
import argparse

def mask_fixed_percentage(csv_path, output_path, mask_percentage):
    # Carica il dataset
    df = pd.read_csv(csv_path)
    
    # Converti in numpy array
    data = df.to_numpy()
    
    # Numero di pixel per immagine (MNIST ha 28x28 = 784 pixel)
    num_pixels = data.shape[1]
    num_pixels_to_mask = int(mask_percentage * num_pixels)

    # Per ogni immagine, scegli casualmente quali pixel azzerare
    for i in range(data.shape[0]):
        mask_indices = np.random.choice(num_pixels, num_pixels_to_mask, replace=False)
        data[i, mask_indices] = 0  # Imposta a 0 i pixel selezionati

    # Salva il dataset modificato
    pd.DataFrame(data).to_csv(output_path, index=False)
    print(f"Dataset con {mask_percentage*100:.0f}% pixel rimossi salvato in: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Elimina una percentuale fissa di pixel dalle immagini MNIST")
    parser.add_argument("--mask_percentage", type=float, required=True, help="Percentuale di pixel da eliminare (0-1)")

    args = parser.parse_args()

    mask_fixed_percentage("/home/fgiorgi/TirocinioFp200/mnist_test/x_test.csv", 
                          "/home/fgiorgi/TirocinioFp200/mnist_test/xmasked_test.csv", 
                          args.mask_percentage)
