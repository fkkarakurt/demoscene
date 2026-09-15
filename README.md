# demoscene

Demos written from nothing but C.

No image files, no audio files, no video files, no 3D models. No game engine, no
stock assets, no screen capture, and no GPU. Every pixel and every audio sample
is computed at runtime by code in this repository. The only external program
involved is ffmpeg, and only to pack finished frames into an H.264 file — the
same role the compiler plays for the source.

**COLD START** — https://youtu.be/6soV2AfSZq8 · [KORMOS](https://www.youtube.com/@fkkarakurt)

## Layout

Each part stands on its own and can be read, built or downloaded separately.

| Folder | What is in it |
|---|---|
| [`engine/`](engine/) | The shared renderer and synthesiser. HDR framebuffer, stroke font, post chain, noise, thread pool, oscillators and filters. |
| [`demos/01-coldstart/`](demos/01-coldstart/) | A two-minute demo: nine scenes, a timeline and a soundtrack, built on the engine. |
| [`demos/02-runtime/`](demos/02-runtime/) | Thirty seconds, vertical. The picture is shown being computed, row by row, in the order the thread pool walks it. |
| [`demos/03-liftoff/`](demos/03-liftoff/) | Thirty seconds, vertical, and not abstract. A launch to orbit, flown by an integrated trajectory rather than by keyframes. |
| [`demos/04-trace/`](demos/04-trace/) | Thirty seconds, vertical, in which nothing is drawn. Every frame is the soundtrack, plotted as an oscilloscope plots it, with time laid out in depth. |
| [`demos/05-endurance/`](demos/05-endurance/) | Two minutes in 4K, and a true story: Shackleton's ship, from the winter in the ice to the wreck found 3,008 metres down, lit for the real date, hour and place of every shot. |
| [`tools/`](tools/) | Small programs that use the engine but are not part of any demo: a smoke test, a font sheet, and the channel-art renderer. |
| [`brand/`](brand/) | Finished PNGs — avatar, banner, watermark, thumbnails. Ready to download and upload. |

## Films

The finished films are on the
[releases page](https://github.com/fkkarakurt/demoscene/releases), one release
per demo. Each is the resolution it was rendered at, with the video and audio
streams exactly as the encoder wrote them: nothing is scaled down or
re-encoded for the upload. Every link below starts the download directly.

| Demo | Frame | Rate | Length | Size |
|---|---|---:|---:|---:|
| [COLD START](https://github.com/fkkarakurt/demoscene/releases/download/coldstart/coldstart-1920x1080.mp4) | 1920×1080 | 60 fps | 2:00 | 684.6 MiB |
| [RUNTIME](https://github.com/fkkarakurt/demoscene/releases/download/runtime/runtime-1080x1920.mp4) | 1080×1920 | 60 fps | 0:30 | 140.9 MiB |
| [LIFTOFF](https://github.com/fkkarakurt/demoscene/releases/download/liftoff/liftoff-1080x1920.mp4) | 1080×1920 | 60 fps | 0:30 | 23.6 MiB |
| [TRACE](https://github.com/fkkarakurt/demoscene/releases/download/trace/trace-1440x2560.mp4) | 1440×2560 | 60 fps | 0:30 | 89.0 MiB |
| [ENDURANCE](https://github.com/fkkarakurt/demoscene/releases/download/endurance/endurance-3840x2160.mp4) | 3840×2160 | 24 fps | 2:00 | 331.5 MiB |

H.264 High profile and 48 kHz stereo AAC throughout. Each release's notes
carry the file's SHA-256.

## Building

Requires **gcc** (MinGW-w64 on Windows) and **PowerShell**. The engine uses
Win32 threads and the video sink uses a pipe to ffmpeg, so this is a Windows
project as it stands; the rendering code itself is plain C11 and portable, the
platform layer is `engine/dm_job.c` and `engine/dm_video.c`.

```powershell
.\build.ps1                 # builds demos/01-coldstart
.\build.ps1 -Tool smoke     # builds tools/smoke.c
.\build.ps1 -Debug          # -O0 -g, no fast math
```

There is no makefile on purpose: three dozen translation units rebuild in about
two seconds, so a dependency graph would cost more than it saves.

ffmpeg is only needed to render video. If it is not on `PATH`, point
`FFMPEG_BIN` at the directory holding `ffmpeg.exe`.

## Running

```powershell
.\demos\01-coldstart\out\coldstart.exe song                  # the soundtrack, as a WAV
.\demos\01-coldstart\out\coldstart.exe still 83 1920 1080 2  # one frame, as a PPM
.\demos\01-coldstart\out\coldstart.exe sheet 4 3 640 1       # a contact sheet of the whole demo
.\demos\01-coldstart\out\coldstart.exe render 1920 1080 2 2 14
```

`sheet` is the one that earns its keep: two minutes of demo reviewed in a single
image, so a scene that is too dark or too similar to its neighbour shows up
immediately instead of after a two-hour render.

Rendering is offline and slow on purpose. A frame that takes three seconds to
compute still plays back at 60 fps, which buys real motion blur, high sample
counts and effects that would not fit in a real-time frame budget.

## Size

Counted with `grep -vE '^\s*$' | grep -vE '^\s*(/\*|\*|//)'` for the code column,
so comments and blank lines are excluded there but present in the line count.

| | Files | Lines | Code |
|---|---:|---:|---:|
| `engine/` | 18 | 2,754 | 2,018 |
| `demos/01-coldstart/` | 13 | 2,313 | 1,563 |
| `demos/02-runtime/` | 7 | 1,404 | 935 |
| `demos/03-liftoff/` | 12 | 3,067 | 1,906 |
| `demos/04-trace/` | 14 | 2,188 | 1,449 |
| `demos/05-endurance/` | 24 | 7,510 | 5,424 |
| `tools/` | 3 | 732 | 531 |
| **Total** | **91** | **19,968** | **13,826** |

Third-party libraries: none. The engine links libc and Win32, and nothing else.

## Licence

MIT — see [LICENSE](LICENSE).
