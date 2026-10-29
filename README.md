# AnalogDrums – analoger Drumcomputer (VST3 / AU / Standalone)

Stimmen: Bassdrum, Snare, Closed Hat, Open Hat, Clap, Rimshot.
16-Step-Sequencer mit Accent, Swing, Pattern-Länge und Host-Sync.

## MIDI-Belegung (General MIDI)
- 35/36 Bassdrum, 38/40 Snare, 42/44 Closed Hat, 46 Open Hat, 39 Clap, 37 Rimshot
- Closed Hat würgt die Open Hat ab.

## Bedienung
- Die farbigen Pads über den Reglern spielen die Stimme zum Probehören an.
- RUN startet den Sequencer mit eigenem Tempo, HOST SYNC folgt der DAW (nur bei Play).
- ACC-Reihe: Steps mit Accent sind lauter (Stärke über den ACCENT-Regler).
- RANDOM würfelt ein Pattern, CLEAR leert alles.

## Bauen
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Ohne Entwicklungsumgebung: auf GitHub hochladen -> Reiter "Actions" -> Artefakt herunterladen.
Windows: AnalogDrums.vst3 nach C:\Program Files\Common Files\VST3\ kopieren.
