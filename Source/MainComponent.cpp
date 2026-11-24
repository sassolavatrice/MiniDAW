#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ---- trasporto ----
    rewindButton.onClick = [this] { engine.rewind(); };
    playButton.onClick   = [this] { engine.play(); };
    stopButton.onClick   = [this] { engine.stop(); };
    settingsButton.onClick = [this] { showAudioSettings(); };

    for (auto* b : { &rewindButton, &playButton, &stopButton, &settingsButton })
        addAndMakeVisible (b);

    timeLabel.setFont (juce::FontOptions (22.0f));
    timeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timeLabel);

    masterKnob.slider.setSkewFactorFromMidPoint (-12.0);
    masterKnob.slider.onValueChange = [this]
    {
        const auto db = masterKnob.slider.getValue();
        engine.masterGain = db <= -60.0 ? 0.0f : juce::Decibels::decibelsToGain ((float) db);
    };
    addAndMakeVisible (masterKnob);

    // ---- tracce ----
    for (int i = 0; i < AudioEngine::maxTracks; ++i)
    {
        strips[(size_t) i] = std::make_unique<TrackStrip> (engine, i, [this] (int t) { chooseFileForTrack (t); });
        addAndMakeVisible (*strips[(size_t) i]);
    }

    // ---- effetti ----
    addAndMakeVisible (filterPanel);
    addAndMakeVisible (delayPanel);

    setSize (1100, 680);

    // 0 ingressi, 2 uscite (stereo). Da qui JUCE apre la scheda audio
    // e inizia a chiamare prepareToPlay() e getNextAudioBlock().
    setAudioChannels (0, 2);

    startTimerHz (20);
}

MainComponent::~MainComponent()
{
    stopTimer();
    shutdownAudio();   // obbligatorio: ferma il thread audio prima di distruggere l'engine
}

// =====================================================================
//  Audio
// =====================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    engine.prepare (sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    engine.process (bufferToFill);
}

void MainComponent::releaseResources() {}

// =====================================================================
//  GUI
// =====================================================================
static juce::String formatTime (double seconds)
{
    const int mins = (int) seconds / 60;
    const double secs = seconds - mins * 60;
    return juce::String (mins) + ":" + juce::String (secs, 2).paddedLeft ('0', 5);
}

void MainComponent::timerCallback()
{
    timeLabel.setText (formatTime (engine.getPositionSeconds()) + "  /  " + formatTime (engine.getLengthSeconds()),
                       juce::dontSendNotification);

    playButton.setColour (juce::TextButton::buttonColourId,
                          engine.isPlaying() ? juce::Colours::darkgreen
                                             : getLookAndFeel().findColour (juce::TextButton::buttonColourId));
}

void MainComponent::chooseFileForTrack (int trackIndex)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Scegli un file audio per la traccia " + juce::String (trackIndex + 1),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       engine.getSupportedWildcards());

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this, trackIndex] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;   // annullato

        const auto error = engine.loadTrack (trackIndex, file);

        if (error.isNotEmpty())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                                                                                  "Errore di caricamento", error),
                                          nullptr);
        else
            strips[(size_t) trackIndex]->setFileName (engine.getTrackName (trackIndex));
    });
}

void MainComponent::showAudioSettings()
{
    // Finestra standard di JUCE per scegliere driver (ALSA / JACK),
    // dispositivo, sample rate e dimensione del buffer.
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (deviceManager,
                                                                          0, 0,   // ingressi
                                                                          2, 2,   // uscite
                                                                          false, false, true, false);
    selector->setSize (520, 380);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector.release());
    options.dialogTitle = "Impostazioni audio";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c2026));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (8);

    // barra superiore
    auto top = r.removeFromTop (90);
    masterKnob.setBounds (top.removeFromRight (100));
    settingsButton.setBounds (top.removeFromRight (100).withSizeKeepingCentre (90, 30));

    auto transport = top.removeFromLeft (300).withSizeKeepingCentre (300, 34);
    rewindButton.setBounds (transport.removeFromLeft (80).reduced (3, 0));
    playButton.setBounds   (transport.removeFromLeft (110).reduced (3, 0));
    stopButton.setBounds   (transport.removeFromLeft (110).reduced (3, 0));
    timeLabel.setBounds (top);

    r.removeFromTop (6);

    // effetti in basso
    auto fx = r.removeFromBottom (200);
    filterPanel.setBounds (fx.removeFromLeft (fx.getWidth() * 2 / 5));
    delayPanel.setBounds (fx);

    r.removeFromBottom (6);

    // tracce al centro
    const int w = r.getWidth() / AudioEngine::maxTracks;
    for (auto& s : strips)
        s->setBounds (r.removeFromLeft (w));
}
