#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "Engine/AudioEngine.h"

// =====================================================================
//  Componenti grafici riutilizzabili. Ogni controllo scrive il proprio
//  valore in un parametro std::atomic dell'AudioEngine: la GUI non tocca
//  MAI direttamente l'audio.
// =====================================================================

/** Manopola rotativa con un'etichetta sopra. */
class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, double min, double max, double initial,
          const juce::String& suffix = {}, double step = 0.0)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        slider.setRange (min, max, step);
        slider.setTextValueSuffix (suffix);
        slider.setValue (initial, juce::dontSendNotification);
        slider.setDoubleClickReturnValue (true, initial);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label  label;
};

// ---------------------------------------------------------------------
/** Una "striscia" del mixer: carica file, guadagno, pan, mute, solo. */
class TrackStrip : public juce::Component
{
public:
    TrackStrip (AudioEngine& e, int index, std::function<void (int)> onLoadRequested)
        : engine (e), trackIndex (index)
    {
        title.setText ("Traccia " + juce::String (index + 1), juce::dontSendNotification);
        title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        title.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (title);

        fileName.setText ("(vuota)", juce::dontSendNotification);
        fileName.setJustificationType (juce::Justification::centred);
        fileName.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (fileName);

        loadButton.onClick = [this, onLoadRequested] { onLoadRequested (trackIndex); };
        addAndMakeVisible (loadButton);

        // Guadagno in dB: è come lavorano le DAW vere (l'orecchio è logaritmico)
        gain.slider.setSkewFactorFromMidPoint (-12.0);
        gain.slider.onValueChange = [this]
        {
            const auto db = gain.slider.getValue();
            engine.trackParams[(size_t) trackIndex].gain = db <= -60.0 ? 0.0f
                                                                       : juce::Decibels::decibelsToGain ((float) db);
        };
        addAndMakeVisible (gain);

        pan.slider.onValueChange = [this] { engine.trackParams[(size_t) trackIndex].pan = (float) pan.slider.getValue(); };
        addAndMakeVisible (pan);

        mute.onClick = [this] { engine.trackParams[(size_t) trackIndex].mute = mute.getToggleState(); };
        solo.onClick = [this] { engine.trackParams[(size_t) trackIndex].solo = solo.getToggleState(); };
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
    }

    void setFileName (const juce::String& name)
    {
        fileName.setText (name, juce::dontSendNotification);
        fileName.setColour (juce::Label::textColourId, juce::Colours::white);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff2a2f36));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 8.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (22));
        fileName.setBounds (r.removeFromTop (20));
        loadButton.setBounds (r.removeFromTop (26).reduced (10, 0));
        r.removeFromTop (6);

        auto buttons = r.removeFromBottom (26);
        mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2));
        solo.setBounds (buttons);

        gain.setBounds (r.removeFromTop (r.getHeight() / 2));
        pan.setBounds (r);
    }

private:
    AudioEngine& engine;
    const int trackIndex;

    juce::Label title, fileName;
    juce::TextButton loadButton { "Carica..." };
    Knob gain { "Volume", -60.0, 6.0, 0.0, " dB", 0.1 };
    Knob pan  { "Pan", -1.0, 1.0, 0.0, {}, 0.01 };
    juce::ToggleButton mute { "Mute" }, solo { "Solo" };
};

// ---------------------------------------------------------------------
/** Pannello base per un effetto: cornice + interruttore on/off. */
class EffectPanel : public juce::Component
{
public:
    explicit EffectPanel (const juce::String& name)
    {
        enableButton.setButtonText (name);
        addAndMakeVisible (enableButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff2a2f36));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 8.0f);
    }

protected:
    /** Dispone le manopole in riga sotto l'interruttore. */
    void layoutKnobs (std::initializer_list<juce::Component*> comps)
    {
        auto r = getLocalBounds().reduced (10);
        enableButton.setBounds (r.removeFromTop (26).removeFromLeft (200));
        r.removeFromTop (6);

        const int w = r.getWidth() / (int) comps.size();
        for (auto* c : comps)
            c->setBounds (r.removeFromLeft (w).reduced (4, 0));
    }

    juce::ToggleButton enableButton;
};

// ---------------------------------------------------------------------
class FilterPanel : public EffectPanel
{
public:
    explicit FilterPanel (AudioEngine& e) : EffectPanel ("Filtro (biquad)"), engine (e)
    {
        auto& p = engine.filterParams;

        enableButton.onClick = [this] { engine.filterParams.enabled = enableButton.getToggleState(); };

        typeBox.addItem ("Passa-basso",   (int) FilterType::LowPass);
        typeBox.addItem ("Passa-alto",    (int) FilterType::HighPass);
        typeBox.addItem ("Peak (campana)",(int) FilterType::Peak);
        typeBox.addItem ("Low shelf",     (int) FilterType::LowShelf);
        typeBox.addItem ("High shelf",    (int) FilterType::HighShelf);
        typeBox.setSelectedId (p.type.load(), juce::dontSendNotification);
        typeBox.onChange = [this] { engine.filterParams.type = typeBox.getSelectedId(); };
        addAndMakeVisible (typeBox);

        // La frequenza si percepisce in scala logaritmica: 1 kHz a metà corsa
        freq.slider.setSkewFactorFromMidPoint (1000.0);
        freq.slider.onValueChange = [this] { engine.filterParams.freqHz = (float) freq.slider.getValue(); };
        q.slider.setSkewFactorFromMidPoint (1.0);
        q.slider.onValueChange    = [this] { engine.filterParams.q      = (float) q.slider.getValue(); };
        gain.slider.onValueChange = [this] { engine.filterParams.gainDb = (float) gain.slider.getValue(); };

        for (auto* k : { &freq, &q, &gain })
            addAndMakeVisible (k);
    }

    void resized() override
    {
        layoutKnobs ({ &freq, &q, &gain });
        typeBox.setBounds (getLocalBounds().reduced (10).removeFromTop (26).removeFromRight (170));
    }

private:
    AudioEngine& engine;
    juce::ComboBox typeBox;
    Knob freq { "Frequenza", 20.0, 20000.0, 1000.0, " Hz", 1.0 };
    Knob q    { "Q", 0.1, 10.0, 0.707, {}, 0.001 };
    Knob gain { "Guadagno", -24.0, 24.0, 0.0, " dB", 0.1 };
};

// ---------------------------------------------------------------------
class DelayPanel : public EffectPanel
{
public:
    explicit DelayPanel (AudioEngine& e) : EffectPanel ("Delay / Chorus / Flanger"), engine (e)
    {
        enableButton.onClick = [this] { engine.delayParams.enabled = enableButton.getToggleState(); };

        time.slider.setSkewFactorFromMidPoint (250.0);
        time.slider.onValueChange     = [this] { engine.delayParams.timeMs     = (float) time.slider.getValue(); };
        feedback.slider.onValueChange = [this] { engine.delayParams.feedback   = (float) feedback.slider.getValue(); };
        mix.slider.onValueChange      = [this] { engine.delayParams.mix        = (float) mix.slider.getValue(); };
        lfoRate.slider.onValueChange  = [this] { engine.delayParams.lfoRateHz  = (float) lfoRate.slider.getValue(); };
        lfoDepth.slider.onValueChange = [this] { engine.delayParams.lfoDepthMs = (float) lfoDepth.slider.getValue(); };

        for (auto* k : { &time, &feedback, &mix, &lfoRate, &lfoDepth })
            addAndMakeVisible (k);
    }

    void resized() override
    {
        layoutKnobs ({ &time, &feedback, &mix, &lfoRate, &lfoDepth });
    }

private:
    AudioEngine& engine;
    Knob time     { "Tempo",     1.0, 2000.0, 350.0, " ms", 0.1 };
    Knob feedback { "Feedback",  0.0, 0.95,   0.4,   {},    0.01 };
    Knob mix      { "Dry/Wet",   0.0, 1.0,    0.35,  {},    0.01 };
    Knob lfoRate  { "LFO rate",  0.0, 5.0,    0.0,   " Hz", 0.01 };
    Knob lfoDepth { "LFO depth", 0.0, 10.0,   0.0,   " ms", 0.01 };
};
