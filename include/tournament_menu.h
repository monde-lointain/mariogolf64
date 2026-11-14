#ifndef TOURNAMENT_MENU_H
#define TOURNAMENT_MENU_H

#include <ultra64.h>

#include "character.h"
#include "system/c_language.h"

enum TournamentBuffer {
    TOURNAMENT_BUFFER_WIDE_CLOUD = 1,
    TOURNAMENT_BUFFER_YES_NO,
    TOURNAMENT_BUFFER_NINTENDO_BADGE,
    TOURNAMENT_BUFFER_ICON,
    TOURNAMENT_BUFFER_MENU_WINDOW,
    TOURNAMENT_BUFFER_ROW_FILE
};

typedef s16 TournamentBuffer;

enum CodeEntryBuffer {
    CODE_ENTRY_BUFFER_NAME_PLATE = 1,
    CODE_ENTRY_BUFFER_BAR,
    CODE_ENTRY_BUFFER_GLYPHS,
    CODE_ENTRY_BUFFER_POINTING_HAND,
    CODE_ENTRY_BUFFER_CURSOR_BOX
};

typedef s32 CodeEntryBuffer;

void LoadTournamentBuffer(void *buffer, TournamentBuffer bufferKind);

#define NO_OK_TEXT (-1)
void DrawYesNoWindow(
    Gfx **displayListHead,
    s32 width,
    s32 height,
    s32 centerX,
    s32 centerY,
    s32 okTextShown,
    s32 chosenButton
);
void LoadCodeEntryBuffer(void *buffer, CodeEntryBuffer bufferKind);
void ResetCodeEntry(void);
void UpdateClubTournament(void);
void DrawClubTournament(Gfx **displayListHead);
CharacterIndex GetClubTournamentCharacter(void);
Boolean TakeClubTournamentEntry(void);

#endif
