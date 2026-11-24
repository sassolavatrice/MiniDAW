#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <memory>

#include "DSP/Biquad.h"
#include "DSP/DelayLine.h"

// =====================================================================
//  AudioEngine — il "cuore" della DAW, separato dalla grafica.
//
//  Catena del segnale (per ogni blocco di campioni):
//
//   Traccia 1 ─[gain]─[pan]─┐
//   Traccia 2 ─[gain]─[pan]─┤
//   Traccia 3 ─[gain]─[pan]─┼─► Σ (bus master stereo) ─► Filtro ─► Delay ─► Master gain ─► uscita
//   Traccia 4 ─[gain]─[pan]─┘
//
//  DUE THREAD lavorano su questa classe:
//   - il thread della GUI (message thread): carica file, muove i parametri
//   - il thread AUDIO: chiama process() ogni pochi millisecondi
//
//  Regole d'oro del thread audio (domanda d'esame probabile!):
//   1. niente allocazioni di memoria (new, vector::push_back, ...)
//   2. niente lock bloccanti, niente I/O su disco
//   → i parametri sono std::atomic, la memoria è allocata in prepare().
// =====================================================================
class AudioEngine
{
public:
    static constexpr int maxTracks = 4;

    // ---------------------------------------------------------------
    //  Parametri: scritti dalla GUI, letti dal thread audio
    // ---------------------------------------------------------------
    struct TrackParams
    {
        std::atomic<float> gain { 1.0f };    // lineare (1 = 0 dB)
        std::atomic<float> pan  { 0.0f };    // -1 = sinistra, 0 = centro, +1 = destra
        std::atomic<bool>  mute { false };
        std::atomic<bool>  solo { false };
    };

    struct FilterParams
    {
        std::atomic<bool>  enabled { false };
        std::atomic<int>   type    { (int) FilterType::LowPass };
        std::atomic<float> freqHz  { 1000.0f };
        std::atomic<float> q       { 0.707f };
        std::atomic<float> gainDb  { 0.0f };
    };

    struct DelayParams
    {
        std::atomic<bool>  enabled    { false };
        std::atomic<float> timeMs     { 350.0f };
        std::atomic<float> feedback   { 0.4f };
        std::atomic<float> mix        { 0.35f };
        std::atomic<float> lfoRateHz  { 0.0f };   // 0 = delay classico
        std::atomic<float> lfoDepthMs { 0.0f };
    };

    std::array<TrackParams, maxTracks> trackParams;
    FilterParams filterParams;
    DelayParams  delayParams;
    std::atomic<float> masterGain { 1.0f };

    // ---------------------------------------------------------------
    AudioEngine();

    /** Chiamata quando la scheda audio parte (o cambia sample rate). */
    void prepare (double sampleRate, int maxBlockSize);

    /** Chiamata dal thread audio: riempie il buffer d'uscita. */
    void process (const juce::AudioSourceChannelInfo& info);

    // --- gestione tracce (thread GUI) ---
    /** Carica un file nella traccia indicata. Restituisce un messaggio
        d'errore vuoto se è andato tutto bene. */
    juce::String loadTrack (int trackIndex, const juce::File& file);
    juce::String getTrackName (int trackIndex) const;
    juce::String getSupportedWildcards() const;

    // --- trasporto ---
    void play();
    void stop()    { playing = false; }
    void rewind()  { position = 0; }
    bool isPlaying() const          { return playing; }
    double getPositionSeconds() const;
    double getLengthSeconds() const;
    double getSampleRate() const    { return currentSampleRate.load(); }

private:
    struct Track
    {
        std::unique_ptr<juce::AudioBuffer<float>> audio;   // protetto da audioLock
        juce::String name;
        float lastGainL = 0.0f, lastGainR = 0.0f;          // solo thread audio (per le rampe)
        bool  justLoaded = false;                          // primo blocco: niente rampa
    };

    void updateEffectParameters();
    void mixTracksInto (juce::AudioBuffer<float>& bus, int numSamples, juce::int64 startPos);
    void recomputeLength();

    juce::AudioFormatManager formatManager;

    std::array<Track, maxTracks> tracks;
    juce::SpinLock audioLock;   // protegge SOLO lo scambio dei buffer audio delle tracce

    juce::AudioBuffer<float> masterBus;   // allocato in prepare()

    std::atomic<bool>        playing  { false };
    std::atomic<juce::int64> position { 0 };   // in campioni
    std::atomic<juce::int64> length   { 0 };   // traccia più lunga, in campioni
    std::atomic<double>      currentSampleRate { 44100.0 };

    // effetti: uno per canale (L, R)
    std::array<Biquad, 2>    filters;
    std::array<DelayLine, 2> delays;

    // copia locale degli ultimi parametri del filtro: ricalcoliamo i
    // coefficienti solo quando cambiano (sono costosi: sin, cos, pow)
    int   lastFilterType = -1;
    float lastFreq = -1.0f, lastQ = -1.0f, lastGainDb = -999.0f;
    float lastMasterGain = 1.0f;
    bool  lastFilterEnabled = false, lastDelayEnabled = false;

    JUCE_DECLARE_NON_COPYABLE (AudioEngine)
};
