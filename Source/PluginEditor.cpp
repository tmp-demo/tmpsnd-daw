#include "PluginProcessor.h"
#include "PluginEditor.h"

const uint32_t instWidth = 200;
const uint32_t paramHeight = 20;
const uint32_t verticalPadding = 20;
const uint32_t horizontalPadding = 20;

TmpSndDawAudioProcessorEditor::TmpSndDawAudioProcessorEditor (TmpSndDawAudioProcessor* aProcessor)
: AudioProcessorEditor (aProcessor),
  mProcessor (aProcessor),
  mTitle(nullptr)
{
  uint32_t maxParams = Initialize();
  uint32_t instCount = mInstLabels.size();
  uint32_t w = instCount * (horizontalPadding + instWidth);
  uint32_t h = maxParams * (verticalPadding + paramHeight * 2) + paramHeight;
  setSize (w + verticalPadding, h + horizontalPadding);
}

uint32_t TmpSndDawAudioProcessorEditor::Initialize()
{
  Array<Parameter*, CriticalSection>* p = mProcessor->GetParametersArray();
  String currentInstName;
  mInstLabels.clearQuick();
  mParamLabels.clearQuick();
  mSliders.clearQuick();
  delete mTitle;

  mTitle = new Label();
  mTitle->setText(mProcessor->getName(), dontSendNotification);
  addAndMakeVisible(mTitle);

  uint32_t currentParamCount = 0;
  uint32_t maxParams = 0;
  // params are ordered by instruments in the array, because they come form the
  // json. we keep the current instrument index, so we can then map params to
  // instrument to do the layout properly.
  uint32_t currentInstIndex = -1;
  for (uint32_t i = 0; i < p->size(); i++) {
    // sliders
    Slider* s = new Slider();
    s->setSliderStyle(Slider::LinearHorizontal);
    s->setRange((*p)[i]->mMin,(*p)[i]->mMax, (*p)[i]->mStep);
    // text size: letter width * log(max) + log(1/step) ?
    s->setTextBoxStyle (Slider::TextBoxRight, false, 30, 20);
    s->setValue((*p)[i]->mDefault);

    // instrument label
    uint32_t index_param = (*p)[i]->mName.indexOfChar(' ');
    String instName = (*p)[i]->mName.substring(0, index_param);
    if (instName != currentInstName) {
      currentInstIndex++;
      Label* l = new Label();
      currentInstName = instName;
      l->setText(currentInstName, dontSendNotification);
      addAndMakeVisible(l);
      mInstLabels.add(l);
      if (currentParamCount > maxParams) {
        maxParams = currentParamCount;
        currentParamCount = 0;
      }
    }

    // parameters label
    String paramName = (*p)[i]->mName.substring(index_param);
    Label* l = new Label();
    l->setText(paramName, dontSendNotification);
    l->attachToComponent(s, false);
    mInstMapping.add(currentInstIndex);

    addAndMakeVisible(l);
    addAndMakeVisible(s);

    mSliders.add(s);
    mParamLabels.add(l);
    currentParamCount++;
  }
  return maxParams;
}

TmpSndDawAudioProcessorEditor::~TmpSndDawAudioProcessorEditor()
{
}

void TmpSndDawAudioProcessorEditor::paint (Graphics& g)
{
  g.fillAll (Colours::white);

  g.setColour (Colours::black);
  g.setFont (15.0f);
}

void TmpSndDawAudioProcessorEditor::resized()
{
  // in px
  uint32_t offsetX = -(horizontalPadding + instWidth);
  uint32_t offsetY = 0;

  Font instFont("Courier New", 15, Font::bold);
  Font param("Courier New", 15, Font::plain);
  Font title("Courier New", 20, Font::italic | Font::bold);


  Initialize();

  mTitle->setFont(title);
  mTitle->setBounds(0, 0, 200, 25);

  uint32_t currentInst = -1;
  for (uint32_t i = 0; i < mSliders.size(); i++) {
    if (mInstMapping[i] != currentInst) {
      // new instrument put the instrument label
      currentInst = mInstMapping[i];
      offsetY = 25;
      offsetX += horizontalPadding + instWidth;
      mInstLabels[currentInst]->setBounds(offsetX,
                                          offsetY,
                                          instWidth,
                                          verticalPadding);
      mInstLabels[currentInst]->setFont(instFont);
      offsetY += verticalPadding * 2;
    }
    mSliders[i]->setBounds (offsetX,
                            offsetY,
                            instWidth,
                            paramHeight);
    mParamLabels[i]->setFont(param);
    offsetY += paramHeight + verticalPadding;
  }
}
