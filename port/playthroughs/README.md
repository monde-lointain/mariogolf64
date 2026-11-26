# Playthroughs

This folder holds sessions of the game played by hand in the PC port and
recorded with `--record`. Each file is an input script that you can replay
headless: under the sanitizers, on another host, or after a change, to
check that the game still reaches the same save.

`NOTES.md` lists what looked wrong while each session was recorded, and
what became of each note.

## Before you begin

- Build the port, as [`port/README.md`](../README.md#build-the-port)
  describes. The examples use the `linux` preset.
- Get the save that the session starts from. Each script's second line
  names it by its CRC-32. The repository doesn't keep saves, so ask the
  person who recorded the session. A replay from any other save soon goes
  its own way.

## Replay a session

1.  Copy the session's start save, so that the replay doesn't change it:

    ```sh
    cp 01-start.sra 01-replay.sra
    ```

1.  Replay the session headless, to the frame on the script's last line,
    `# Ends at frame N`:

    ```sh
    build/pc/linux/port/src/mariogolf64 --headless --save 01-replay.sra \
        --input-script port/playthroughs/01-opening-and-menus.txt \
        --game-frames 8305 baserom.us.z64
    ```

1.  Compare the save with the one that the recording left:

    ```sh
    cmp 01-replay.sra 01-play.sra
    ```

A headless replay keeps the game in step with the video retraces, so it
makes the same save on every build and host. The sanitizer presets and
the Windows build under Wine match the recordings. A save holds only what
the game keeps, so a match doesn't prove that every frame drew the same.
To check a frame, add `--screenshot PATH`.

### Why a replay in real time can differ

Compare saves only from headless replays. A replay in a window, or on a
phone, can end differently, and session 03's does, the same way on every
host tried.

The thread that builds a hole runs while the game's thread waits for a
retrace, and both take numbers from the C library's random number
generator. Which of them takes a number first depends on how long the
frames take. In session 03, in real time, the builder takes the numbers
for a hole's ground a frame later. From then on, each thread gets other
numbers: other raindrops, and in the end another save.

## Record a session

1.  Copy the save that you want to start from. Never record on the save
    that you play on:

    ```sh
    cp ~/.local/share/mariogolf64/mariogolf64/mariogolf64.sra 12-start.sra
    cp 12-start.sra 12-play.sra
    ```

1.  Play the session in a window, recording the first controller:

    ```sh
    build/pc/linux/port/src/mariogolf64 --save 12-play.sra \
        --record port/playthroughs/12-my-goal.txt baserom.us.z64
    ```

1.  When you reach the session's goal, close the window.
1.  Add a row for the session to the table below, and add what looked
    wrong, with about when it happened, to `NOTES.md`.
1.  Keep `12-start.sra` and `12-play.sra` outside the repository, so that
    you can replay the session later.

## Sessions

Each session has one goal, and takes 5 to 15 minutes unless it says
otherwise. Sessions 01 to 10 start from the same save, with CRC-32
1F93B89D. Session 11 starts from another, with CRC-32 7B17B522.

| Script                     | Goal                                                                                                                                       |
|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| `01-opening-and-menus.txt` | Watch the opening and the attract hole through to the title, then open every main menu entry and back out of each, without playing.        |
| `01b-options.txt`          | Change settings in Options, answer No and then Yes to the Erase Data question, and back out.                                               |
| `02-first-holes.txt`       | Play 3 holes of Stroke on Toad Highlands with Plum, using club changes, the landing view (R), the overview (Z+R), and the shot replay (B). |
| `03-shot-controls.txt`     | Over several holes, use spin (Z and the stick), power shots, target moves, long putts, and chips.                                          |
| `04-pause-menu.txt`        | Pause in the middle of a hole, try each pause menu entry, and then quit the round.                                                         |
| `05-character-select.txt`  | Choose left-handed players (hold L or Z while choosing) and other colors (hold a C button) for several characters, then play one hole.     |
| `06-ring-shot.txt`         | Play Ring Shot on the first course, until you earn a star or play 3 holes.                                                                 |
| `07-mini-golf.txt`         | Play 4 to 6 holes of Mini-Golf with one player.                                                                                            |
| `08-speed-golf.txt`        | Play Speed Golf on the front nine of Toad Highlands. Takes 15 to 20 minutes.                                                               |
| `09-get-character.txt`     | Play Get Character against Luigi to the end of the match. Takes up to 30 minutes.                                                          |
| `10-toad-tournament.txt`   | Play the Tournament at Toad Highlands to the end, which adds its points and badges to the save. Takes about 30 minutes.                    |
| `11-save.txt`              | Check the records, statistics, and unlocks in the save, then play one hole.                                                                |

Sessions 09 and 10 are long, but they are the only way to reach the
screens for unlocks and course points. The recorder records only the first
controller, so no session plays the modes for 2 to 4 players.
