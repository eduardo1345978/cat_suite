# Cat Box Pocket

Vreckový sampler ako Pocket Operator na steroidoch: Pico Plus 2, zvuková doska WM8960, 16 padov,
20 funkčných tlačidiel, 4 enkódery, crossfader, OLED 1,3", 16 LED a microSD. Návrh, nákupný
zoznam a zapojenie sú v dokumente „Cat Box Pocket – vreckový sampler“.

## Čo vie

- 64 zvukov (4 banky × 16), 8 naraz, nahrávanie z mikrofónu alebo line in (s prahom a orezaním ticha), čistý 48 kHz alebo lo-fi
- 16 patternov do 64 krokov, vlastná dĺžka každého zvuku (polymeter), podmienky krokov a pravdepodobnosť, FILL,
  sila, prízvuk, pan a pitch / filter na krok, swing, roll, posun krokov, euklid, CHAOS, live nahrávanie
- reťaz patternov, režim piesne, mute a sólo, COPY / PASTE / CLEAR, UNDO, metronóm
- attack, decay, drive, choke skupiny, LFO, vintage sampler (SP-1200, S950, 8 bit), klávesy so stupnicami a arpeggiom
- chop, trim + normalizácia, reverse, loop pri držaní, time-stretch, granulárne zmrazenie, resampling, overdub looper
- 16 punch-in efektov, delay, reverb, kompresor, sidechain, dve scény a crossfader
- 16 projektov, kity, prehliadač WAV, export patternu alebo piesne do stereo WAV
- vnútorná flash aj microSD (FAT16 / FAT32), cez USB-C sa ukážu ako disk

## Ovládanie

Drž MENU a stlač ľubovoľné tlačidlo: displej napíše, čo robí. Celá tabuľka je v dokumente.

| Stlačíš | Čo sa stane |
| --- | --- |
| pad | zahrá zvuk (v režime krokov krok, v klávesoch tón) |
| SOUND + pad | vyberie zvuk · SOUND + SHIFT + pad 1–4 = banka · SOUND + A–D = arpeggio, rýchlosť, stupnica, tónina |
| REC + pad (drž) | nahrávanie · REC + PLAY resampling · SHIFT + REC chop |
| WRITE | režim krokov · drž WRITE + hraj = live nahrávanie · SHIFT + WRITE uloží |
| drž krok + A–D | pitch, filter, sila, podmienka · SHIFT + A pan · ACCENT prízvuk |
| LEN + pad / A / B | dĺžka patternu · presne · vlastná dĺžka zvuku |
| PATTERN, SONG + pady | pattern / reťaz, piesňa |
| MUTE, COPY, PASTE, CLEAR, UNDO | + pad = zvuk (v krokoch krok), samotné = pattern |
| FILL, ACCENT, SCENE, KIT | fill, plná sila, scény A / B, kity |
| BPM, FX + A–D | tempo, swing, kompresor, hlasitosť · delay, reverb, sidechain, vstup |
| MENU | projekty, prehliadač, export, nastavenia, disk, USB disk |

## Súbory

- `engine/` celý nástroj bez hardvéru (zvuk, sekvencer, ovládanie, displej, disky)
- `host/` testy na PC: `host/build.sh` (183 testov, vytvorí aj `pocket-demo.wav` a obrázky displeja)
- `pico/` vrstva pre Pico Plus 2 (I2S, WM8960, SH1106, 2× MCP23017, microSD, crossfader, LED, PSRAM, flash, USB disk)

## Nahratie firmvéru

1. GitHub Actions → „Build Cat Box Pocket“ → stiahni `Cat-Box-Pocket` → `cat_box_pocket.uf2`.
2. Drž BOOT na Pico Plus 2 a pripoj USB-C, ukáže sa disk RP2350.
3. Skopíruj naň `cat_box_pocket.uf2`. Pico sa reštartuje do Cat Box Pocket.

Karta microSD musí byť FAT32 (karty do 32 GB sú tak naformátované z výroby).
