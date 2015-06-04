#include "../JuceLibraryCode/JuceHeader.h"

// solid rectangle
class TmpSndDawLookAndFeelHeavy : public juce::LookAndFeel_V3
{
  void drawLinearSlider(Graphics &g,
                        int x,
                        int y,
                        int width,
                        int height,
                        float sliderPos,
                        float minSliderPos,
                        float maxSliderPos,
                        const Slider::SliderStyle style,
                        Slider &slider) override
  {
    Colour c1(255,255,255);
    Colour c2(0,0,0);
    const int padding = 4;
    int rectWidth = width * (slider.getValue() - slider.getMinimum()) /
                            (slider.getMaximum() - slider.getMinimum());
    g.setColour(c1);
    g.fillRect(x, y, width, height);
    g.setColour(c2);
    g.drawRect(x, y, width, height);
    g.fillRect(x + padding, y + padding, rectWidth - padding * 2, height - padding * 2);
  }
};

// just a line
class TmpSndDawLookAndFeelMinimal : public juce::LookAndFeel_V3
{
  void drawLinearSlider(Graphics &g,
                        int x,
                        int y,
                        int width,
                        int height,
                        float sliderPos,
                        float minSliderPos,
                        float maxSliderPos,
                        const Slider::SliderStyle style,
                        Slider &slider) override
  {
    Colour c1(255,255,255);
    Colour c2(0,0,0);
    const int paddingv = 4;
    const int paddingh = 14;
    int pos = (width - paddingh / 2 - 2) *
      (slider.getValue() - slider.getMinimum()) /
      (slider.getMaximum() - slider.getMinimum());
    g.setColour(c1);
    g.fillRect(x, y, width, height);
    g.setColour(c2);
    g.drawRect(x, y, width, height);
    g.drawLine(pos + paddingh, paddingv, pos + paddingh, height - paddingv);
  }
};
