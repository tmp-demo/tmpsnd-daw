#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

class SliderValueListener;

class TmpSndDawAudioProcessorEditor  : public AudioProcessorEditor
{
  public:
    TmpSndDawAudioProcessorEditor (TmpSndDawAudioProcessor*);
    ~TmpSndDawAudioProcessorEditor();

    void paint (Graphics&) override;
    void resized() override;
	void setParameter(uint32_t aIndex, float aValue);


    uint32_t Initialize();
    void reset();

    friend SliderValueListener;

  private:
    TmpSndDawAudioProcessor* mProcessor;

    // return the max number of params for an inst so we can layout properly
    uint32_t InitializeParams();

    OwnedArray<Slider> mSliders;
    OwnedArray<Label> mParamLabels;
    OwnedArray<Label> mInstLabels;
    Array<uint32_t> mInstMapping;
    ScopedPointer<Label> mInstructions;
    WebSocketServer* mWebSocket;
    Image mLogo;
    ScopedPointer<ImageComponent> mLogoComponent;
	ScopedPointer<LookAndFeel> mLookAndFeel;
	Font mFontInst;
	Font mFontParam;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TmpSndDawAudioProcessorEditor)
};


#endif  // PLUGINEDITOR_H_INCLUDED
