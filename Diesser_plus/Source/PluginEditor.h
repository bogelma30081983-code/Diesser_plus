/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
// === Створюємо наш власний стиль для повзунків ===
class CustomSliderAppearance : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // 1. Малюємо фонову лінію (трек)
        g.setColour(juce::Colours::darkgrey.darker());
        g.fillRoundedRectangle(x + width / 2 - 3, y, 6, height, 3.0f);

        // 2. Малюємо сам повзунок (ручку)
        g.setColour(juce::Colours::orange); // Колір індикатора
        g.fillRoundedRectangle(x + width / 2 - 15, sliderPos - 8, 30, 16, 4.0f);

        // Малюємо лінію посередині ручки для краси
        g.setColour(juce::Colours::black);
        g.fillRect(x + width / 2 - 10, static_cast<int>(sliderPos) - 1, 20, 2);
    }
};

// =================================================
// 
//struct VisualPeak
//{
//    float frequency = 0.0f;
//    float suppressionAmount = 0.0f; // Наскільки сильно подавляється (для розміру кола)
//    float alpha = 0.0f;             // Прозорість (від 1.0 до 0.0 для плавного згасання)
//};
//============================
class Diesser_plusAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    Diesser_plusAudioProcessorEditor (Diesser_plusAudioProcessor&);
    ~Diesser_plusAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    // Метод таймера
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Diesser_plusAudioProcessor& audioProcessor;

    

    // Наш кастомний дизайн
    CustomSliderAppearance customLookAndFeel;

    // Слайдери
    juce::Slider bassSlider;
    juce::Slider midHighSlider;

    // "Міст" між слайдерами та параметрами в Processor
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midHighAttachment;

    std::vector<VisualPeak> activeVisualPeaks;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Diesser_plusAudioProcessorEditor)
};
