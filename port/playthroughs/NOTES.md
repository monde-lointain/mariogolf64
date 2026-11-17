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
| 01-1 | The lines coming from Luigi's head in the intro look black instead of white | open |
| 01-2 | Peach's gem has a black diamond shape around it                            | open |
| 01-3 | There's clipping around the logo while the club is spinning                | open |
| 01-4 | Occasionally garbage will appear on the black background between menu transitions | open |
| 01-5 | Game crashes when "Erase Data" is selected                                 | fixed in 3619e6f6; replaying the session also needed 0e8e49d1, b6c9b679 and 925280c9 |

## Session 02: first holes

Recorded in `02-first-holes.txt`.

| ID   | Note                                                                       | Status |
|------|----------------------------------------------------------------------------|--------|
| 02-1 | Cracks in the tiles in the hole preview                                    | open |
| 02-2 | Hole disappears when zoom into flag occurs                                 | open |
| 02-3 | Course does not redraw when turning to aim                                 | open |
| 02-4 | Black circular shadows overlapping preview line when moving camera with C buttons | open |
| 02-5 | Preview line is solid instead of dashed                                    | open |
| 02-6 | Yardage text seems to clip/wrap improperly                                 | open |
| 02-7 | Gleam off ball while flying looks black                                    | open |
| 02-8 | When zoomed in on cup during putting, unnatural green block appears from below it | open |

## Found by replays and comparisons

Not noted while playing, but seen in the port's own runs.

| ID  | Note                                                                       | Status |
|-----|----------------------------------------------------------------------------|--------|
| R-1 | A black wedge in the sky, top right, only in windowed runs; the RSP's sky triangles match a headless run's | open |
| R-2 | Frames differ from ares by up to a few thousand pixels: slightly darker overall, dither, two-texture blends, the tree line, the HUD and shadows | open |
| R-3 | AddressSanitizer stopped session 02's replay on choosing the mode: a preset's empty bet slot read before the stakes table | fixed in 6062eae5 |
