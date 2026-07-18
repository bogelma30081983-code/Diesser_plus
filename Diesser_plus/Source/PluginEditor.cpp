/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Diesser_plusAudioProcessorEditor::Diesser_plusAudioProcessorEditor (Diesser_plusAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Запускаємо таймер на 30 кадрів на секунду (приблизно кожні 33 мілісекунди)
    startTimerHz(45);
    
    //gain
    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    gainSlider.setLookAndFeel(&customLookAndFeel); // Застосовуємо кастомний дизайн
    addAndMakeVisible(gainSlider);

    // Прив'язуємо до параметра "GAIN"
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "GAIN", gainSlider);


    // === Налаштування Bass Slider ===
    bassSlider.setSliderStyle(juce::Slider::LinearVertical);
    bassSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    bassSlider.setLookAndFeel(&customLookAndFeel); // Застосовуємо кастомний дизайн
    addAndMakeVisible(bassSlider);

    // Прив'язуємо до параметра "BASS_SUPPRESS"
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "BASS_SUPPRESS", bassSlider);

    // === Налаштування Mid/High Slider ===
    midHighSlider.setSliderStyle(juce::Slider::LinearVertical);
    midHighSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    midHighSlider.setLookAndFeel(&customLookAndFeel); // Застосовуємо кастомний дизайн
    addAndMakeVisible(midHighSlider);

    // Прив'язуємо до параметра "MID_HIGH_SUPPRESS"
    midHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "MID_HIGH_SUPPRESS", midHighSlider);

    // Задаємо базовий розмір вікна плагіна
    setSize(400, 400);

}

Diesser_plusAudioProcessorEditor::~Diesser_plusAudioProcessorEditor()
{
    // Обов'язково відв'язуємо LookAndFeel при закритті, щоб уникнути витоків пам'яті
    bassSlider.setLookAndFeel(nullptr);
    midHighSlider.setLookAndFeel(nullptr);
    gainSlider.setLookAndFeel(nullptr);
}
//
// =================
void Diesser_plusAudioProcessorEditor::timerCallback()
{
    // 1. Плавно зменшуємо прозорість існуючих точок
    for (auto& vp : activeVisualPeaks)
    {
        vp.alpha -= 0.05f; // Швидкість згасання (чим більше число, тим швидше зникає)
    }

    // 2. Видаляємо точки, які повністю згасли (alpha < 0)
    activeVisualPeaks.erase(
        std::remove_if(activeVisualPeaks.begin(), activeVisualPeaks.end(),
            [](const VisualPeak& p) { return p.alpha <= 0.0f; }),
        activeVisualPeaks.end());

    // 3. Беремо нові спалахи від аудіопроцесора
    auto newPeaks = audioProcessor.getAndClearVisualPeaks();
    for (const auto& np : newPeaks)
    {
        activeVisualPeaks.push_back(np);
    }

    // 4. Даємо команду перемалювати екран (викличеться метод paint)
    repaint();
}
//==============================================================================

void Diesser_plusAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Темний фон плагіна
    g.fillAll(juce::Colours::black.brighter(0.1f));

    // Наш екран аналізатора (x, y, ширина, висота)
    juce::Rectangle<int> displayArea(10, 10, getWidth() - 20, 130);

    // Фон самого екрану
    g.setColour(juce::Colours::black);
    g.fillRect(displayArea);

    // === МАЛЮЄМО ЛОГАРИФМІЧНУ СІТКУ ===
    // Масив опорних частот, які ми хочемо підписати і прокреслити
    struct GridLine { float freq; juce::String label; };
    std::vector<GridLine> gridFrequencies = {
        { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" },
        { 500.0f, "500" }, { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" },
        { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(10.0f); // Дрібний акуратний шрифт для частот

    for (const auto& line : gridFrequencies)
    {
        // Рахуємо X для кожної лінії за тією ж формулою, що й для стовпчиків
        float normalizedX = juce::mapFromLog10(line.freq, 20.0f, 20000.0f);
        int xPos = displayArea.getX() + (int)(normalizedX * displayArea.getWidth());

        // Перевіряємо, щоб лінія не вилізла за межі екрану
        if (xPos >= displayArea.getX() && xPos <= displayArea.getRight())
        {
            // Малюємо тонку напівпрозору вертикальну лінію сітки
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawVerticalLine(xPos, displayArea.getY(), displayArea.getBottom() - 15);

            // Малюємо підпис частоти трохи вище нижньої межі екрану
            g.setColour(juce::Colours::grey);
            g.drawText(line.label, xPos - 20, displayArea.getBottom() - 12, 40, 12,
                juce::Justification::centred);
        }
    }
    // ==================================

    // Рамка екрану поверх сітки
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(displayArea);

    // 2. МАЛЮЄМО СТОВПЧИКИ ПРИДУШЕННЯ (вони будуть поверх сітки)
    for (const auto& vp : activeVisualPeaks)
    {
        float normalizedX = juce::mapFromLog10(vp.frequency, 20.0f, 20000.0f);
        int xPos = displayArea.getX() + (int)(normalizedX * displayArea.getWidth());

        // Висота екрану для стовпчиків (залишаємо знизу 15 пікселів під текст сітки)
        int usableHeight = displayArea.getHeight() - 15;
        int maxBarHeight = (int)(usableHeight * 0.9f);
        int barHeight = (int)(vp.suppressionAmount * maxBarHeight * vp.alpha);

        // Стовпчик росте знизу вгору, але відраховується від лінії над текстом
        int yPos = (displayArea.getBottom() - 15) - barHeight;
        int barWidth = 2; // Тонка неонова лінія

        // Колір стовпчика (зелений "електронний")
        g.setColour(juce::Colours::limegreen.withAlpha(vp.alpha));
        g.fillRect(xPos - barWidth / 2, yPos, barWidth, barHeight);
    }

    // Підписи над повзунками управління нижче екрану
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawFittedText("Bass Suppress", 50, 150, 100, 20, juce::Justification::centred, 1);
    g.drawFittedText("Mid/High Suppress", 250, 150, 100, 20, juce::Justification::centred, 1);
    g.drawFittedText("Saturator", 150, 150, 100, 20, juce::Justification::centred, 1);
}



void Diesser_plusAudioProcessorEditor::resized()
{
    // Розміщуємо слайдери: x, y, ширина, висота
    bassSlider.setBounds(50, 170, 100, 200);
    midHighSlider.setBounds(250, 170, 100, 200);
    gainSlider.setBounds(150, 170, 100, 200);
}
