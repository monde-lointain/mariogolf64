# Playthroughs

Sessions played by hand with `--record`, kept here as input scripts so that
they can be replayed headless under the sanitizers and on other builds.
Saves made by them are not kept here.

## Recording a session

Start each session from a copy of a save, never the save you play on, and
keep a copy of the save it started from:

    cp ~/.local/share/mariogolf64/mariogolf64/mariogolf64.sra 01-start.sra
    cp 01-start.sra 01-play.sra
    build/pc/linux/port/src/mariogolf64 --save 01-play.sra \
        --record port/playthroughs/01-opening-and-menus.txt baserom.us.z64

Quit the window when the goal is done. Note anything that looks wrong,
and roughly when it happened.

## Sessions

Each one has one goal, and takes 5 to 15 minutes unless it says otherwise.

| #  | Goal                                                                                                      |
|----|-----------------------------------------------------------------------------------------------------------|
| 01 | Opening and menus: watch the opening, the attract hole, through to the title, then visit every main-menu entry and back out of each without playing |
| 02 | First holes: Stroke on Toad Highlands with Plum, 3 holes, using club changes, the landing view (R), the overview (Z+R) and the shot replay (B) |
| 03 | Shot controls: spin (Z + stick), power shots, moving the target, long putts and chips, over several holes |
| 04 | Pause and quit: pause mid-hole, try each pause-menu entry, then quit the round |
| 05 | Character select: lefty (hold L or Z while choosing) and colour changes (hold a C button) on several characters, then one hole |
| 06 | Ring Shot: the first course, until a star is earned or 3 holes are played |
| 07 | Mini-Golf: one player, 4 to 6 holes |
| 08 | Speed Golf: Toad Highlands, the front nine (about 15 to 20 minutes) |
| 09 | Get Character against Luigi, to the end of the match (up to about 30 minutes; can be split at a hole if the game allows it) |
| 10 | Tournament at Toad Highlands, to the end (about 30 minutes); its points and badges go into the save |
| 11 | The save: start from 10's save, check the records, stats and unlocks, then play one hole |

Sessions 09 and 10 are long, but they are the only way to reach the game's
unlock and course-point screens. The recorder takes only the first
controller, so the 2 to 4 player modes are left for later.
