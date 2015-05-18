#include "PluginProcessor.h"
#include "PluginEditor.h"

TmpSndDawAudioProcessorEditor::TmpSndDawAudioProcessorEditor (TmpSndDawAudioProcessor& p)
: AudioProcessorEditor (&p),
  processor (p)
{
  setSize (400, 300);

  mParam.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
  mParam.setRange(0.0, 127.0, 1.0);
  mParam.setTextBoxStyle (Slider::NoTextBox, false, 90, 0);
  mParam.setPopupDisplayEnabled (true, this);
  mParam.setTextValueSuffix (" Volume");
  mParam.setValue(1.0);

  addAndMakeVisible(&mParam);
}

TmpSndDawAudioProcessorEditor::~TmpSndDawAudioProcessorEditor()
{
}

void TmpSndDawAudioProcessorEditor::paint (Graphics& g)
{
  g.fillAll (Colours::white);

  g.setColour (Colours::black);
  g.setFont (15.0f);
  g.drawFittedText ("Midi Volume", 0, 0, getWidth(), 30, Justification::centred, 1);
}

void TmpSndDawAudioProcessorEditor::resized()
{
  mParam.setBounds (40, 40, 40, 40);
}
