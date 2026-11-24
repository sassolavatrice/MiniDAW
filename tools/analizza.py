#!/usr/bin/env python3
"""
Strumenti per verificare gli effetti della MiniDAW (e per avere grafici
da mostrare all'orale).

  python3 tools/analizza.py genera
      Crea in test/ i segnali di prova: impulse.wav, noise.wav, sine440.wav

  python3 tools/analizza.py impulso test/out.wav
      Grafico della risposta all'impulso (utile per il DELAY:
      devi vedere gli echi ogni M campioni, che decadono di un fattore g)

  python3 tools/analizza.py risposta test/noise.wav test/out.wav
      Risposta in frequenza misurata ingresso → uscita (utile per il FILTRO:
      confrontala con quella teorica del biquad)

Serve: pip install numpy scipy matplotlib
"""
import sys
import os
import numpy as np
from scipy.io import wavfile
from scipy import signal


def leggi(path):
    sr, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x.astype(np.float64) / np.iinfo(x.dtype).max
    if x.ndim > 1:
        x = x[:, 0]  # canale sinistro
    return sr, x


def scrivi(path, x, sr):
    wavfile.write(path, sr, (np.clip(x, -1, 1) * 32767).astype(np.int16))


def genera(sr=48000):
    os.makedirs("test", exist_ok=True)
    imp = np.zeros(sr * 2)
    imp[0] = 1.0
    scrivi("test/impulse.wav", imp, sr)
    rng = np.random.default_rng(0)
    scrivi("test/noise.wav", 0.25 * rng.standard_normal(sr * 3), sr)
    t = np.arange(sr * 2) / sr
    scrivi("test/sine440.wav", 0.5 * np.sin(2 * np.pi * 440 * t), sr)
    print("Creati test/impulse.wav, test/noise.wav, test/sine440.wav")


def impulso(path):
    import matplotlib.pyplot as plt
    sr, y = leggi(path)
    picchi = np.where(np.abs(y) > 1e-3)[0][:10]
    print("Primi campioni non nulli (indice → ms → valore):")
    for i in picchi:
        print(f"  {i:7d}  {1000 * i / sr:8.2f} ms   {y[i]:+.4f}")
    t = np.arange(len(y)) / sr * 1000
    plt.plot(t, y)
    plt.xlabel("tempo [ms]")
    plt.ylabel("ampiezza")
    plt.title("Risposta all'impulso")
    plt.grid(True)
    plt.show()


def risposta(path_in, path_out):
    import matplotlib.pyplot as plt
    sr, x = leggi(path_in)
    _, y = leggi(path_out)
    n = min(len(x), len(y))
    x, y = x[:n], y[:n]
    # stima H(f) = Sxy / Sxx  (metodo di Welch)
    f, Sxy = signal.csd(x, y, sr, nperseg=8192)
    _, Sxx = signal.welch(x, sr, nperseg=8192)
    H = 20 * np.log10(np.abs(Sxy / Sxx) + 1e-12)
    # il pan al centro attenua di 3 dB (legge a potenza costante): lo compensiamo
    H += 20 * np.log10(np.sqrt(2))
    plt.semilogx(f[1:], H[1:])
    plt.xlabel("frequenza [Hz]")
    plt.ylabel("|H(f)| [dB]")
    plt.title("Risposta in frequenza misurata")
    plt.grid(True, which="both")
    plt.xlim(20, sr / 2)
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
    elif sys.argv[1] == "genera":
        genera()
    elif sys.argv[1] == "impulso" and len(sys.argv) == 3:
        impulso(sys.argv[2])
    elif sys.argv[1] == "risposta" and len(sys.argv) == 4:
        risposta(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
