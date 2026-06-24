#include "common.h"

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", nuAuStlMgrInit);

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", nuAuStlSchedInstall);

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", nuAuStlSchedWaitFrame);

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", nuAuStlSchedDoTask);

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", bgm_alloc_song_buffer);

INCLUDE_ASM("asm/nonmatchings/main/audio_mgr", bgm_start_current);
