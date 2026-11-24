# MiniDAW

DAW minimale in JUCE per il progetto di Rielaborazione dei Suoni.

- 4 tracce audio (WAV, AIFF, FLAC, OGG, MP3) con volume, pan, mute e solo
- trasporto: play, stop, riavvolgi
- effetti sul bus master: **filtro biquad** (passa-basso, passa-alto, peak, shelving) → **delay** (con LFO per chorus/flanger)
- `MiniDAWRender`: lo stesso motore audio da riga di comando, che scrive un file WAV

```
Traccia 1..4 ─[volume]─[pan]─► Σ bus master ─► Filtro ─► Delay ─► Master ─► uscita
```

---

## 1. Installazione (Linux)

### Pacchetti

**Ubuntu / Debian / Mint**
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git \
  libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev
```

**Fedora**
```bash
sudo dnf install gcc-c++ cmake ninja-build git alsa-lib-devel \
  jack-audio-connection-kit-devel freetype-devel fontconfig-devel libX11-devel \
  libXcomposite-devel libXcursor-devel libXext-devel libXinerama-devel \
  libXrandr-devel libXrender-devel libXi-devel mesa-libGL-devel
```

**Arch / Manjaro**
```bash
sudo pacman -S base-devel cmake ninja git alsa-lib jack2 freetype2 fontconfig \
  libx11 libxcomposite libxcursor libxext libxinerama libxrandr libxrender libxi mesa
```

### Compilazione

```bash
cd MiniDAW
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

La prima volta CMake scarica JUCE 9.0.6 da GitHub (circa 1 minuto) e la
compilazione dura qualche minuto. Le volte successive ricompila solo i file che modifichi.

Se hai già JUCE sul disco: `cmake -B build -G Ninja -DJUCE_DIR=/percorso/JUCE`

### Avvio

```bash
./build/MiniDAW_artefacts/Debug/MiniDAW
```

Se non senti niente, apri **Audio...** e scegli un altro dispositivo d'uscita.
Con PipeWire o PulseAudio di solito funziona il dispositivo ALSA "default" (oppure "pipewire").

> **Debug o Release?** Per lavorare usa `Debug`: JUCE controlla molti errori
> (asserzioni) e il debugger funziona bene. Per la consegna puoi compilare anche in `Release`.

### Editor consigliato

VS Code con le estensioni *C/C++* e *CMake Tools*: apri la cartella e fa tutto da solo.
CMake genera `build/compile_commands.json`, quindi anche l'autocompletamento funziona.
In alternativa CLion apre il progetto direttamente.

---

## 2. Com'è organizzato il codice

```
Source/
├── Main.cpp              avvio dell'app e finestra (codice standard JUCE)
├── MainComponent.h/.cpp  finestra principale: collega la GUI e la scheda audio all'engine
├── GUI/Widgets.h         manopole, strisce del mixer, pannelli degli effetti
├── Engine/
│   ├── AudioEngine.h     ★ il motore audio: tracce, mixer, trasporto, catena effetti
│   └── AudioEngine.cpp
├── DSP/
│   ├── DelayLine.h       ✎ DA COMPLETARE: delay / chorus / flanger
│   └── Biquad.h          ✎ DA COMPLETARE: filtro IIR del 2° ordine
└── RenderMain.cpp        renderer offline da riga di comando
tools/analizza.py         genera segnali di prova e mostra i grafici di risposta
```

### Cosa c'è già e cosa scrivi tu

| Parte | Stato |
|---|---|
| Caricamento file, conversione di sample rate | ✅ fatto |
| Mixer (volume in dB, pan a potenza costante, mute/solo) | ✅ fatto |
| Trasporto, GUI, impostazioni audio | ✅ fatto |
| Renderer offline | ✅ fatto |
| **Delay con feedback** (`DelayLine::processSample`) | ✎ lunedì 30/11 |
| **Chorus/flanger** (LFO + interpolazione) | ✎ martedì 1/12 |
| **Coefficienti del biquad** (`Biquad::setParameters`) | ✎ mercoledì 2/12 |
| **Equazione alle differenze** (`Biquad::processSample`) | ✎ mercoledì 2/12 |

Finché non li completi, gli effetti lasciano passare il segnale invariato,
quindi l'app funziona comunque. Ogni TODO nel codice spiega i passi e indica la slide di riferimento.

### Le due regole del thread audio (domanda d'esame probabile)

`AudioEngine::process()` gira sul **thread audio**, che deve consegnare un blocco
ogni pochi millisecondi. Se arriva in ritardo, si sente un "click" (*dropout*). Quindi lì dentro:

1. **niente allocazioni di memoria**: i buffer si allocano in `prepare()`;
2. **niente attese**: niente lock bloccanti, niente lettura di file.

Per questo i parametri sono `std::atomic` (la GUI scrive, l'audio legge) e il caricamento di un file
scambia il buffer con uno `SpinLock` tenuto solo per il tempo di uno `std::swap`.

---

## 3. Provare gli effetti senza scheda audio

```bash
pip install numpy scipy matplotlib        # una volta sola
python3 tools/analizza.py genera          # crea test/impulse.wav, noise.wav, sine440.wav
R=./build/MiniDAWRender_artefacts/Debug/MiniDAWRender

# DELAY: la risposta all'impulso deve mostrare echi ogni 100 ms, ognuno metà del precedente
$R -o test/out_delay.wav --delay 100,0.5,0.5 --tail 1 test/impulse.wav
python3 tools/analizza.py impulso test/out_delay.wav

# FILTRO: risposta in frequenza misurata (qui: campana +9 dB a 2 kHz)
$R -o test/out_peak.wav --filter peak,2000,1.5,9 --tail 0 test/noise.wav
python3 tools/analizza.py risposta test/noise.wav test/out_peak.wav
```

`$R --help` mostra tutte le opzioni.
I grafici che ottieni sono ottimi da mostrare all'orale: "ecco la risposta all'impulso
del mio delay, ecco la risposta in frequenza del mio filtro".

---

## 4. Possibili estensioni (se avanza tempo)

- Distorsione `tanh` / waveshaping (lezioni 16b e 19), eventualmente con oversampling per l'aliasing
- Export del mix in WAV dalla GUI (il codice di `RenderMain.cpp` è già un buon punto di partenza)
- Indicatore di livello (VU meter) sul master
- Visualizzazione della risposta in frequenza del filtro nella GUI
