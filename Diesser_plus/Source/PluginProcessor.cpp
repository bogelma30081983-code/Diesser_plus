/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout Diesser_plusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Крутілка 1: Подавлення "Б" та "П" (Низькі частоти)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("BASS_SUPPRESS", 1),
        "Bass Suppress (P/B)",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));

    // Крутілка 2: Подавлення СЧ резонансів та шиплячих
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MID_HIGH_SUPPRESS", 1),
        "Mid/High Suppress",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));

    return { params.begin(), params.end() };
}

// Допоміжний метод для заповнення вектора об'єктами фільтрів
void Diesser_plusAudioProcessor::createFilters(std::vector<std::unique_ptr<juce::dsp::IIR::Filter<float>>>& filters)
{
    filters.clear();
    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        filters.push_back(std::make_unique<juce::dsp::IIR::Filter<float>>());
    }
}

//==============================================================================
Diesser_plusAudioProcessor::Diesser_plusAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
                apvts(*this, nullptr, "Parameters", createParameterLayout()) // Ініціалізація тут
#endif
{
    // Отримуємо вказівники на значення, щоб читати їх в processBlock максимально швидко
    bassSuppressParam = apvts.getRawParameterValue("BASS_SUPPRESS");
    midHighSuppressParam = apvts.getRawParameterValue("MID_HIGH_SUPPRESS");

    // Створюємо фільтри для обох каналів
    createFilters(bassFiltersL); createFilters(bassFiltersR);
    createFilters(midFiltersL);  createFilters(midFiltersR);
    createFilters(highFiltersL); createFilters(highFiltersR);
}

Diesser_plusAudioProcessor::~Diesser_plusAudioProcessor()
{
}

//==============================================================================
const juce::String Diesser_plusAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Diesser_plusAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Diesser_plusAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Diesser_plusAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Diesser_plusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Diesser_plusAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Diesser_plusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Diesser_plusAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Diesser_plusAudioProcessor::getProgramName (int index)
{
    return {};
}

void Diesser_plusAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Diesser_plusAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dspSpec.sampleRate = sampleRate;
    dspSpec.maximumBlockSize = samplesPerBlock;
    dspSpec.numChannels = getTotalNumInputChannels();

    // Наші опорні робочі частоти для кожної зони
    /*float bassFreqs[5] = { 60.0f, 80.0f, 100.0f, 120.0f, 150.0f };
    float midFreqs[5] = { 1200.0f, 1800.0f, 2500.0f, 3200.0f, 4000.0f };
    float highFreqs[5] = { 5500.0f, 7000.0f, 8500.0f, 10000.0f, 12000.0f };*/

    double safeSampleRate = sampleRate <= 0.0 ? 44100.0 : sampleRate; //zachist vid durnya

    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        // 1. Готуємо кожен фільтр під поточний SampleRate та розмір буфера DAW
        bassFiltersL[i]->prepare(dspSpec);
        bassFiltersR[i]->prepare(dspSpec);
        midFiltersL[i]->prepare(dspSpec);
        midFiltersR[i]->prepare(dspSpec);
        highFiltersL[i]->prepare(dspSpec);
        highFiltersR[i]->prepare(dspSpec);

        // 2. Задаємо їм СТАРТОВІ коефіцієнти (прозорий режим: Gain = 1.0)
        auto bassCoeffs = 
            juce::dsp::IIR::Coefficients<float>::makePeakFilter(safeSampleRate, 
                bassFreqs[i], 1.2f, 1.0f);
        *bassFiltersL[i]->coefficients = *bassCoeffs;
        *bassFiltersR[i]->coefficients = *bassCoeffs;

        auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(safeSampleRate, 
            midFreqs[i], 5.0f, 1.0f);
        *midFiltersL[i]->coefficients = *midCoeffs;
        *midFiltersR[i]->coefficients = *midCoeffs;

        auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(safeSampleRate,
            highFreqs[i], 5.0f, 1.0f);
        *highFiltersL[i]->coefficients = *highCoeffs;
        *highFiltersR[i]->coefficients = *highCoeffs;

        // 3. Скидаємо внутрішню пам'ять затримок
        bassFiltersL[i]->reset(); bassFiltersR[i]->reset();
        midFiltersL[i]->reset();  midFiltersR[i]->reset();
        highFiltersL[i]->reset(); highFiltersR[i]->reset();
    }
}

void Diesser_plusAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Diesser_plusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Diesser_plusAudioProcessor::runAnalysis(const juce::AudioBuffer<float>& buffer)
{
    // Отримуємо кількість семплів у поточному блоці DAW (наприклад, 128, 256 або 512)
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    // Очищаємо масив fftData нулями перед копіюванням
    std::fill(std::begin(fftData), std::end(fftData), 0.0f);

    // Копіюємо семпли з 0-го (лівого) каналу нашої DAW в масив для FFT аналізу.
    // Беремо максимум стільки, скільки влазить у наш fftSize.
    int samplesToCopy = std::min(numSamples, (int)fftSize);
    auto* readPointer = buffer.getReadPointer(0); // Лівий канал

    for (int i = 0; i < samplesToCopy; ++i)
    {
        fftData[i] = readPointer[i];
    }

    // === Далі йде твій рідний код без змін ===
    // 1. Застосовуємо вікно Ханна, щоб згладити краї звуку
    window.multiplyWithWindowingTable(fftData, fftSize);

    // 2. Виконуємо саме FFT обчислення (перетворює fftData на спектр)
    fft.performFrequencyOnlyForwardTransform(fftData);

    // Тимчасові вектори для збору кандидатів на резонанси
    std::vector<ResonancePeak> localBass;
    std::vector<ResonancePeak> localMid;
    std::vector<ResonancePeak> localHigh;

    double sampleRate = getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    // 3. Скануємо спектр (йдемо по індексах від 1 до fftSize / 2)
    // Пропускаємо 0-й бін (постійний струм / DC Offset)
    for (int i = 2; i < (fftSize / 2) - 1; ++i)
    {
        float currentMag = fftData[i];
        float prevMag = fftData[i - 1];
        float nextMag = fftData[i + 1];

        // Перевіряємо, чи є точка ЛОКАЛЬНИМ МАКСИМУМОМ (піком)
        if (currentMag > prevMag && currentMag > nextMag)
        {
            float freq = (float)i * ((float)sampleRate / (float)fftSize);

            // Розраховуємо "гостроту" піку відносно сусідів
            float sharpness = currentMag - ((prevMag + nextMag) * 0.5f);

            // Фільтруємо поріг чутливості, щоб не збирати тихий шум (наприклад, менше -60 дБ)
            if (currentMag > 0.01f)
            {
                ResonancePeak peak{ freq, currentMag, sharpness };

                // Сортуємо по зонах
                if (freq >= 20.0f && freq < 200.0f)
                    localBass.push_back(peak);
                else if (freq >= 200.0f && freq < 4000.0f)
                    localMid.push_back(peak);
                else if (freq >= 4000.0f && freq <= 13000.0f)
                    localHigh.push_back(peak);
            }
        }
    }

    // Лямбда-функція для сортування: вибираємо найгостріші та найгучніші проблеми
    auto peakSorter = [](const ResonancePeak& a, const ResonancePeak& b) {
        return (a.magnitude * a.sharpness) > (b.magnitude * b.sharpness);
        };

    // Сортуємо кожен список та залишаємо МАКСИМУМ 5 штук
    std::sort(localBass.begin(), localBass.end(), peakSorter);
    if (localBass.size() > 5) localBass.erase(localBass.begin() + 5, localBass.end());

    std::sort(localMid.begin(), localMid.end(), peakSorter);
    if (localMid.size() > 5) localMid.erase(localMid.begin() + 5, localMid.end());

    std::sort(localHigh.begin(), localHigh.end(), peakSorter);
    if (localHigh.size() > 5) localHigh.erase(localHigh.begin() + 5, localHigh.end());

    // Записуємо відфільтровані топ-5 піків у глобальні змінні під захистом м'ютекса
    {
        const juce::ScopedLock sl(peakMutex);
        bassPeaks = localBass;
        midPeaks = localMid;
        highPeaks = localHigh;
    }
}


void Diesser_plusAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Очищення невикористовуваних каналів
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Твої крутілки та аналіз
    runAnalysis(buffer);

    float bassKnob = bassSuppressParam->load();
    float midHighKnob = midHighSuppressParam->load();

    float maxAttenuationDb = -15.0f;
    float targetBassGainDb = (bassKnob / 100.0f) * maxAttenuationDb;
    float targetMidHighGainDb = (midHighKnob / 100.0f) * maxAttenuationDb;

    double sampleRate = getSampleRate() <= 0.0 ? 44100.0 : getSampleRate();

    // Наші 5 фіксованих опорних частот
    /*float bassFreqs[5] = { 60.0f, 80.0f, 100.0f, 120.0f, 150.0f };
    float midFreqs[5] = { 1200.0f, 1800.0f, 2500.0f, 3200.0f, 4000.0f };
    float highFreqs[5] = { 5500.0f, 7000.0f, 8500.0f, 10000.0f, 12000.0f };*/

    // 1. Створюємо локальні копії прямо тут, щоб вони не змінювалися під час обробки
    std::vector<ResonancePeak> currentBassPeaks;
    std::vector<ResonancePeak> currentMidPeaks;
    std::vector<ResonancePeak> currentHighPeaks;

    // 2. Пробуємо взяти дані БЕЗ очікування
    if (peakMutex.tryEnter())
    {
        currentBassPeaks = bassPeaks;
        currentMidPeaks = midPeaks;
        currentHighPeaks = highPeaks;

        peakMutex.exit(); // Обов'язково самі відкриваємо замок!
    }
    // Якщо tryEnter() повернув false (зайнято), ми просто пролітаємо далі.
    // Аудіо-потік не чекає, а фільтри просто попрацюють один блок на минулих частотах.

    // =========================================================================
    // =========================================================================
    // НАКОПИЧУВАЧІ ДЛЯ GUI (Тепер шукаємо незалежні піки для кожної зони!)
    // =========================================================================
    float bassMaxSuppression = 0.0f;
    float bassSuppressionFreq = 0.0f;

    float midMaxSuppression = 0.0f;
    float midSuppressionFreq = 0.0f;

    float highMaxSuppression = 0.0f;
    float highSuppressionFreq = 0.0f;

    // === 1. Обробка Басу ===
    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        float targetGain = 1.0f;
        float targetFreq = smoothedBassFreqs[i];

        if (bassKnob > 0.1f && i < currentBassPeaks.size())
        {
            float peakWeight = juce::jlimit(0.0f, 1.0f, currentBassPeaks[i].magnitude * 25.0f);
            targetGain = juce::Decibels::decibelsToGain(targetBassGainDb * peakWeight);
            targetFreq = currentBassPeaks[i].frequency;
        }

        smoothedBassGains[i] += (targetGain - smoothedBassGains[i]) * 0.25f;
        smoothedBassFreqs[i] += (targetFreq - smoothedBassFreqs[i]) * 0.05f;

        auto coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, smoothedBassFreqs[i], 0.7f, smoothedBassGains[i]);
        *bassFiltersL[i]->coefficients = *coefficients;
        *bassFiltersR[i]->coefficients = *coefficients;

        // Зберігаємо пік ТІЛЬКИ для басу
        if (targetGain < 0.99f)
        {
            float currentSuppression = 1.0f - targetGain;
            if (currentSuppression > bassMaxSuppression)
            {
                bassMaxSuppression = currentSuppression;
                bassSuppressionFreq = targetFreq;
            }
        }
    }

    // === 2. Обробка Середини ===
    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        float targetMidGain = 1.0f;
        float targetMidFreq = smoothedMidFreqs[i];

        if (midHighKnob > 0.1f && i < currentMidPeaks.size())
        {
            float midWeight = juce::jlimit(0.0f, 1.0f, currentMidPeaks[i].magnitude * 20.0f);
            targetMidGain = juce::Decibels::decibelsToGain(targetMidHighGainDb * midWeight);
            targetMidFreq = currentMidPeaks[i].frequency;
        }

        smoothedMidGains[i] += (targetMidGain - smoothedMidGains[i]) * 0.15f;
        smoothedMidFreqs[i] += (targetMidFreq - smoothedMidFreqs[i]) * 0.05f;

        auto midCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, smoothedMidFreqs[i], 5.0f, smoothedMidGains[i]);
        *midFiltersL[i]->coefficients = *midCoefficients;
        *midFiltersR[i]->coefficients = *midCoefficients;

        // Зберігаємо пік ТІЛЬКИ для середини
        if (targetMidGain < 0.99f)
        {
            float currentSuppression = 1.0f - targetMidGain;
            if (currentSuppression > midMaxSuppression)
            {
                midMaxSuppression = currentSuppression;
                midSuppressionFreq = targetMidFreq;
            }
        }
    }

    // === 3. Обробка Високих ===
    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        float targetHighGain = 1.0f;
        float targetHighFreq = smoothedHighFreqs[i];

        if (midHighKnob > 0.1f && i < currentHighPeaks.size())
        {
            float highWeight = juce::jlimit(0.0f, 1.0f, currentHighPeaks[i].magnitude * 30.0f);
            targetHighGain = juce::Decibels::decibelsToGain(targetMidHighGainDb * highWeight);
            targetHighFreq = currentHighPeaks[i].frequency;
        }

        smoothedHighGains[i] += (targetHighGain - smoothedHighGains[i]) * 0.11f;
        smoothedHighFreqs[i] += (targetHighFreq - smoothedHighFreqs[i]) * 0.05f;

        auto highCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, smoothedHighFreqs[i], 5.0f, smoothedHighGains[i]);
        *highFiltersL[i]->coefficients = *highCoefficients;
        *highFiltersR[i]->coefficients = *highCoefficients;

        // Зберігаємо пік ТІЛЬКИ для високих
        if (targetHighGain < 0.99f)
        {
            float currentSuppression = 1.0f - targetHighGain;
            if (currentSuppression > highMaxSuppression)
            {
                highMaxSuppression = currentSuppression;
                highSuppressionFreq = targetHighFreq;
            }
        }
    }

    // === 4. ПРОГІН АУДІО ЧЕРЕЗ ФІЛЬТРИ ===
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    for (int i = 0; i < maxPeaksPerZone; ++i)
    {
        bassFiltersL[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(0)));
        bassFiltersR[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(1)));
        midFiltersL[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(0)));
        midFiltersR[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(1)));
        highFiltersL[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(0)));
        highFiltersR[i]->process(juce::dsp::ProcessContextReplacing<float>(audioBlock.getSingleChannelBlock(1)));
    }

    // =========================================================================
    // ВІДПРАВКА В GUI БУФЕР (Тепер відправляємо незалежно кожну зону)
    // =========================================================================
    {
        juce::ScopedLock sl(visualMutex);

        if (bassMaxSuppression > 0.01f)
        {
            VisualPeak vp;
            vp.frequency = bassSuppressionFreq;
            vp.suppressionAmount = bassMaxSuppression;
            vp.alpha = 1.0f;
            visualPeaksBuffer.push_back(vp);
        }

        if (midMaxSuppression > 0.01f)
        {
            VisualPeak vp;
            vp.frequency = midSuppressionFreq;
            vp.suppressionAmount = midMaxSuppression;
            vp.alpha = 1.0f;
            visualPeaksBuffer.push_back(vp);
        }

        if (highMaxSuppression > 0.01f)
        {
            VisualPeak vp;
            vp.frequency = highSuppressionFreq;
            vp.suppressionAmount = highMaxSuppression;
            vp.alpha = 1.0f;
            visualPeaksBuffer.push_back(vp);
        }
    }
}

//==============================================================================
bool Diesser_plusAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Diesser_plusAudioProcessor::createEditor()
{
    return new Diesser_plusAudioProcessorEditor (*this);
    //return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void Diesser_plusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Diesser_plusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Diesser_plusAudioProcessor();
}
