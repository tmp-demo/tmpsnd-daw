#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

class TmpSndDawAudioProcessorEditor  : public AudioProcessorEditor
{
  public:
    TmpSndDawAudioProcessorEditor (TmpSndDawAudioProcessor*);
    ~TmpSndDawAudioProcessorEditor();

    void paint (Graphics&) override;
    void resized() override;

    uint32_t Initialize();

  private:
    TmpSndDawAudioProcessor* mProcessor;

    // return the max number of params for an inst so we can layout properly
    uint32_t InitializeParams();

    Array<Slider*> mSliders;
    Array<Label*> mParamLabels;
    Array<Label*> mInstLabels;
    Array<uint32_t> mInstMapping;
    Label* mTitle;
    Label* mInstructions;
    WebSocketServer* mWebSocket;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TmpSndDawAudioProcessorEditor)
};


#endif  // PLUGINEDITOR_H_INCLUDED
