#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SliderLookAndFeel.h"
#include "LogoBlob.h"

// width of a column
const uint32_t instWidth = 200;
// height of a slider
const uint32_t paramHeight = 20;
// padding to add to the height of a slider to display the name of the param
const uint32_t verticalPadding = 25;
// padding in between columns
const uint32_t horizontalPadding = 20;

// this appears when you open the plugin UI, but it has not received JSON
// configuraiton data yet
const String INSTRUCTIONS =
"Feed me some JSON at ws://localhost:7681. mmmh, JSON...";

class SliderValueListener : public Slider::Listener
{
public:
  SliderValueListener(TmpSndDawAudioProcessorEditor* aEditor)
    : mEditor(aEditor)
  { }
  void sliderValueChanged(Slider* aSlider) override
  {
    float value = aSlider->getValue();
    // meh linear search
    uint32_t paramIndex = mEditor->mSliders.indexOf(aSlider);
    if (paramIndex != -1) {
      mEditor->mProcessor->setParameter(paramIndex, value);
    }
  }
private:
  TmpSndDawAudioProcessorEditor* mEditor;
};

TmpSndDawAudioProcessorEditor::TmpSndDawAudioProcessorEditor(
    TmpSndDawAudioProcessor* aProcessor)
: AudioProcessorEditor (aProcessor),
  mProcessor (aProcessor),
  mInstructions(nullptr)
{
  mLogo = ImageFileFormat::loadFrom(LogoBlob::logoresized_png, LogoBlob::logoresized_pngSize);
  Initialize();
}

uint32_t TmpSndDawAudioProcessorEditor::Initialize()
{
  const MessageManagerLock mmLock;
  uint32_t maxParams = InitializeParams();
  uint32_t instCount = mInstLabels.size();
  uint32_t w = instCount * (horizontalPadding + instWidth);
  uint32_t h = maxParams * (verticalPadding + paramHeight) + paramHeight;
  // a cute little square window when we haven't received configuration data
  if (maxParams == 0 && instCount == 0) {
    w = 200;
    h = 200;
  }
  setSize (w + verticalPadding, h + horizontalPadding);
    return maxParams;
}

void TmpSndDawAudioProcessorEditor::reset()
{
  const MessageManagerLock mmLock;
  mInstLabels.clearQuick();
  mParamLabels.clearQuick();
  mSliders.clearQuick();
  mInstMapping.clearQuick();

  mInstructions = new Label();
  mInstructions->setText(INSTRUCTIONS, dontSendNotification);
  addAndMakeVisible(mInstructions);

  uint32_t w = 200;
  uint32_t h = 200;
  setSize (w + verticalPadding, h + horizontalPadding);
}

uint32_t TmpSndDawAudioProcessorEditor::InitializeParams()
{
  Array<Parameter*, CriticalSection>* p = mProcessor->GetParametersArray();
  String currentInstName;
  mInstLabels.clearQuick();
  mParamLabels.clearQuick();
  mSliders.clearQuick();
  delete mInstructions;
  mInstructions = nullptr;

  mLogoComponent = new ImageComponent();
  mLogoComponent->setImage(mLogo);
  addAndMakeVisible(mLogoComponent);

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
    s->addListener(new SliderValueListener(this));
    s->setLookAndFeel(new TmpSndDawLookAndFeelMinimal());

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

  // we haven't received configuration data now, display a friendly message
  // with instructions
  if (maxParams == 0 && p->size() == 0) {
    mInstructions = new Label();
    mInstructions->setText(INSTRUCTIONS, dontSendNotification);
    addAndMakeVisible(mInstructions);
  }

  return maxParams;
}

TmpSndDawAudioProcessorEditor::~TmpSndDawAudioProcessorEditor()
{
  mProcessor->OnEditorClose();
  mSliders.clear();
  mParamLabels.clear();
  mInstLabels.clear();
  delete mInstructions;
}

void TmpSndDawAudioProcessorEditor::paint (Graphics& g)
{
  g.fillAll (Colours::white);

  g.setColour (Colours::black);
  g.setFont (16.0f);
}

void TmpSndDawAudioProcessorEditor::resized()
{
  // In some cases, this can be called from the web socket thread, we need to
  // lock the main thread event loop.
  const MessageManagerLock mmLock;
  // in px
  uint32_t offsetX = -(horizontalPadding + instWidth);
  uint32_t offsetY = 0;

  Font instFont("Courier New", 16, Font::bold);
  Font param("Courier New", 16, Font::plain);

  uint32_t maxParams = Initialize();

  mLogoComponent->setBounds(4,4,mLogo.getWidth(),mLogo.getHeight());

  // we haven't received the param list
  if (maxParams == 0 && mSliders.size() == 0) {
    mInstructions->setBounds(0, 30, 200, 170);
  }

  uint32_t currentInst = -1;
  for (uint32_t i = 0; i < mSliders.size(); i++) {
    if (mInstMapping[i] != currentInst) {
      // new instrument, put the instrument label
      currentInst = mInstMapping[i];
      offsetY = mLogo.getHeight() + 4; // room for the title
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
