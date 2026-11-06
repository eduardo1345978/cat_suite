# Cat Box Pocket

Vreckový sampler ako Pocket Operator na steroidoch: Pico Plus 2, zvuková doska WM8960, 16 padov,
8 funkčných tlačidiel, 2 enkódery, OLED a 16 LED. Návrh, nákupný zoznam a zapojenie sú v dokumente
„Cat Box Pocket – vreckový sampler“.

## Čo vie

- 64 zvukov (4 banky × 16), 8 naraz, nahrávanie z mikrofónu alebo line in, čistý 48 kHz alebo lo-fi zvuk
- 16 patternov × 16 krokov, reťaz patternov, swing, parameter na krok (pitch, filter, efekt), live nahrávanie, roll,
  euklidovské rytmy, CHAOS
- klávesnica z jednej vzorky, chop na 16 padov, trim, reverse, loop pri držaní, time-stretch, granulárne zmrazenie
- resampling, overdub looper, 16 punch-in efektov, filter, delay a reverb
- disk vo flash pamäti: cez USB-C sa ukáže ako disk, WAV súbory A01.WAV až D16.WAV (aj ľubovoľné WAV z PC)

## Ovládanie

| Stlačíš | Čo sa stane |
| --- | --- |
| pad | zahrá zvuk (v režime krokov zapne / vypne krok) |
| SOUND + pad | vyberie zvuk · SOUND + SHIFT + pad 1–4 = banka A–D |
| SHIFT + SOUND | klávesnica z vybraného zvuku |
| REC + pad (drž) | nahrávanie do padu |
| REC + PLAY | resampling |
| SHIFT + REC | chop do voľnej banky |
| WRITE | režim krokov · drž WRITE + hraj = live nahrávanie |
| drž krok + A / B | pitch / filter na krok |
| SHIFT + drž pad | roll (rýchlosť enkóderom B) |
| PATTERN + pady | pattern alebo reťaz · SHIFT + PATTERN = CHAOS |
| v režime krokov SHIFT + A / B | euklidovský rytmus |
| BPM | tap tempo · BPM + A / B = tempo / swing · BPM + pad 1 = lo-fi · BPM + pad 13–16 = hlasitosť |
| FX + pad | efekt · FX + A / B = delay / reverb |
| A / B | parametre zvuku, stlačenie = ďalšia stránka |
| SHIFT + A / B | hlasitosť / citlivosť vstupu |
| SHIFT + PLAY | looper (nahráva → hrá → vypnutý) |
| SHIFT + WRITE | uloží projekt |
| SHIFT pri zapnutí | USB disk (PLAY alebo vysunutie = späť) |

## Súbory

- `engine/` celý nástroj bez hardvéru (zvuk, sekvencer, ovládanie, displej, disk)
- `host/` testy na PC: `host/build.sh` (vytvorí aj `pocket-demo.wav`)
- `pico/` vrstva pre Pico Plus 2 (I2S, WM8960, OLED, matica tlačidiel, enkódery, LED, PSRAM, flash, USB disk)

## Nahratie firmvéru

1. GitHub Actions → „Build Cat Box Pocket“ → stiahni `Cat-Box-Pocket` → `cat_box_pocket.uf2`.
2. Drž BOOT na Pico Plus 2 a pripoj USB-C, ukáže sa disk RP2350.
3. Skopíruj naň `cat_box_pocket.uf2`. Pico sa reštartuje do Cat Box Pocket.
