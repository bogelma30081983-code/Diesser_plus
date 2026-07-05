/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/

struct VisualPeak
{
    float frequency = 0.0f;
    float suppressionAmount = 0.0f; // Від 0.0 до 1.0 (наскільки сильно подавляється)
    float alpha = 1.0f;             // Завжди починається з 1.0 (максимальна яскравість)
};
//================
class Diesser_plusAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Diesser_plusAudioProcessor();
    ~Diesser_plusAudioProcessor() override;

    //==============================================================================
    // Структура для опису однієї проблемної точки (піку)
    struct ResonancePeak
    {
        float frequency = 0.0f; // Частота в Гц
        float magnitude = 0.0f; // Амплітуда (гучність)
        float sharpness = 0.0f; // Гострота (наскільки пік вузький)
    };

   

    
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;


    // Створимо публічні методи, щоб GUI міг забирати координати точок для малювання
    std::vector<ResonancePeak> getBassPeaks() { const juce::ScopedLock sl(peakMutex); return bassPeaks; }
    std::vector<ResonancePeak> getMidPeaks() { const juce::ScopedLock sl(peakMutex); return midPeaks; }
    std::vector<ResonancePeak> getHighPeaks() { const juce::ScopedLock sl(peakMutex); return highPeaks; }
    //====
    // === ТУТ ТИ ЗАБУВ ОГОЛОСИТИ ЗМІННІ ДЛЯ ХВИЛЬОВОГО GUI! ДОДАЙ СЮДЫ: ===
    juce::CriticalSection visualMutex;
    std::vector<VisualPeak> visualPeaksBuffer;
    // Метод, який Editor викликатиме 30 разів на секунду
    std::vector<VisualPeak> getAndClearVisualPeaks()
    {
        juce::ScopedLock sl(visualMutex);
        auto copy = visualPeaksBuffer;
        visualPeaksBuffer.clear(); // Очищаємо, щоб не малювати те саме двічі
        return copy;
    }
    juce::AudioProcessorValueTreeState apvts;

private:

    const float drive = 1.12f;
    juce::Random random;
    //int bufferCounter = 0;
    float smoothedBassGains[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float smoothedMidGains[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float smoothedHighGains[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    float smoothedBassFreqs[5] = { 60.0f, 80.0f, 100.0f, 150.0f, 200.0f };
    float smoothedMidFreqs[5] = { 1200.0f, 1800.0f, 2500.0f, 3200.0f, 4000.0f };
    float smoothedHighFreqs[5] = { 5000.0f, 7000.0f, 8500.0f, 10000.0f, 12000.0f };


    //int bufferCounter = 0; // Лічильник пропущених буферів

    

    // Створюємо зручну функцію для генерації параметрів
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Стрілочні вказівники для швидкого доступу до значень крутілок в аудіо-потоці
    std::atomic<float>* bassSuppressParam = nullptr;
    std::atomic<float>* midHighSuppressParam = nullptr;
    std::atomic<float>* gainParam = nullptr;

    //juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Математика FFT (2^11 = 2048)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048

    juce::dsp::FFT fft{ fftOrder };
    juce::dsp::WindowingFunction<float> window{ fftSize, juce::dsp::WindowingFunction<float>::hann };

    // Буфери для збору та аналізу даних
    float fftFifo[fftSize];           // Сюди накопичуємо аудіо з processBlock
    float fftData[fftSize * 2];       // Сюди копіюємо і передаємо в FFT (розмір х2 для комплексної математики)
    int fifoIndex = 0;                 // Вказівник поточної позиції в FIFO
    bool nextFFTBlockReady = false;    // Прапорець, що вікно назбиралося і готове до аналізу

    

    // Списки для збереження знайдених проблем (до 5 на кожну зону)
    std::vector<ResonancePeak> bassPeaks;
    std::vector<ResonancePeak> midPeaks;
    std::vector<ResonancePeak> highPeaks;

    // Спеціальний м'ютекс для безпечної передачі точок між аудіо-потоком і GUI
    juce::CriticalSection peakMutex;


    //==
    

    // Кількість фільтрів на зону
    static constexpr int maxPeaksPerZone = 5;
    // Наші опорні робочі частоти для кожної зони
    const float bassFreqs[5] = { 60.0f, 80.0f, 100.0f, 120.0f, 150.0f };
    const float midFreqs[5] = { 1200.0f, 1800.0f, 2500.0f, 3200.0f, 4000.0f };
    const float highFreqs[5] = { 5500.0f, 7000.0f, 8500.0f, 10000.0f, 12000.0f };

    // Специфікація процесора для ініціалізації DSP
    juce::dsp::ProcessSpec dspSpec;

    // Масиви фільтрів для Лівого (0) та Правого (1) каналів
    // Використовуємо IIR::Filter<float>
    std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>> bassFiltersL, bassFiltersR;
    std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>> midFiltersL, midFiltersR;
    std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>> highFiltersL, highFiltersR;

    // Допоміжна функція для створення масиву фільтрів
    //void createFilters(std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>>& filters);
    void runAnalysis(const juce::AudioBuffer<float>& buffer);
    void createFilters(std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>>& filters);
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Diesser_plusAudioProcessor)
};
