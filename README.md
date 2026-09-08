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
| [`tools/`](tools/) | Small programs that use the engine but are not part of any demo: a smoke test, a font sheet, and the channel-art renderer. |
| [`brand/`](brand/) | Finished PNGs — avatar, banner, watermark, thumbnails. Ready to download and upload. |

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
| `engine/` | 18 | 2,715 | 1,998 |
| `demos/01-coldstart/` | 13 | 2,313 | 1,563 |
| `demos/02-runtime/` | 7 | 1,404 | 935 |
| `demos/03-liftoff/` | 12 | 3,067 | 1,906 |
| `tools/` | 3 | 713 | 517 |
| **Total** | **53** | **10,212** | **6,919** |

Third-party libraries: none. The engine links libc and Win32, and nothing else.

## Licence

MIT — see [LICENSE](LICENSE).
