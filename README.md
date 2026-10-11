# FiveOsc Synth

סינתיסייזר וירטואלי (VST3 / AU / Standalone) עם 5 אוסילטורים, בנוי על JUCE.

## מה יש בפנים

- **5 אוסילטורים** - לכל אחד: On/Off, צורת גל (Sine / Saw / Square / Triangle / Noise), Octave, Semi, Fine (cents), Level, Pan, Pulse Width
- **Unison** - עד 7 קולות לכל אוסילטור, Detune ו-Stereo Spread
- **פילטר** - LP12, LP24, HP12, BP, Notch. Cutoff, Resonance, Env Amount, Key Track
- **2 מעטפות ADSR** - אחת לווליום ואחת לפילטר
- **LFO** - Sine / Triangle / Square / Random, מופנה ל-Pitch, Filter ו-Amp
- **אפקטים** - Drive, Ping-Pong Delay, Reverb
- **Master** ו-16 קולות פולי
- **5 פריסטים** + שמירת מצב בפרויקט ב-DAW
- Pitch Bend (±2 סמיטונים) ורגישות ל-Velocity

## בנייה (Windows / macOS / Linux)

דרישות: CMake 3.22+, Git, ומהדר C++17 (Visual Studio 2022 / Xcode / GCC).

```
cd FiveOscSynth
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

בהרצה הראשונה CMake מוריד את JUCE 8.0.6 אוטומטית (צריך אינטרנט).
בסיום הבנייה ה-VST3 מועתק אוטומטית לתיקיית הפלאגינים של המערכת:

- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `~/Library/Audio/Plug-Ins/VST3` (וגם AU ב-`Components`)
- Linux: `~/.vst3`

אחר כך פותחים את ה-DAW, עושים סריקת פלאגינים (Rescan) ומוסיפים את "FiveOsc Synth" כאינסטרומנט על טראק MIDI.
בתיקיית `build` יש גם גרסת Standalone לבדיקה בלי DAW.

## רישוי

JUCE מוגש תחת GPLv3 או רישיון מסחרי. אם מפיצים את הפלאגין, צריך לעמוד באחד מהם.

## הורדה ל-Windows (בנייה אוטומטית ב-GitHub)

1. יוצרים repository חדש ב-github.com ומעלים אליו את תוכן התיקייה (כולל `.github`).
2. נכנסים ללשונית **Actions**. הבנייה רצה אוטומטית אחרי ה-push (או ידנית דרך Run workflow).
3. בסיום (כמה דקות) נכנסים להרצה ומורידים מתחת ל-**Artifacts**:
   - `FiveOscSynth-Windows` - קובץ `.vst3` ואפליקציה עצמאית
   - `FiveOscSynth-macOS` - VST3, AU ואפליקציה עצמאית
4. ב-Windows מעתיקים את תיקיית ה-`.vst3` אל `C:\Program Files\Common Files\VST3` ועושים Rescan ב-DAW.
