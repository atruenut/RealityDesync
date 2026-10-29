# Reality Desync: user guide

## The idea in one minute

Reality Desync makes a sound feel as if its parts are on different clocks.

* **Rhythm is always tied to your tempo.** Each channel's delay, each spectral band's retrigger and each pitch-sequence step is a note value of the host tempo.
* **Texture is allowed to drift.** *Desync* controls how far the delay times wobble, the pitch wanders and the band clocks slip away from the grid.
* **Then it snaps back.** Every N bars (the *Snap Every* setting) all of that drift glides to zero and every band restarts together, exactly on the bar line.

So a loop slowly tears itself apart and reassembles, in time, every N bars.

## Quick start

1. Put the plugin on a drum bus, guitar, synth or vocal. Play the song so the host sends tempo (or set *Clock Source* to Internal and set *Int. BPM*).
2. Pick the preset **Two Clocks**. You hear straight eighths on the left and dotted eighths on the right.
3. Raise **Desync** to 100%. Over the next 4 bars the echoes wobble, the pitch leans and the slices slip.
4. Listen to the bar line at the end of the cycle: the drift disappears and everything is tight again. Press **Snap Now** (a parameter in your host) to trigger that on demand.

## Worked example: turning a dry loop into a fractured one

Goal: a 2-bar drum loop at 120 BPM that stays groovy but keeps sliding out of phase, then locks up on the downbeat of every 4th bar.

| Step | Setting | Why |
|---|---|---|
| 1 | Left time `1/8`, Right time `1/8D` | Left repeats every 0.25 s, right every 0.375 s. They only coincide every 3 beats. |
| 2 | Feedback 40%, Cross 50% | Echoes bounce between the ears. |
| 3 | Slice Amount 50%, Slice rate `1/8`, Bands 8, Spread 80%, Pattern `Slope` | Low bands retrigger slowly, high bands fast. |
| 4 | Pitch +7 st, Stage `Post-delay`, Amount 40% | Adds a fifth above the echoes without moving their timing. |
| 5 | Desync 80%, Snap Every `4 bars`, Build 90% | Drift builds for almost the whole cycle, then snaps. |
| 6 | Time Wobble 60%, Pitch Drift 40%, Slice Slip 60% | Choose *which* aspects drift. |

Save that as your own preset (JUCE edition: **Save** button; Windows DLL edition: use your host's preset saving).

## Controls

### Time split
| Control | What it does |
|---|---|
| Left / Right time | Delay length as a note value (1/64 to 1 bar; triplets and dotted values). Always follows tempo. |
| Feedback | How much of each side is sent back into the delay. Above 100% self-builds (soft limited). |
| Cross | 0% = each side feeds itself. 100% = ping-pong between left and right. |
| Time Glide | How quickly delay times follow a change (avoids pitch zips). |
| Time Wobble | How far the delay times may wander off the grid (up to about 30 ms) when Desync is up. |

### Pitch fracture
| Control | What it does |
|---|---|
| Pitch / Fine | Shift in semitones (±24) and cents. This does not slow or speed the audio, so timing stays on the grid. |
| L/R Split | Left goes down by half the value, right goes up by half. At 7 st the ears are a fifth apart. |
| Pitch Stage | **Pre-delay**: shift, then delay. **In loop**: every repeat shifts again (shimmer / spiral). The first echo is unshifted. **Post-delay**: shift the echoes only, no accumulation. |
| Amount | Blend between the clean and shifted signal. |
| Grain | Shifter window. Short = tighter, more artefacts. Long = smoother, softer attack. |
| Pitch Drift | Random pitch wandering (up to ±2 semitones), scaled by Desync. |

### Motion (LFO and sequencer)
* **LFO** (sine, triangle, saw up/down, square, random step, random glide) modulates pitch. *LFO Length* is a musical length (1/16 to 8 bars). It stays locked to the bar, *Depth* is in semitones and *L/R Phase* offsets the two ears.
* **Sequencer**: 8 steps of ±12 semitones. *Seq Step* sets the step length, *Seq Amount* scales it, *Seq Glide* smooths between steps.

### Spectral slices
The input spectrum is cut into *Bands* log-spaced bands between *Low* and *High*. Each band holds a frozen frame of its spectrum and re-grabs a new one on its own clock.
* **Slice Rate**: the base retrigger length (note value).
* **Rate Spread**: how far apart the band rates fan out (rates run from 1/4x to 4x of the base). Negative values flip the order.
* **Spread Pattern**: *Slope* (low = slow, high = fast), *Alternate* (odd fast, even slow), *Scatter* (fixed pseudo-random).
* **L/R Skew**: runs the right ear faster or slower than the left (up to 3:2).
* **Slice Decay**: 0 holds until the next trigger; higher values fade each slice out.
* **Slice Slip**: how much each band clock drifts off its tempo grid when Desync is up.
* **Amount**: blend between live spectrum and frozen slices.

### Desync and snap
| Control | What it does |
|---|---|
| Desync | Master amount of drift. 0 = never leaves the grid. |
| Drift Rate | How fast the random wandering moves. |
| Snap Every | Realign every 1, 2, 4, 8, 16 or 32 bars, or Off (drift forever). |
| Build Time / Curve | How much of the cycle the drift takes to build, and its shape (negative = early, positive = late). |
| Snap Glide | How quickly drift disappears at the snap (0 to 500 ms). |
| Snap Now | Parameter in the host: any change triggers an immediate realignment. Also automatable. |

### Filter, output
* **Filter**: state-variable low-pass/band-pass/high-pass/notch, placed *In feedback* (each repeat darkens or thins) or *On wet out*. *Mod Depth* and *Mod Length* sweep the cutoff in time with the bar.
* **Mix** is equal-power. **Output** is trim in dB. **Clock Source** `Host` follows the DAW transport and tempo; `Internal` uses *Int. BPM*. If the host does not send tempo the plugin falls back to the internal clock.

## Tips

* Want the cleanest effect? Keep Desync below 30% and Slice Amount below 30%. The result is a subtle widening that stays in time.
* Want to hear the snap clearly? Use Snap Every `1 bar`, Desync 100%, Build 60%.
* Use *In loop* pitch for evolving ambient tails, and *Post-delay* for rhythmic harmony.
* Feedback above 90% builds quickly. Add a low-pass or high-pass in feedback to keep it musical.
* The first moment after pressing play re-aligns everything to the host bar grid.
* Pitch shifting always has a little grain texture. For the cleanest sound use longer Grain on sustained material and shorter on percussion.

## Factory presets

| Name | Category | What it does |
|---|---|---|
| Init - Aligned | Utility | Everything on the grid. Nothing drifts. A clean starting point. |
| Two Clocks | Rhythm | Left runs straight eighths, right runs dotted eighths. They meet again every 3 beats. |
| Three Against Two | Rhythm | Left plays in triplet quarters, right in straight quarters. Classic 3:2 polyrhythm. |
| Slipping Grid | Rhythm | Starts locked, then the grid slides out from under you until the 4-bar snap. |
| Bar Line Cliff | Rhythm | Tension builds quietly, then everything falls apart in the last bar and snaps shut. |
| Metronome Ghost | Rhythm | A dark, steady quarter-note echo that gently ages with every repeat. |
| Syncopation Engine | Rhythm | Odd-length echoes in each ear that shuffle against the beat. |
| Octave Ladder | Pitch | Each repeat climbs another octave. Shimmer with a clock. |
| Falling Floor | Pitch | Every repeat sinks an octave deeper into the floor. |
| Fifth Wheel | Pitch | Left stays home, right sits a perfect fifth above. Instant parallel harmony. |
| Seasick Choir | Pitch | Slow, out-of-phase pitch waves in each ear. Notes lean like a boat. |
| Arpeggio Collapse | Pitch | The 8-step pitch sequencer plays an arpeggio that the delays smear across time. |
| Pitch Cathedral | Pitch | Huge, slow, rising echoes in a stone room that does not exist. |
| Detune Twins | Pitch | Short offset echoes pitched a few cents apart. Wide, liquid doubling. |
| Frozen Prism | Spectral | Frequency bands hold their last moment and restart half a bar later. A lit-up freeze. |
| Shatter Glass | Spectral | Sixteen bands retrigger at scattered rates. Sounds like a window breaking in slow motion. |
| Rainfall Spectrum | Spectral | Lows repeat slowly, highs repeat fast. Every band is on a different clock. |
| Mirror Slices | Spectral | Odd bands race ahead, even bands lag. The right side runs 3:2 against the left. |
| Vowel Drift | Spectral | A band-pass sweeps slowly over retriggering mid bands. Speech-like and unsettling. |
| Time Dilation | Cinematic | Long, sinking repeats where the high and low bands slowly lose track of each other. |
| Event Horizon | Cinematic | Everything falls in. Repeats darken, sink and slowly stretch for eight bars before the snap. |
| Parallel Hallway | Cinematic | Two corridors of different length. The second one sounds a fifth higher. |
| Memory Leak | Cinematic | Feedback that never quite dies. A slight detune makes the tail spiral upward. |
| Sleepwalker | Cinematic | Slow, loose and dreamy. Time and pitch both wander, then the whole thing wakes up on the bar line. |
| Haunted Elevator | Cinematic | A sagging low-pass echo that rides up and down in pitch on every floor. |
| Stutter Machine | Glitch | Fast frozen slices chopping the sound into a mechanical stutter. |
| Buffer Overrun | Glitch | Tiny delays with near-runaway feedback and a resonant band-pass. Metallic and unstable. |
| Sample And Hold Rain | Glitch | A stepped-random pitch generator fires on every eighth note. Falling droplets on a grid. |
| Tape Stop Echo | Glitch | Every bar the pitch falls off a cliff and recovers. Locked to the tempo. |
| Subtle Desync | Subtle | A faint haze of offset echoes. Makes a dry track feel slightly unreal. |
| Stereo Slip | Subtle | Very short, slowly wobbling delays in each ear. Wide without sounding like an effect. |
| Bar Line Reset (Demo) | Subtle | Every drift control maxed with a 1-bar snap. Watch the scope fall apart and re-align. |
