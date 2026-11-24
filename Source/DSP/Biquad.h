#pragma once

#include <cmath>

// =====================================================================
//  Biquad  —  filtro IIR del secondo ordine (EQ / filtro)
//
//  Riferimenti:
//   - lezione 17 "Filtri per mastering": filtri canonici, Q, shelving, peak
//   - lezioni 8-11 e 4-7: funzione di trasferimento, forma diretta II
//     (forma CANONICA), poli/zeri, stabilità
//
//  Funzione di trasferimento (coefficienti normalizzati, a0 = 1):
//
//              b0 + b1·z^-1 + b2·z^-2
//      H(z) = ------------------------
//               1 + a1·z^-1 + a2·z^-2
//
//  Equazione alle differenze in forma diretta II (canonica):
//
//      w[n] = x[n] - a1·w[n-1] - a2·w[n-2]
//      y[n] = b0·w[n] + b1·w[n-1] + b2·w[n-2]
//
//  Un'istanza elabora UN canale. L'AudioEngine ne usa due (L e R).
// =====================================================================

enum class FilterType
{
    LowPass = 1,
    HighPass,
    Peak,
    LowShelf,
    HighShelf
};

class Biquad
{
public:
    /** Calcola i coefficienti a partire dai parametri "musicali".
        Chiamata dal thread audio solo quando un parametro cambia. */
    void setParameters ([[maybe_unused]] FilterType type,
                        [[maybe_unused]] double freqHz,
                        [[maybe_unused]] double Q,
                        [[maybe_unused]] double gainDb,
                        [[maybe_unused]] double sampleRate)
    {
        // -------------------------------------------------------------
        // TODO (mercoledì 2 dicembre) — calcolo dei coefficienti
        //
        //   Grandezze comuni:
        //     w0    = 2π · freqHz / sampleRate      (frequenza normalizzata)
        //     alpha = sin(w0) / (2·Q)
        //     A     = 10^(gainDb / 40)              (solo peak / shelving)
        //
        //   Poi, per ogni tipo, b0 b1 b2 a0 a1 a2 dalle formule
        //   dell'"Audio EQ Cookbook" (R. Bristow-Johnson) — le vediamo
        //   insieme e le colleghiamo alle slide della lezione 17.
        //
        //   Infine NORMALIZZA dividendo tutto per a0.
        // -------------------------------------------------------------

        // segnaposto: filtro "passa-tutto banale" (y = x)
        b0 = 1.0; b1 = 0.0; b2 = 0.0;
        a1 = 0.0; a2 = 0.0;
    }

    void reset()
    {
        w1 = 0.0;
        w2 = 0.0;
    }

    float processSample (float x)
    {
        // -------------------------------------------------------------
        // TODO (mercoledì 2 dicembre) — equazione alle differenze
        //
        //   Implementa la forma diretta II scritta qui sopra usando lo
        //   stato w1 = w[n-1], w2 = w[n-2], poi aggiorna lo stato.
        //   (Sono 4 righe. All'orale saprai dire riga per riga cosa fanno.)
        // -------------------------------------------------------------

        return x;   // segnaposto
    }

private:
    // coefficienti (a0 normalizzato a 1)
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    // stato della forma diretta II: w[n-1], w[n-2]
    double w1 = 0.0, w2 = 0.0;
};
