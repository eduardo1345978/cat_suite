# Cat Ocean

An endless pixel-art ocean that streams itself to YouTube, with fish your viewers create in the chat.

**Cat-Ocean.exe** (comes with Get-Cat-Sound-Apps.bat) is a relaxing pixel-art ocean that streams itself to YouTube in 1080p, 24/7:
1. Paste your **stream key** (YouTube Studio > Go live > Stream) and your **channel** (`@name`).
2. Press **Get ffmpeg** once (the free tool that sends the video), then **Go live**. *Record a 30 s test video* checks everything without going live.
3. **Copy the YouTube description** puts a ready description with every command on the clipboard (the same text is in `packaging/Cat-Ocean-YouTube-Description.txt`).

Viewers type commands in the live chat:

| Command | What it does |
|---|---|
| `!fish Miro blue stripes` | get your own fish (name + any looks) |
| `!color red yellow white` | body, pattern and fin colours (red, orange, yellow, lime, green, teal, cyan, blue, navy, purple, violet, pink, magenta, coral, white, gray, black, brown, peach, mint, lavender) |
| `!body` slim / round / long / puffer / flat / cat, `!size` small / medium / big | the body |
| `!pattern` plain / stripes / spots / gradient / belly / checker | the pattern |
| `!tail` fan / fork / round / veil, `!fin` none / small / sail / spiky, `!eyes` dot / big / sleepy / star | tail, fin, eyes |
| `!hat` none / hat / glasses / bow / scarf / headphones / lamp / crown | something to wear |
| `!mood` calm / shy / social / hyper / lazy, `!speed` slow / normal / fast | how it swims |
| `!voice` bubble / pluck / bell / flute / chip / marimba / whistle / pad, `!pitch` low / mid / high | how it sings |
| `!melody 1353` (1 = key note .. 8 = octave), `!rhythm` sparse / normal / busy, `!notes` short / long | what it sings |
| `!dance`, `!spin`, `!jump` | moves |
| `!feed`, `!vote 1`, `!name Miro`, `!random`, `!info`, `!level`, `!home`, `!release`, `!help` | food, votes, ... |

**Moderators** (and the host typing in the app): `!rename @name New Name`, `!resetname @name`, `!kick @name`, `!ban @name`, `!unban @name`, `!event storm/calm/feast/babyboom/rarehunt`, `!place reef/kelp/crystal/ship/cave/ice/lagoon/ruins`, `!music aqua/lounge/crystal/bubble`, `!startvote`, `!endvote`.

- **Levels**: a viewer's fish earns XP while it swims (1 a minute) and when they chat (2, at most every 30 s); the name shows the level (*MIRO LV7*). Some looks unlock with levels: LV2 puffer, gradient, sail fin, sleepy eyes, scarf, chip voice; LV3 flat, belly, spiky fin, star eyes, headphones, marimba; LV4 checker, veil tail, whistle; LV5 cat body, lamp; LV6 pad voice; LV8 crown.
- Your fish swims in when you write in the chat; after 20 quiet minutes it naps in the cave (it is kept forever). A cat in a boat waves hello.
- **Genetics**: babies mix their parents' genes (babies of your fish are called *Miro Jr.*), sometimes mutate, and a few are born **Golden, Rainbow, Ghost, Glowing or Crystal**. After many generations evolution discovers **new species** with something no fish had before (whiskers, a glowing antenna, a twin tail, starry spots, a frill, a horn).
- **Music** is always relaxing, in 14 styles: Aqua dream, Sky lounge, Crystal deep, Bubble pop, Glass garden, Bossa breeze, Pixel lullaby, Strings at dusk, City lights, Island drift, Folk tide, Dream haze, Funky reef, Night jazz. The composer never stops: every few minutes a brand new song (its title shows under NOW PLAYING) starts on the next bar and **crossfades** in over the old one, and a change of style fades over too, so there are no jumps. The fishes' genes slowly pull its **tempo and key**, and every fish sings its own melody (with a little fin wave and rings of light rippling out).
- **Places**: Aqua reef, Emerald kelp, Crystal deep (glowing plants, fish glow), Sunken ship (with a treasure chest), Coral cave, Ice ocean, Tropical lagoon (palm trees), Sunken ruins (a stone cat head).
- **Look** (in the app): *Smooth* (default) rounds the pixel edges with xBR when the stream is scaled up to 1920 x 1080; *Pixel* keeps sharp blocks (a bit less CPU). If the PC can't keep up, the stream status says so.
- **Day and night** follow the real clock: sunrise, a pink sunset, stars and a moon, glowing fish and calmer music at night. Waves, bubbles, rain and far-away whale songs play under the music. A chat message becomes a **speech bubble** above your fish.
- **Admin console** (in the app, under the picture): `/help`, `/stats`, `/fish @name`, `/viewers`, `/spawn 5`, `/feed 10`, `/give @name 50`, `/level @name 8`, `/rare @name gold`, `/weather storm`, `/time night` (or `auto`), `/place lagoon`, `/music bubble`, `/event feast`, `/vote start`, `/announce text`, `/party`, `/species`, `/newspecies`, `/sleepall`, `/wakeall`, `/clear food`, `/save`, `/health` and every moderator command. `!` commands run as the host.
- **Health check**: a stream that stops or gets stuck restarts by itself; if the whole ocean freezes, the app restarts itself and goes live again. The uptime and restarts show in the app. Chat votes where the ocean goes, which music plays, and events (storm, feast, baby boom, rare hunt).
- New **subscribers** (the channel's count going up) start a confetti party. Leaderboards: oldest fish, biggest families, rare fish, most babies, species, highest levels, most active today.
- **Alerts:** new members get a golden welcome and a crown, Super Chats / Super Stickers get a big panel and fireworks, subscriber milestones (10, 25, 50, 100, 250 ...) get fireworks.
- **The boat cat** has hunger, sleepiness and happiness, naps, talks to chat, fishes up items (boots, bottles, shells - treasure gives every viewer fish XP) and gets visitors (a gull, a grey cat, a dog). Chat: `!pet`, `!feedcat`.
- **Events:** plankton bloom (glowing water), migration (a school of fish and turtles), meteor shower (`!wish` for +5 XP).
- **End of day:** a recap panel with the day's numbers; `!today` shows them any time. Admin: `/today /recap /testalert member|superchat|milestone /cat /catvisit`.
- **No spam**: one fish per viewer, a word filter (English and Slovak) for names and bubbles, pauses per command (looks 8 s, emotes 20 s, name 1 min, food 45 s, help once for everyone every 90 s), the same message twice is ignored, more than 10 commands a minute mutes a viewer for 2 minutes, shouting bubbles are lowered, at most 6 bubbles and 40 viewers' fish on screen.
