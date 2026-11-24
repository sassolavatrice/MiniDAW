#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Engine/AudioEngine.h"
#include "GUI/Widgets.h"

// =====================================================================
//  MainComponent — la finestra principale.
//
//  È un AudioAppComponent: JUCE apre la scheda audio e chiama
//  periodicamente getNextAudioBlock() sul thread audio. Noi giriamo
//  tutto all'AudioEngine.
// =====================================================================
class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // --- callback audio (chiamate da JUCE) ---
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // --- grafica ---
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;          // aggiorna il display del tempo
    void chooseFileForTrack (int trackIndex);
    void showAudioSettings();

    AudioEngine engine;

    // barra del trasporto
    juce::TextButton rewindButton { "|<<" }, playButton { "Play" }, stopButton { "Stop" };
    juce::TextButton settingsButton { "Audio..." };
    juce::Label timeLabel;
    Knob masterKnob { "Master", -60.0, 6.0, 0.0, " dB", 0.1 };

    std::array<std::unique_ptr<TrackStrip>, AudioEngine::maxTracks> strips;
    FilterPanel filterPanel { engine };
    DelayPanel  delayPanel  { engine };

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
