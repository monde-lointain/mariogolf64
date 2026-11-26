# Playthrough notes

What the player noted while recording each session, in their words, with
an ID that commits can name. A note stays here once fixed, with the commit
that fixed it, so that a replay of its session can check it again.

Status is one of: open; fixed in a commit; or not a port defect, with the
reason (when ares or the N64 shows the same).

## Session 01: opening and menus

Recorded in `01-opening-and-menus.txt` and `01b-options.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 01-1 | The lines coming from Luigi's head in the intro look black instead of white | fixed in 02188566 |
| 01-2 | Peach's gem has a black diamond shape around it                            | fixed in 4ba29360 |
| 01-3 | There's clipping around the logo while the club is spinning                | not a port defect: the club spins through the logo's depth, and ares draws it the same, its head across MARIO and its shaft behind the O (port frames 2615 and 2635 against ares's 2620 and 2640, at 1x and 6x); the frames differ only in edge pixels that the VI blends |
| 01-4 | Occasionally garbage will appear on the black background between menu transitions | fixed in 242e0406 |
| 01-5 | Game crashes when "Erase Data" is selected                                 | fixed in 3619e6f6; replaying the session also needed 0e8e49d1, b6c9b679 and 925280c9 |

## Session 02: first holes

Recorded in `02-first-holes.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 02-1 | Cracks in the tiles in the hole preview                                    | open; where the course's finer tiles meet coarser ones, a corner of the fine ones lies up to 0.7 pixels off the coarse edge, leaving a sliver that nothing draws. The RDP's anti-aliasing draws every pixel that any coverage sample reaches, and its VI blends the partly covered ones, so the N64 hides it; the port, drawing pixel centers, shows it at 1x as ares's RDP does but upscaled as a line. Widening each triangle to its samples' reach closed it, but drew depth seams across characters and notched their outlines without the VI's blend |
| 02-2 | Hole disappears when zoom into flag occurs                                 | fixed in 236e63e4; the game fills only the preview's box of the depth image, and the port cleared all of it to the nearest depth, so the course outside the box failed its depth test |
| 02-3 | Course does not redraw when turning to aim                                 | fixed in 6bee8520; the game clears the frozen frame's depth by filling it as a color image, which the RDP never counted as a depth clear |
| 02-4 | Black circular shadows overlapping preview line when moving camera with C buttons | fixed in 46973715; they were the ball's shadow and sprite rectangles, drawn as columns once their edges wrapped |
| 02-5 | Preview line is solid instead of dashed                                    | not a port defect: ares's RDP draws the same dashes; its VI anti-aliases the partly covered stretches to near nothing, and the port renders without coverage or the VI's filter |
| 02-6 | Yardage text seems to clip/wrap improperly                                 | fixed in 2099d5f9; upscaled, a glyph's first row of target pixels took the texel before its edge, a sliver of the glyph above it in the font |
| 02-7 | Gleam off ball while flying looks black                                    | open |
| 02-8 | When zoomed in on cup during putting, unnatural green block appears from below it | fixed in 28baab46 |

Every note from sessions 01 and 02 was seen again in sessions 03 to 11.
Sessions 05, 06, 08 and 11 brought no new notes.

## Session 03: shot controls

Recorded in `03-shot-controls.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 03-1 | "PIKA" text appears in the ball's spot when the ball falls into the cup, blue and red or blue and gold by turns | fixed in 639c0f7f; the game draws "PIKA" about the ball without G_ZBUFFER but at the primitive's depth, which the cup's wall hides on the N64, and the port tested a triangle's depth only under G_ZBUFFER |
| 03-2 | When raining, the rain occasionally covers the entire screen, which is distracting and unlike the real game | fixed in 46973715; the landed drops are texture rectangles, and those projected far off screen wrapped their edges, so the port drew them over the whole screen as a pattern of drops |

## Session 04: pause menu

Recorded in `04-pause-menu.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 04-1 | Trees on hole 1 appear to alias into other trees                           | open |

## Session 07: Mini-Golf

Recorded in `07-mini-golf.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 07-1 | Walls appear enlarged on the Mini-Golf course                              | open |

## Session 09: Get Character against Luigi

Recorded in `09-get-character.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 09-1 | Luigi's button triangles are surrounded by black                           | fixed in 4ba29360 |

## Session 10: Toad Highlands tournament

Recorded in `10-toad-tournament.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 10-1 | Froze briefly at the scorecard between hole 9 and hole 10                  | open; not seen in replays, headless or windowed, with the save and a recording on the SSD: the save there (frame 12451) takes about 3 ms, as at 11 other holes, and no game frame waits more than 3 retraces, nor any retrace more than 12 ms on the RTX 4070; play in a window now logs each stall with its game frame (see `port/README.md`) |
| 10-2 | A blue background appears around the spinning Toad trophy                  | fixed in 28baab46 |

## Found by replays and comparisons

Not noted while playing, but seen in the port's own runs.

| ID  | Note                                                                       | Status |
|-----|----------------------------------------------------------------------------|--------|
| R-1 | A black wedge in the sky, top right, only in windowed runs; the RSP's sky triangles match a headless run's | fixed in 869026f5; it showed only on lavapipe, headless or not |
| R-2 | Frames differ from ares by up to a few thousand pixels: slightly darker overall, dither, two-texture blends, the tree line, the HUD and shadows | open |
| R-3 | AddressSanitizer stopped session 02's replay on choosing the mode: a preset's empty bet slot read before the stakes table | fixed in 6062eae5 |
| R-4 | On the RTX 4070 each new blend state's pipeline takes about 59 ms to create, on first use, so the first screens hitch four times | open |
| R-5 | At 6x, the intro's distant skyline, a copied strip, was filtered: lighter where it met the sky, and its edges' alpha short of one | fixed in 0845c9ed; at 320x240 it moved the tree line closer to ares's, so R-2's tree line remains |
