#include "AudioEngine.h"

AudioEngine::AudioEngine()
{
    // WAV, AIFF, FLAC, Ogg Vorbis, MP3 (abilitato in CMakeLists.txt)
    formatManager.registerBasicFormats();
}

// =====================================================================
//  Preparazione (NON è il thread audio "caldo": qui si può allocare)
// =====================================================================
void AudioEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    masterBus.setSize (2, juce::jmax (maxBlockSize, 1));
    masterBus.clear();

    for (auto& d : delays)
        d.prepare (sampleRate, 2.5);   // fino a 2,5 s di ritardo

    for (auto& f : filters)
        f.reset();

    lastFilterType = -1;               // forza il ricalcolo dei coefficienti
}

// =====================================================================
//  Thread AUDIO: chiamata ogni blocco (es. 512 campioni ≈ 11,6 ms a 44,1 kHz)
// =====================================================================
void AudioEngine::process (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    const int numSamples = info.numSamples;

    // Caso raro: la scheda chiede un blocco più grande del previsto.
    if (numSamples > masterBus.getNumSamples())
        masterBus.setSize (2, numSamples, false, false, true);

    masterBus.clear (0, numSamples);

    // ---- 1. Mixer: somma delle tracce nel bus master ---------------
    if (playing)
    {
        const auto startPos = position.load();
        mixTracksInto (masterBus, numSamples, startPos);

        const auto newPos = startPos + numSamples;
        position = newPos;

        if (newPos >= length.load())
            playing = false;           // fine del brano: ci fermiamo
    }

    // ---- 2. Effetti sul bus master, campione per campione -----------
    //  (gli effetti girano anche da fermi: così la "coda" del delay
    //   continua a suonare dopo Stop, come in una DAW vera)
    updateEffectParameters();

    const bool filterOn = filterParams.enabled.load();
    const bool delayOn  = delayParams.enabled.load();

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = masterBus.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float s = data[i];

            if (filterOn) s = filters[(size_t) ch].processSample (s);
            if (delayOn)  s = delays [(size_t) ch].processSample (s);

            data[i] = s;
        }
    }

    // ---- 3. Master gain (con rampa per evitare "click") ---------------
    const float mg = masterGain.load();
    masterBus.applyGainRamp (0, numSamples, lastMasterGain, mg);
    lastMasterGain = mg;

    // ---- 4. Copia sull'uscita della scheda audio ---------------------
    auto& out = *info.buffer;

    if (out.getNumChannels() >= 2)
    {
        out.copyFrom (0, info.startSample, masterBus, 0, 0, numSamples);
        out.copyFrom (1, info.startSample, masterBus, 1, 0, numSamples);
    }
    else if (out.getNumChannels() == 1)   // uscita mono: (L + R) / 2
    {
        // (il buffer d'uscita è già stato azzerato all'inizio)
        out.addFrom (0, info.startSample, masterBus, 0, 0, numSamples, 0.5f);
        out.addFrom (0, info.startSample, masterBus, 1, 0, numSamples, 0.5f);
    }
}

void AudioEngine::mixTracksInto (juce::AudioBuffer<float>& bus, int numSamples, juce::int64 startPos)
{
    // Se il thread GUI sta scambiando un buffer (caricamento file) NON
    // aspettiamo: saltiamo il blocco (pochi ms di silenzio) piuttosto che
    // bloccare il thread audio.
    const juce::SpinLock::ScopedTryLockType lock (audioLock);

    if (! lock.isLocked())
        return;

    bool anySolo = false;
    for (auto& p : trackParams)
        anySolo = anySolo || p.solo.load();

    for (size_t t = 0; t < tracks.size(); ++t)
    {
        auto& track = tracks[t];
        auto& p     = trackParams[t];

        if (track.audio == nullptr)
            continue;

        const bool  audible = ! p.mute.load() && (! anySolo || p.solo.load());
        const float gain    = audible ? p.gain.load() : 0.0f;
        const float pan     = juce::jlimit (-1.0f, 1.0f, p.pan.load());

        // Legge di pan a POTENZA COSTANTE:
        //   θ = (pan + 1) · π/4  ∈ [0, π/2]
        //   gL = cos θ,  gR = sin θ   →  gL² + gR² = 1
        // Al centro entrambi valgono 0,707 (-3 dB): la potenza percepita
        // resta costante mentre si sposta il suono.
        const float theta = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gainL = gain * std::cos (theta);
        const float gainR = gain * std::sin (theta);

        if (track.justLoaded)   // traccia appena caricata: partiamo già dal guadagno giusto
        {
            track.lastGainL = gainL;
            track.lastGainR = gainR;
            track.justLoaded = false;
        }

        const auto trackLength = (juce::int64) track.audio->getNumSamples();

        if (startPos < trackLength)
        {
            const int count = (int) juce::jmin ((juce::int64) numSamples, trackLength - startPos);
            const int srcR  = track.audio->getNumChannels() > 1 ? 1 : 0;   // mono → su entrambi i lati

            // Rampa lineare dal guadagno del blocco precedente a quello
            // nuovo: evita il "zipper noise" quando muovi uno slider.
            bus.addFromWithRamp (0, 0, track.audio->getReadPointer (0,    (int) startPos), count, track.lastGainL, gainL);
            bus.addFromWithRamp (1, 0, track.audio->getReadPointer (srcR, (int) startPos), count, track.lastGainR, gainR);
        }

        track.lastGainL = gainL;
        track.lastGainR = gainR;
    }
}

void AudioEngine::updateEffectParameters()
{
    const double sr = currentSampleRate.load();

    // ---- Filtro: ricalcolo coefficienti solo se qualcosa è cambiato ----
    const int   type   = filterParams.type.load();
    const float freq   = juce::jlimit (20.0f, (float) (0.45 * sr), filterParams.freqHz.load());  // sotto Nyquist!
    const float q      = juce::jmax (0.1f, filterParams.q.load());
    const float gainDb = filterParams.gainDb.load();

    // (confronto esatto voluto: ci interessa sapere se il valore è stato toccato)
    if (type != lastFilterType || ! juce::exactlyEqual (freq, lastFreq)
        || ! juce::exactlyEqual (q, lastQ) || ! juce::exactlyEqual (gainDb, lastGainDb))
    {
        for (auto& f : filters)
            f.setParameters ((FilterType) type, freq, q, gainDb, sr);

        lastFilterType = type;
        lastFreq = freq; lastQ = q; lastGainDb = gainDb;
    }

    // ---- Delay: conversione ms → campioni (economica, ogni blocco) ----
    for (auto& d : delays)
        d.setParameters (delayParams.timeMs.load(),
                         delayParams.feedback.load(),
                         delayParams.mix.load(),
                         delayParams.lfoRateHz.load(),
                         delayParams.lfoDepthMs.load());

    // Quando un effetto viene riacceso, svuotiamo la sua memoria
    // (altrimenti risuonerebbe audio "vecchio").
    const bool filterOn = filterParams.enabled.load();
    const bool delayOn  = delayParams.enabled.load();

    if (filterOn && ! lastFilterEnabled) for (auto& f : filters) f.reset();
    if (delayOn  && ! lastDelayEnabled)  for (auto& d : delays)  d.reset();

    lastFilterEnabled = filterOn;
    lastDelayEnabled  = delayOn;
}

// =====================================================================
//  Thread GUI
// =====================================================================
void AudioEngine::play()
{
    if (position.load() >= length.load())
        position = 0;   // eravamo alla fine: ripartiamo dall'inizio

    playing = true;
}

juce::String AudioEngine::loadTrack (int trackIndex, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (trackIndex, maxTracks))
        return "Indice di traccia non valido";

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return "Impossibile leggere \"" + file.getFileName() + "\": formato non supportato o file danneggiato.";

    const double fileRate = reader->sampleRate;
    if (reader->lengthInSamples > (juce::int64) (fileRate * 60.0 * 15.0))
        return "File troppo lungo (massimo 15 minuti).";

    const int numChannels = juce::jmin (2, (int) reader->numChannels);
    const int fileLength  = (int) reader->lengthInSamples;

    // 1. Lettura dal disco (qualche campione di zeri in fondo serve
    //    all'interpolatore per non leggere fuori dal buffer)
    juce::AudioBuffer<float> fileBuffer (numChannels, fileLength + 8);
    fileBuffer.clear();
    reader->read (&fileBuffer, 0, fileLength, 0, true, true);

    // 2. Conversione della frequenza di campionamento, se necessario
    //    (es. file a 44,1 kHz, scheda audio a 48 kHz). Senza questo passo
    //    il brano suonerebbe più acuto/veloce (vedi lezione 16d: leggere
    //    i campioni a velocità diversa cambia il pitch).
    const double engineRate = currentSampleRate.load();
    const double ratio      = fileRate / engineRate;   // campioni letti per campione prodotto

    std::unique_ptr<juce::AudioBuffer<float>> newAudio;

    if (std::abs (ratio - 1.0) < 1.0e-9)
    {
        newAudio = std::make_unique<juce::AudioBuffer<float>> (numChannels, fileLength);
        for (int ch = 0; ch < numChannels; ++ch)
            newAudio->copyFrom (ch, 0, fileBuffer, ch, 0, fileLength);
    }
    else
    {
        const int outLength = (int) std::floor (fileLength / ratio);
        newAudio = std::make_unique<juce::AudioBuffer<float>> (numChannels, outLength);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;   // interpolazione polinomiale a 4 punti
            interpolator.process (ratio, fileBuffer.getReadPointer (ch), newAudio->getWritePointer (ch), outLength);
        }
    }

    // 3. Scambio "atomico" del buffer: il lock dura solo il tempo di uno swap
    {
        const juce::SpinLock::ScopedLockType lock (audioLock);
        std::swap (tracks[(size_t) trackIndex].audio, newAudio);
        tracks[(size_t) trackIndex].name = file.getFileNameWithoutExtension();
        tracks[(size_t) trackIndex].justLoaded = true;
    }

    // newAudio adesso contiene il VECCHIO buffer: viene liberato qui,
    // nel thread GUI e fuori dal lock (mai deallocare nel thread audio).
    newAudio.reset();

    recomputeLength();
    return {};
}

void AudioEngine::recomputeLength()
{
    juce::int64 maxLen = 0;

    for (auto& t : tracks)
        if (t.audio != nullptr)
            maxLen = juce::jmax (maxLen, (juce::int64) t.audio->getNumSamples());

    length = maxLen;
}

juce::String AudioEngine::getTrackName (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, maxTracks) ? tracks[(size_t) trackIndex].name : juce::String();
}

juce::String AudioEngine::getSupportedWildcards() const
{
    return formatManager.getWildcardForAllFormats();
}

double AudioEngine::getPositionSeconds() const
{
    return (double) juce::jmin (position.load(), length.load()) / currentSampleRate.load();
}

double AudioEngine::getLengthSeconds() const
{
    return (double) length.load() / currentSampleRate.load();
}
