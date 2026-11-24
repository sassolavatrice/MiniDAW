// =====================================================================
//  MiniDAWRender — renderer offline da riga di comando
//
//  Usa lo STESSO AudioEngine dell'app grafica, ma invece di mandare
//  l'audio alla scheda lo scrive in un file WAV. Utile per:
//   - provare gli effetti senza scheda audio
//   - confrontare ingresso e uscita in Audacity (spettro, forma d'onda)
//
//  Esempi:
//    MiniDAWRender -o out.wav batteria.wav basso.wav
//    MiniDAWRender -o out.wav --delay 350,0.5,0.4 voce.wav
//    MiniDAWRender -o out.wav --filter lp,800,0.707 --tail 3 synth.wav
//    MiniDAWRender -o out.wav --delay 5,0.3,0.5,0.4,3 chitarra.wav   (flanger)
// =====================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

#include "Engine/AudioEngine.h"

static void printUsage()
{
    std::cout <<
        "Uso: MiniDAWRender -o uscita.wav [opzioni] traccia1 [traccia2 ... traccia4]\n\n"
        "Opzioni:\n"
        "  -o FILE                       file WAV di uscita (24 bit, stereo)\n"
        "  --sr HZ                       frequenza di campionamento (default 48000)\n"
        "  --filter TIPO,FREQ,Q[,GAIN]   TIPO = lp | hp | peak | ls | hs ; GAIN in dB\n"
        "  --delay MS,FB,MIX[,LFOHZ,PROFMS]   delay (+ modulazione per chorus/flanger)\n"
        "  --gain T,DB                   guadagno della traccia T (1-4) in dB\n"
        "  --pan  T,P                    pan della traccia T, P in [-1, 1]\n"
        "  --tail S                      secondi extra in fondo per le code (default 2)\n";
}

static FilterType parseFilterType (const juce::String& s)
{
    if (s == "hp")   return FilterType::HighPass;
    if (s == "peak") return FilterType::Peak;
    if (s == "ls")   return FilterType::LowShelf;
    if (s == "hs")   return FilterType::HighShelf;
    return FilterType::LowPass;
}

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args.isEmpty() || args.contains ("-h") || args.contains ("--help"))
    {
        printUsage();
        return args.isEmpty() ? 1 : 0;
    }

    double sampleRate = 48000.0;
    double tailSeconds = 2.0;
    juce::File outFile;
    juce::StringArray inputs;

    AudioEngine engine;
    juce::StringArray deferred;   // opzioni da applicare dopo prepare()

    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        const bool hasValue = i + 1 < args.size();

        if      (a == "-o"   && hasValue) outFile     = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
        else if (a == "--sr" && hasValue) sampleRate  = args[++i].getDoubleValue();
        else if (a == "--tail" && hasValue) tailSeconds = args[++i].getDoubleValue();
        else if ((a == "--filter" || a == "--delay" || a == "--gain" || a == "--pan") && hasValue)
        {
            deferred.add (a);
            deferred.add (args[++i]);
        }
        else if (a.startsWith ("-"))
        {
            std::cerr << "Opzione sconosciuta: " << a << "\n";
            printUsage();
            return 1;
        }
        else
            inputs.add (a);
    }

    if (outFile == juce::File() || inputs.isEmpty())
    {
        printUsage();
        return 1;
    }

    constexpr int blockSize = 512;
    engine.prepare (sampleRate, blockSize);

    // ---- parametri ----------------------------------------------------
    for (int i = 0; i < deferred.size(); i += 2)
    {
        const auto& opt = deferred[i];
        juce::StringArray v;
        v.addTokens (deferred[i + 1], ",", "");

        if (opt == "--filter" && v.size() >= 3)
        {
            engine.filterParams.enabled = true;
            engine.filterParams.type    = (int) parseFilterType (v[0]);
            engine.filterParams.freqHz  = v[1].getFloatValue();
            engine.filterParams.q       = v[2].getFloatValue();
            engine.filterParams.gainDb  = v.size() > 3 ? v[3].getFloatValue() : 0.0f;
        }
        else if (opt == "--delay" && v.size() >= 3)
        {
            engine.delayParams.enabled    = true;
            engine.delayParams.timeMs     = v[0].getFloatValue();
            engine.delayParams.feedback   = v[1].getFloatValue();
            engine.delayParams.mix        = v[2].getFloatValue();
            engine.delayParams.lfoRateHz  = v.size() > 3 ? v[3].getFloatValue() : 0.0f;
            engine.delayParams.lfoDepthMs = v.size() > 4 ? v[4].getFloatValue() : 0.0f;
        }
        else if ((opt == "--gain" || opt == "--pan") && v.size() == 2)
        {
            const int t = v[0].getIntValue() - 1;
            if (juce::isPositiveAndBelow (t, AudioEngine::maxTracks))
            {
                if (opt == "--gain") engine.trackParams[(size_t) t].gain = juce::Decibels::decibelsToGain (v[1].getFloatValue());
                else                 engine.trackParams[(size_t) t].pan  = v[1].getFloatValue();
            }
        }
        else
        {
            std::cerr << "Valori non validi per " << opt << ": " << deferred[i + 1] << "\n";
            return 1;
        }
    }

    // ---- tracce -------------------------------------------------------
    for (int t = 0; t < juce::jmin (inputs.size(), AudioEngine::maxTracks); ++t)
    {
        const auto file  = juce::File::getCurrentWorkingDirectory().getChildFile (inputs[t]);
        const auto error = engine.loadTrack (t, file);

        if (error.isNotEmpty())
        {
            std::cerr << "Traccia " << (t + 1) << ": " << error << "\n";
            return 1;
        }
        std::cout << "Traccia " << (t + 1) << ": " << file.getFileName() << "\n";
    }

    // ---- rendering a blocchi, esattamente come farebbe la scheda audio ----
    const int total = (int) std::ceil ((engine.getLengthSeconds() + tailSeconds) * sampleRate);
    juce::AudioBuffer<float> block (2, blockSize);
    juce::AudioBuffer<float> result (2, total);

    engine.play();

    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, total - pos);
        juce::AudioSourceChannelInfo info (&block, 0, n);
        engine.process (info);

        for (int ch = 0; ch < 2; ++ch)
            result.copyFrom (ch, pos, block, ch, 0, n);
    }

    // ---- scrittura WAV -----------------------------------------------
    outFile.deleteFile();
    auto fileStream = std::make_unique<juce::FileOutputStream> (outFile);

    if (fileStream->failedToOpen())
    {
        std::cerr << "Impossibile scrivere " << outFile.getFullPathName() << "\n";
        return 1;
    }

    std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (sampleRate)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (24));
    if (writer == nullptr)
    {
        std::cerr << "Impossibile creare il file WAV\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer (result, 0, total);
    writer.reset();

    const float peak = result.getMagnitude (0, total);
    std::cout << "Scritto " << outFile.getFullPathName() << "  ("
              << juce::String (total / sampleRate, 2) << " s, picco "
              << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS)\n";

    if (peak > 1.0f)
        std::cout << "Attenzione: il segnale supera 0 dBFS (clipping). Abbassa i guadagni.\n";

    return 0;
}
