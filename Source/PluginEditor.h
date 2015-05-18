#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

class TmpSndDawAudioProcessorEditor  : public AudioProcessorEditor
{
  public:
    TmpSndDawAudioProcessorEditor (TmpSndDawAudioProcessor&);
    ~TmpSndDawAudioProcessorEditor();

    void paint (Graphics&) override;
    void resized() override;

  private:
    TmpSndDawAudioProcessor& processor;

    Slider mParam;
    WebSocketServer* mWebSocket;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TmpSndDawAudioProcessorEditor)
};


#endif  // PLUGINEDITOR_H_INCLUDED
