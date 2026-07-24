## 🚀 Що нового (Changelog)

### ✨ Додано (New Features)
* **Перемикач дизерингу (Dither Noise Toggle):**
  * Додано новий параметр `AudioParameterBool` ("NOISE_ENABLE") до `APVTS` із підтримкою автоматизації в DAW.
  * Додано кнопку керування шумом на GUI через `ButtonAttachment`.
  * Кастомізовано зовнішній вигляд кнопки в єдиному стилі через `drawToggleButton` у класі `CustomSliderAppearance`.

### 🛠 Виправлення та оптимізація (Fixes & Optimizations)
* **Оптимізація CPU:** Генерація випадкового шуму (`random.nextFloat()`) тепер повністю обходить виконання, коли перемикач вимкнено.
* **Безпека пам'яті та стабільність:**
  * Явно ініціалізовано прапорець `isNoiseEnabled = false;` у заголовочному файлі, що усунуло некоректну поведінку при запуску.
  * Додано відключення стилю (`setLookAndFeel(nullptr)`) у деструкторі `PluginEditor` для запобігання падінням плагіна при закритті вікна.
 
  * feat: add Dither Noise toggle and LookAndFeel styling

- Added toggleable Dither Noise feature linked to APVTS ("NOISE_ENABLE").
- Integrated custom `drawToggleButton` inside `CustomSliderAppearance` (LookAndFeel_V4).
- Optimized processBlock: bypass random noise generation when dithering is disabled.
- Fixed uninitialized boolean state (`isNoiseEnabled = false;`).
- Safely detached LookAndFeel in `PluginEditor` destructor to prevent GUI crashes.
