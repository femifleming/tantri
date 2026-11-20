<p align="center"><img src="Resources/icon/icon_1024.png" width="160" alt="tantri icon"></p>

# tantri

**Sitar, tabla and tanpura**: physically modelled, polyphonic, and tuned in just-intonation shrutis instead of 12-TET. Builds as VST3, AU, VST2 and a standalone app for macOS (universal: Apple silicon and Intel).

![Tantri](docs/manual/img/ui.png)

The full guide is [`docs/manual/Tantri Manual.pdf`](docs/manual/Tantri%20Manual.pdf), 11 pages, including how the waveguide strings and the modal tabla work.

## What's inside

| | Model | Highlights |
|---|---|---|
| **Sitar** | 2× oversampled digital waveguide, two polarisations, 12 voices | Passive nonlinear **jawari** bridge (Pierce–Van Duyne lattice allpasses driven by the string's own displacement); 13 **taraf** sympathetic strings tuned to the raga; 2 **chikari**; 14-mode gourd body; da/ra mizrab strokes |
| **Tabla** | Modal synthesis: complex one-pole resonators | Dayan modes pulled onto Raman's harmonic series by the **syahi** (continuous from a plain membrane to fully loaded); Bessel mode shapes set each stroke's excitation; damping fingers persist between strokes; degenerate mode pairs beat; tension modulation; bayan wrist pressure and Ghe glide |
| **Tanpura** | Four jawari strings, two polarisations each | Classic Pa–Sa–Sa–sa cycle with a pause; **Auto** first string (Pa, else Ma, else Ni, from the raga); thread jawari swirl; slow drift; humanised plucking |

**Tuning.** Sa is set in Hz, and every pitch is Sa × a just ratio from the **22 shrutis**. The 25 ragas choose the shruti for each swara (for example ati-komal re 256/243 in Bhairav) and mark andolan swaras. You can edit the tuning live on the shruti star. The raga lock can be Off, Snap or Mute.

**Measured** (`TantriTest --pitch`): 198 notes across 4 ragas and 3 octaves, worst error **0.3 cents** without jawari and **1.5 cents** with it.

**Playing.** Pitch-bend gives meend (never quantised). Poly or channel aftertouch glides a note to the next raga swara, and the mod wheel adds gamak. Keys 36–47 play tabla bols, and so does channel 10. Keys below 36 strike the chikari.

**Output.** The instruments are balanced so default playing peaks at about −3 dBFS. A stereo-linked look-ahead limiter (−1 dBFS ceiling, 1.5 ms latency reported to the host) means the output never clips. The **OUT** meter shows the pre-limiter peak and any gain reduction.

**Vortex.** A spinning, tilted spectrogram disc. Rings are partials (log frequency from the eye outward), and spiral arms are time.

## Build

```sh
git submodule update --init          # JUCE
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DTANTRI_COPY_AFTER_BUILD=ON
cmake --build build-release --target Tantri_All
```

`TANTRI_COPY_AFTER_BUILD` installs into `~/Library/Audio/Plug-Ins/{Components,VST3,VST}`. The app is `build-release/Tantri_artefacts/Release/Standalone/Tantri.app`. VST2 uses the FST headers in `third_party/vst2-fst`; turn it off with `-DTANTRI_BUILD_VST2=OFF`.

## Tests and renders

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target TantriTest
T=build/TantriTest_artefacts/Release/TantriTest
$T --pitch            # every swara vs its just ratio, in cents
$T --stress           # 60 s of random MIDI: finite, bounded
$T --levels           # peak / RMS per instrument, limiter activity
$T --cpu              # real-time factor
$T --wav demos        # Yaman alap, teentaal theka, tanpura, full mix
$T --shot ui.png      # editor screenshot
```

## Layout

```
Source/Tuning.h      shrutis, swaras, ragas
Source/Dsp.h         ContactString (waveguide + jawari), SympString, ModalBody, FDN reverb, Limiter
Source/Sitar.h       voices, meend/gamak/andolan, taraf, chikari, gourd
Source/Tabla.h       modal drums, strokes (bols), bayan pressure
Source/Tanpura.h     four-string drone and pluck cycle
Source/Vortex.h      vortex spectrogram
Source/PluginEditor  the E "Grace" look: shruti star, teentaal ring, tanpura flower, star knobs
docs/manual          manual.html → Tantri Manual.pdf (headless Chrome)
mockups              the six look options shown before the UI was designed
tests/Test.cpp       TantriTest; tests/lab = single-string experiments
```

## References

C. V. Raman, *The Indian musical drums* (1934) · J. O. Smith, *Physical Audio Signal Processing* · J. R. Pierce & S. A. Van Duyne, *A passive nonlinear digital filter design…*, JASA 101 (1997) · S. Siddiq, *A physical model of the nonlinear sitar string*, Archives of Acoustics 37 (2012) · V. Välimäki et al., *Discrete-time modelling of musical instruments* (2006).
