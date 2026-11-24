#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

// =====================================================================
//  DelayLine  —  effetto Delay / Echo (e, dal 1 dicembre, Chorus/Flanger)
//
//  Riferimento: lezione 18 "Effetti basati su DL" (Delay Line, comb IIR)
//
//  Un'istanza elabora UN canale. L'AudioEngine ne usa due (L e R).
//
//  Schema (comb IIR con feedback + miscelazione dry/wet):
//
//      x[n] ──►(+)──────────────┬──────────────► buffer (linea di ritardo)
//               ▲               │
//               │  g            │ z^-M
//               └──[×]◄─────────┴── d[n] = v[n-M]
//
//      v[n] = x[n] + g · v[n-M]          (ciò che scriviamo nel buffer)
//      d[n] = v[n-M]                     (ciò che leggiamo: il "wet")
//      y[n] = (1 - mix) · x[n] + mix · d[n]
//
//  Funzione di trasferimento del ramo wet:  H(z) = z^-M / (1 - g·z^-M)
//  → M poli sul cerchio di raggio |g|^(1/M): stabile solo se |g| < 1.
//  (Domanda d'esame tipica: "perché il feedback deve stare sotto 1?")
// =====================================================================
class DelayLine
{
public:
    /** Da chiamare prima di iniziare l'elaborazione (fuori dal thread audio
        o in prepareToPlay): alloca la memoria UNA volta sola. */
    void prepare (double newSampleRate, double maxDelaySeconds)
    {
        sampleRate = newSampleRate;
        // +2 campioni di margine per l'interpolazione
        buffer.assign ((size_t) std::ceil (maxDelaySeconds * sampleRate) + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        lfoPhase   = 0.0;
    }

    /** Parametri "musicali" → parametri in campioni.
        Chiamata dal thread audio all'inizio di ogni blocco. */
    void setParameters (float delayMs, float newFeedback, float newMix,
                        float newLfoRateHz, float lfoDepthMs)
    {
        const auto maxDelay = (float) buffer.size() - 2.0f;

        delaySamples     = std::clamp (delayMs * 0.001f * (float) sampleRate, 1.0f, maxDelay);
        feedback         = std::clamp (newFeedback, 0.0f, 0.99f);   // |g| < 1 → stabile
        mix              = std::clamp (newMix, 0.0f, 1.0f);
        lfoRateHz        = std::max (newLfoRateHz, 0.0f);
        lfoDepthSamples  = std::max (lfoDepthMs * 0.001f * (float) sampleRate, 0.0f);
    }

    /** Elabora un campione. È QUI che va scritto l'algoritmo. */
    float processSample (float x)
    {
        // -------------------------------------------------------------
        // TODO (lunedì 30 novembre) — delay con feedback
        //
        //   1. Calcola l'indice di lettura: writeIndex - delaySamples,
        //      riportato dentro [0, buffer.size()) (buffer CIRCOLARE).
        //      Per ora arrotonda delaySamples a intero.
        //   2. Leggi d = buffer[readIndex]
        //   3. Scrivi nel buffer v = x + feedback * d
        //   4. Avanza writeIndex (con il wrap-around)
        //   5. Restituisci (1 - mix) * x + mix * d
        //
        // TODO (martedì 1 dicembre) — chorus / flanger
        //
        //   6. Fai oscillare il ritardo con un LFO sinusoidale:
        //        M(n) = delaySamples + lfoDepthSamples * sin(lfoPhase)
        //      e aggiorna lfoPhase += 2π · lfoRateHz / sampleRate
        //   7. Ora M(n) non è intero → INTERPOLAZIONE LINEARE tra i due
        //      campioni vicini (stessa idea della lezione 16d, lettura dei
        //      campioni a velocità variabile).
        // -------------------------------------------------------------

        return x;   // segnaposto: per ora l'effetto lascia passare il segnale
    }

private:
    std::vector<float> buffer;
    int    writeIndex = 0;
    double sampleRate = 44100.0;

    float delaySamples    = 1.0f;
    float feedback        = 0.0f;
    float mix             = 0.0f;

    float  lfoRateHz       = 0.0f;
    float  lfoDepthSamples = 0.0f;
    double lfoPhase        = 0.0;
};
