/*
 * The end of main's bss: every object that the original's linker allocated
 * from main's common block, defined here in the original's address order. Only
 * the matching build compiles it.
 *
 * The original main segment was one relocatable, whose commons its linker
 * laid out in the order of a 4051-bucket symbol hash table, a layout no
 * per-object link can reproduce from the files that use them. Defining them
 * in .bss here, in that order, fixes their addresses whatever their names,
 * while their types still give their sizes. Each object's own module, game or
 * library, defines it too, as a common that binds to the definition here.
 * Other builds place each object with its module instead.
 */

#include <PRinternal/controller.h>
#include <PRinternal/macros.h>
#include <PRinternal/osint.h>
#include <nusys.h>
#include <player_fx.h>
#include <ultra64.h>

#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/billboard.h"
#include "course/course.h"
#include "course/hole_loader.h"
#include "course/pin.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "math/geometry.h"
#include "menus/main_menu.h"
#include "menus/menu.h"
#include "menus/menu_memory.h"
#include "menus/password.h"
#include "round/bets.h"
#include "round/fireworks.h"
#include "round/game_state.h"
#include "round/play.h"
#include "round/player.h"
#include "round/round_hud.h"
#include "round/round_stats.h"
#include "round/scorecard.h"
#include "round/tournament.h"
#include "round/turn.h"
#include "save/course_stats.h"
#include "save/play_record.h"
#include "save/save.h"
#include "screen/animated_box.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/ball_simulation.h"
#include "shot/club_panel.h"
#include "shot/current_shot.h"
#include "shot/particle_effects.h"
#include "shot/shot_effects.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/crash_screen.h"
#include "system/main.h"
#include "system/overlay_manager.h"
#include "text/text.h"

// KMC GCC 2.7.2 leaves an uninitialized variable to the common block, and
// honors a section attribute only on an initialized one, so each object is
// zero-initialized into .bss
#define BSS __attribute__((section(".bss")))

// A common of the original's that no routine reads, so whose object is
// unknown. It is named after its line, as it holds only its place.
#define UnreadCommonName(line) UnreadCommonNameAtLine(line)
#define UnreadCommonNameAtLine(line) f_UnreadCommon##line
#define UNREAD_COMMON(size) static u8 UnreadCommonName(__LINE__)[(size)] UNUSED BSS = {0}

// Each object keeps the alignment that the original's assembler gave it as a
// common under --force-n64align: 8 bytes from 0x10 bytes long, unless its
// address shows otherwise. The assembler gave those from 0x400 bytes long 16,
// which KMC GCC can't ask for, as it aligns no object past 8. The order of
// the definitions puts them there instead.

int f_RandomState BSS = {0};
UNREAD_COMMON(0x8);
TakenCharacters g_TakenCharacters BSS ALIGNED(8) = {0};
u8 nuScPreNMIFlag BSS = {0};
NUContData g_ControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
ContestMovieState g_ContestMovieState BSS = {0};
LabelGrowthDirection g_ChosenLabelGrowthDirection BSS = {0};
ControllerState g_ControllerStates[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
f32 g_CurrentAnimationFrame BSS = {0};
ImageAsset *g_TrophyFrameBuffers[TROPHY_FRAME_BUFFER_COUNT] BSS = {0};
u32 __osBaseCounter BSS = {0};
Boolean8 g_ChosenLefties[PLAYER_COUNT_MAX] BSS = {0};
ImageAsset *g_SkyImage BSS = {0};
NUDebTaskPerf *nuDebTaskPerfPtr BSS = {0};
u8 g_GlyphGroupSizes[GLYPH_GROUP_COUNT] BSS ALIGNED(8) = {0};
s16 g_LastMenuPlayerCountChoice BSS = {0};
ImageAsset *g_MenuSpareImages[MENU_SPARE_IMAGE_COUNT] BSS = {0};
PaletteColor *g_TrophyPalettes[TROPHY_PALETTE_COUNT] BSS = {0};
CurrentShot g_CurrentShot BSS ALIGNED(8) = {0};
Billboard g_Billboards[BILLBOARD_COUNT] BSS ALIGNED(8) = {{0}};
ImageAsset *g_MenuWideSpareImage BSS = {0};
OSThread __osThreadSave BSS ALIGNED(8) = {0};
Boolean8 g_IsModuleAlive[MODULE_COUNT] BSS ALIGNED(8) = {0};
ImageAsset *g_SpinningBadgeImage BSS = {0};
u8 *g_BlendedTextureScrollingLayer BSS = {0};
ShotDebris f_ShotDebris BSS ALIGNED(8) = {0};
s32 f_SpawnGroundHeight BSS = {0};
OSMesgQueue f_TaskDoneQueue BSS ALIGNED(8) = {0};
OSMesgQueue nuContWaitMesgQ BSS ALIGNED(8) = {0};
s32 g_CoarseVertexOffsets[GROUND_TILE_COUNT][TILE_VERTEX_SEGMENT_COUNT] BSS ALIGNED(8) = {{0}};
Mtx f_EffectMatrices[EFFECT_COUNT] BSS ALIGNED(8) = {{{{0}}}};
Mtx g_ScreenEffectProjection BSS ALIGNED(8) = {{{0}}};
s8 g_ReplayStrokeCount BSS = {0};
Boolean g_IsChosenHoleLoadStarted BSS = {0};
FireworkShape *f_FireworkShape BSS = {0};
s16 g_ChosenLabelGrowth BSS = {0};
OSPifRam __osContPifRam BSS ALIGNED(8) = {{0}};
f32 f_RainDriftZ BSS = {0};
CameraView g_SpareCameraView BSS ALIGNED(8) = {{0}};
int errno BSS = {0};
u8 *g_RingTexels BSS = {0};
ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT] BSS ALIGNED(8) = {{0}};
s32 g_CurrentAnimationEndFrame BSS = {0};
UNREAD_COMMON(0x18);
GameBoyTransfer g_GameBoyTransfer BSS ALIGNED(8) = {0};
OSPiHandle __Dom2SpeedParam BSS ALIGNED(0x8) = {0};
ImageAsset *g_LeaderLineImage BSS = {0};
u16 *f_LastFrameBuffer BSS = {0};
u16 *nuGfxZBuffer BSS = {0};
NUContData g_MenuControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
u32 nuContDataLockKey BSS = {0};
PlayRecord f_BestShotReplayRecord BSS ALIGNED(8) = {{{0}}};
NUContData g_PreviousControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
OSContStatus nuContStatus[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
PlayRecord g_PlayRecords[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{{{0}}}};
OSPiHandle *diskrom_handle BSS = {0};
Gfx *g_GroundTileDisplayLists[GROUND_TILE_COUNT] BSS ALIGNED(8) = {0};
ImageAsset *g_SecondAnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT] BSS = {0};
NUScTask f_AudioTask BSS ALIGNED(8) = {0};
u32 __osViIntrCount BSS = {0};
s32 g_CupViewDepth BSS = {0};
GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT] BSS ALIGNED(8) = {{0}};
s32 f_RainCenter[PLANE_AXIS_COUNT] BSS = {0};
ImageAsset *g_MenuBoardImages[MENU_IMAGE_BUFFER_COUNT] BSS = {0};
char f_FaultText[FAULT_TEXT_SIZE] BSS ALIGNED(8) = {0};
s32 g_PerspectiveNormalization BSS = {0};
OSMesg __osGbpakTimerMsg BSS = {0};
RgbColor g_ScreenFadeColor BSS = {0};
u32 nuGfxCfbCounter BSS = {0};
CourseSelectKind g_CourseSelectKind BSS = {0};
ImageAsset *g_CourseTrophyImages[MENU_IMAGE_BUFFER_COUNT] BSS = {0};
ImageAsset *f_RainSplashImage BSS = {0};
RoundStats g_RoundStats BSS ALIGNED(8) = {0};
Boolean8 g_WasDonkeyKongUnlocked BSS = {0};
ImageAsset *g_BackdropPatternImage BSS = {0};
u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT] BSS ALIGNED(8) = {0};
MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
GlyphSprite g_GlyphSprites[GLYPH_SPRITE_COUNT] BSS ALIGNED(8) = {{{{{0}}}}};
ControllerState g_MergedController BSS = {0};
ImageAsset *g_MenuIconImages[MENU_ICON_COUNT] BSS ALIGNED(8) = {0};
s32 g_PlayerControllerPorts[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
UNREAD_COMMON(0x2C);
f32 f_RainDriftX BSS = {0};
Mtx f_RingMatrices[RING_COUNT] BSS ALIGNED(8) = {{{{0}}}};
s32 f_GroundCheckDropIndex BSS = {0};
BallSimulation g_TurnBallSimulation BSS ALIGNED(8) = {{0}};
s32 g_CurrentAnimationStartFrame BSS = {0};
Player g_SavedPlayers[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{{0}}};
volatile u32 nuScRetraceCounter BSS = {0};
u32 nuGfxDisplay BSS = {0};
s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
UBoolean8 g_IsGroundTileNearby[GROUND_TILE_COUNT] BSS ALIGNED(8) = {0};
UNREAD_COMMON(0xC);
Vec3 f_RainVelocity BSS = {0};
NUContRmbCtl nuContRmbCtl[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
CurrentShot g_ScriptedShot BSS ALIGNED(8) = {0};
s32 g_MessageCodesStart BSS = {0};
// Its address is only 4-byte aligned, so it keeps its type's alignment
Scorecard g_Scorecard BSS = {{0}};
char g_PrintBuffer[PRINT_BUFFER_SIZE] BSS ALIGNED(8) = {0};
Firework f_Fireworks[FIREWORK_CAPACITY] BSS ALIGNED(8) = {{0}};
Boolean g_HasOwnControllers BSS = {0};
OSContPad nuContData[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
Mtx *g_CurrentJointMatrices BSS = {0};
ImageAsset *g_DistanceSignImage BSS = {0};
s32 g_ControllerCount BSS = {0};
// Its address is only 4-byte aligned, so it keeps its type's alignment
Camera g_MapCamera BSS = {0};
ImageAsset *g_AnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT] BSS = {0};
u8 __osMaxControllers BSS = {0};
Mtx f_UnusedNarrowProjection BSS ALIGNED(8) = {{{0}}};
u8 g_ConnectedControllerBits BSS = {0};
ImageAsset *g_MenuTitleImages[MENU_IMAGE_BUFFER_COUNT] BSS = {0};
ImageAsset *g_MiniGolfDiscImages[MINI_GOLF_DISC_IMAGE_COUNT] BSS = {0};
volatile s32 g_MainLoopState BSS = {0};
OSMesgQueue nuGfxMesgQ BSS ALIGNED(8) = {0};
UBoolean8 g_IsRingShotWipeOpening BSS = {0};
OSTime __osCurrentTime BSS = {0};
u8 f_GameFlagBits[GAME_FLAG_BYTE_COUNT] BSS ALIGNED(8) = {0};
u32 g_GroundDrawFrame BSS = {0};
u16 g_MirrorTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(8) = {0};
s32 g_HoleTimeFrames[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
s32 g_ChosenPlayerCount BSS = {0};
Vec3 f_FramedEye BSS = {0};
PaletteColor *g_CharacterFacePalettes[CHARACTER_COUNT] BSS ALIGNED(8) = {0};
OSPiHandle __CartRomHandle BSS ALIGNED(0x8) = {0};
CourseStatFigures g_CourseStatFigures[COURSE_COUNT + 1] BSS ALIGNED(8) = {{0}};
TournamentRules g_ClubTournamentRules BSS = {0};
Boolean g_IsNintendoClubTournament BSS = {0};
s32 g_ClubTournamentNameIndex BSS = {0};
s32 g_ClubTournamentNumber BSS = {0};
UNREAD_COMMON(0x40);
HolePoints g_HolePoints BSS ALIGNED(8) = {{{0}}};
Vec3 f_FramedLookAt BSS = {0};
RingShotRecord g_RingShotRecords[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{{{0}}}};
NUContData g_PreviousMenuControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
ImageAsset *g_HoleBadgeImages BSS = {0};
Mtx f_DivotMarkMatrix BSS ALIGNED(8) = {{{0}}};
s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
f32 g_RainSoundVolume BSS = {0};
OSMesgQueue __osPiAccessQueue BSS ALIGNED(0x8) = {0};
NUUcode *nuGfxUcode BSS = {0};
RoundHud g_RoundHud BSS ALIGNED(8) = {0};
OSPiHandle __Dom1SpeedParam BSS ALIGNED(0x8) = {0};
s16 f_NoiseGrids[NOISE_GRID_COUNT][NOISE_GRID_SIZE][NOISE_GRID_SIZE] BSS ALIGNED(8) = {{{0}}};
Boolean8 g_WasMetalMarioUnlocked BSS = {0};
UNREAD_COMMON(0x8);
u64 nuMainStack[NU_MAIN_STACK_SIZE / sizeof(u64)] BSS ALIGNED(8) = {0};
u32 nuContNum BSS = {0};
UNREAD_COMMON(0x8);
u8 nuYieldBuf[NU_GFX_YIELD_BUF_SIZE] BSS ALIGNED(8) = {0};
UNREAD_COMMON(0x80);
s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
u8 g_TextPalette[TEXT_PALETTE_COLOR_COUNT][RGB_CHANNEL_COUNT] BSS = {{0}};
UNREAD_COMMON(0x8);
u8 nuRDPOutputBuf[NU_GFX_RDP_OUTPUTBUFF_SIZE] BSS ALIGNED(8) = {0};
s32 g_ModuleRdpRequestFrames[MODULE_COUNT] BSS ALIGNED(8) = {0};
UNREAD_COMMON(0x8);
Player g_BackedUpPlayers[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{{0}}};
WideAssetId f_LastOpenedAssetId BSS = {0};
ImageAsset *g_BrickWallImage BSS = {0};
Boolean f_HasRumblePak[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
ImageAsset *g_SharedEffectImages BSS = {0};
Boolean8 g_HadEveryRingShotStar BSS = {0};
s16 g_BetTallies[PLAYER_COUNT_MAX][BET_COUNT] BSS ALIGNED(8) = {{0}};
u8 g_TournamentCode[TOURNAMENT_CODE_SIZE] BSS = {0};
OSMesgQueue nuSiMgrMesgQ BSS ALIGNED(8) = {0};
ImageAsset *g_MenuLabelImages[MENU_IMAGE_BUFFER_COUNT] BSS = {0};
ImageAsset *g_ShotPreviewDashImage BSS = {0};
ImageAsset *g_CharacterFaceImages[CHARACTER_COUNT] BSS ALIGNED(8) = {0};
volatile u32 nuGfxTaskSpool BSS = {0};
// Its address is only 4-byte aligned, so it keeps its type's alignment
u8 g_UnreadMenuBuffer[0x1218] BSS = {0};
// Its address is only 4-byte aligned, so it keeps its type's alignment
u8 g_UnreadPlayBuffer[0xDEC] BSS = {0};
s32 g_ChosenCharacters[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {0};
OSTimer __osGbpakTimer BSS ALIGNED(8) = {0};
ImageAsset *g_ModeIconImages[MENU_ICON_COUNT] BSS ALIGNED(8) = {0};
u8 __osContLastCmd BSS = {0};
UNREAD_COMMON(0x4);
volatile u32 nuScGfxTaskStartRetrace BSS = {0};
OSPiHandle *nuPiSramHandle BSS = {0};
OSMesg __osEepromTimerMsg BSS = {0};
ImageAsset *g_ShadowedFontImage BSS = {0};
char g_SaveBuildDate[SAVE_BUILD_DATE_SIZE] BSS ALIGNED(8) = {0};
TextSlot g_TextSlots[TEXT_SLOT_COUNT] BSS ALIGNED(8) = {{0}};
SaveData g_SaveData BSS ALIGNED(8) = {0};
OSMesgQueue f_RetraceQueue BSS ALIGNED(8) = {0};
char g_TextName[TEXT_NAME_SIZE] BSS ALIGNED(8) = {0};
PaletteColor *g_PlayerFacePalettes[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {0};
OSTimer __osBaseTimer BSS ALIGNED(8) = {0};
Boolean8 g_WasMapleUnlocked BSS = {0};
ImageAsset *g_ClubPanelImage BSS = {0};
CurrentShot g_SavedShots[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{0}};
ImageAsset *g_PlayerTagImages BSS = {0};
u32 __osTimerCounter BSS = {0};
unsigned long __muscontrol_flag BSS = {0};
AnimatedBox *f_QueuedBoxes[QUEUED_BOX_CAPACITY] BSS ALIGNED(8) = {0};
NUIdleFunc nuIdleFunc BSS = {0};
MenuMemory g_MenuMemory BSS ALIGNED(8) = {0};
u8 *g_NoiseTexels BSS = {0};
CameraMatrices f_CameraMatrices[2 * 2] BSS ALIGNED(8) = {{{{{0}}}}};
s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1] BSS ALIGNED(8) = {0};
u8 *g_BlendedTextureStillLayer BSS = {0};
s32 g_BallFlightStep BSS = {0};
UNREAD_COMMON(0x8);
WorkArea g_WorkArea BSS ALIGNED(8) = {{0}};
UNREAD_COMMON(0x8);
Camera g_Cameras[CAMERA_COUNT] BSS ALIGNED(8) = {{0}};
char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1] BSS ALIGNED(8) = {0};
OSPiHandle *nuPiCartHandle BSS = {0};
ControllerState g_TurnController BSS = {0};
ImageAsset *g_PinFlagImages BSS = {0};
ImageAsset *g_PlayerFaceImages[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {0};
GameState g_BackedUpGameState BSS ALIGNED(8) = {0};
BetSet g_BetSet BSS = {0};
Mtx g_ScreenOrthoMatrix BSS ALIGNED(8) = {{{0}}};
UNREAD_COMMON(0x8);
ShotLaunch g_LastShotLaunch BSS ALIGNED(8) = {0};
ALGlobals __libmus_alglobals BSS ALIGNED(8) = {{0}};
UNREAD_COMMON(0x4);
s16 (*f_NoiseGrid)[NOISE_GRID_SIZE] BSS = {0};
UNREAD_COMMON(0x8);
NUDebTaskPerf nuDebTaskPerf[NU_DEB_PERF_BUF_NUM] BSS ALIGNED(8) = {{0}};
ImageAsset *g_ImageSlots[IMAGE_SLOT_COUNT] BSS ALIGNED(8) = {0};
NUScTask nuGfxTask[NU_GFX_TASK_NUM] BSS ALIGNED(8) = {{0}};
s8 g_RingShotCourseOpenedAtStart BSS = {0};
UNREAD_COMMON(0x8);
GameState g_GameState BSS ALIGNED(8) = {0};
UNREAD_COMMON(0x4);
ImageAsset *g_NamePlateImage BSS = {0};
UNREAD_COMMON(0x8);
u16 g_BlendedTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(8) = {0};
volatile u32 nuScRetraceTotal BSS = {0};
__OSInode __osPfsInodeCache BSS ALIGNED(0x8) = {{{{0}}}};
u16 *nuGfxCfb_ptr BSS = {0};
ImageAsset *g_ComputerLeftyIcons BSS = {0};
UNREAD_COMMON(0x680);
s32 g_ChoosingPlayerIndex BSS = {0};
__OSEventState __osEventStateTab[OS_NUM_EVENTS] BSS ALIGNED(0x8) = {{0}};
UBoolean8 g_IsMenuCharacterLoaded[MAXCONTROLLERS] BSS = {0};
OSMesgQueue __osGbpakTimerQ BSS ALIGNED(0x8) = {0};
u16 **nuGfxCfb BSS = {0};
TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT] BSS ALIGNED(8) = {{0}};
s16 g_LastMenuScreen BSS = {0};
Player g_Players[PLAYER_COUNT_MAX] BSS ALIGNED(8) = {{{0}}};
REVERB_MEM gReverbFx BSS ALIGNED(8) = {0};
u32 __osFinalrom BSS = {0};
Effect f_Effects[EFFECT_COUNT] BSS ALIGNED(8) = {{{0}}};
u8 *g_ShadowTexture BSS = {0};
u16 g_ScreenEffectPerspectiveNormalization BSS = {0};
UNREAD_COMMON(0x8);
OSPifRam __osPfsPifRam BSS ALIGNED(8) = {{0}};
ScreenRectangle g_HudScissor BSS = {0};
SkyState g_SkyState BSS ALIGNED(8) = {{0}};
InsetView g_InsetView BSS ALIGNED(8) = {0};
u8 nuDramStack[SP_DRAM_STACK_SIZE8] BSS ALIGNED(8) = {0};
NUSched nusched BSS ALIGNED(8) = {0};
OSMesgQueue __osEepromTimerQ BSS ALIGNED(0x8) = {0};
OSPfs nuContPfs[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {{0}};
RainSplash *f_RainSplashes BSS = {0};
u16 g_FlameTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(8) = {0};
OSMesgQueue nuSiMesgQ BSS ALIGNED(8) = {0};
Vtx *g_VertexListHead BSS = {0};
Gfx *g_DisplayListHead BSS = {0};
ImageAsset *g_MenuArrowSizedImages[MENU_ARROW_SIZED_IMAGE_COUNT] BSS ALIGNED(8) = {0};
OSMesgQueue nuGfxTaskMgrMesgQ BSS ALIGNED(8) = {0};
UBoolean8 g_IsMenuWipeOpening BSS = {0};
Mtx f_DropMatrices[DRAW_BUFFER_COUNT] BSS ALIGNED(8) = {{{{0}}}};
UNREAD_COMMON(0x8);
Gfx g_DisplayLists[2][DISPLAY_LIST_LENGTH] BSS ALIGNED(8) = {{{{0}}}};
MenuScreen g_MenuScreen BSS = {0};
ImageAsset *g_CourseSkyImage BSS = {0};
Raindrop *f_Raindrops BSS = {0};
OSMesgQueue __osSiAccessQueue BSS ALIGNED(0x8) = {0};
UNREAD_COMMON(0x8);
BillboardTransform g_BillboardTransforms[BILLBOARD_COUNT] BSS ALIGNED(8) = {{0}};
OSMesgQueue nuContDataMutexQ BSS ALIGNED(8) = {0};
UNREAD_COMMON(0x8);
CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT] BSS ALIGNED(8) = {{0}};
ImageAsset *g_MenuCourseSceneImage BSS = {0};
