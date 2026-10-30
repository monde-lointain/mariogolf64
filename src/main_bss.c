/*
 * The end of main's bss: every object that the original's linker allocated
 * from main's common block, defined here in the original's address order.
 *
 * The original main segment was one relocatable, whose commons its linker
 * laid out in the order of a 4051-bucket symbol hash table, a layout no
 * per-object link can reproduce from the files that use them. Defining them
 * in .bss here, in that order, fixes their addresses whatever their names,
 * while their types still give their sizes. The game's own files still
 * declare the objects they use, and the library ones keep their commons,
 * which bind to these definitions.
 */
#include "common.h"

#include <PRinternal/controller.h>
#include <PRinternal/macros.h>
#include <PRinternal/osint.h>
#include <lib/libnusys/nusys.h>
#include <player_fx.h>
#include <ultra64.h>

#include "animated_box.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "controller.h"
#include "current_shot.h"
#include "effects.h"
#include "fault.h"
#include "fireworks.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "main.h"
#include "main_menu.h"
#include "menu.h"
#include "menu_memory.h"
#include "overlay_manager.h"
#include "password.h"
#include "round_hud.h"
#include "save.h"
#include "scorecard.h"
#include "screen.h"
#include "shot_camera.h"
#include "terrain.h"
#include "text.h"
#include "tournament.h"
#include "transfer_pak.h"
#include "variables.h"
#include "work_area.h"

/*
 * KMC GCC 2.7.2 leaves an uninitialized variable to the common block, and
 * honors a section attribute only on an initialized one, so each object is
 * zero-initialized into .bss
 */
#define BSS __attribute__((section(".bss")))

/*
 * Each object keeps the alignment that the original's assembler gave it as a
 * common under --force-n64align: 8 bytes from 0x10 bytes long, and 16 from
 * 0x400, unless its address shows otherwise
 */

int f_RandomState BSS = {0};
/* Read by no routine */
static u8 D_800FBD38[0x8] BSS = {0};
TakenCharacters g_TakenCharacters BSS ALIGNED(8) = {0};
u8 nuScPreNMIFlag BSS = {0};
NUContData g_ControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
s8 D_800FBDB8 BSS = {0};
LabelGrowthDirection g_ChosenLabelGrowthDirection BSS = {0};
ControllerState g_ControllerStates[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
f32 g_CurrentAnimationFrame BSS = {0};
void* D_800FBDFC[2] BSS = {0};
u32 __osBaseCounter BSS = {0};
s8 D_800FBE08[4] BSS = {0};
u8* g_SkyImage BSS = {0};
NUDebTaskPerf* nuDebTaskPerfPtr BSS = {0};
u8 g_GlyphGroupSizes[GLYPH_GROUP_COUNT] BSS ALIGNED(8) = {0};
s16 D_800FBE2C BSS = {0};
/* Two buffers, each twice the size of a Toad image, that menu screens read
 * their own images into */
void* g_MenuSpareImages[MENU_SPARE_IMAGE_COUNT] BSS = {0};
void* D_800FBE38[COURSE_FILE_COUNT] BSS = {0};
CurrentShot g_CurrentShot BSS ALIGNED(8) = {0};
Billboard g_Billboards[BILLBOARD_COUNT] BSS ALIGNED(16) = {0};
void* D_800FC6A0 BSS = {0};
OSThread __osThreadSave BSS ALIGNED(8) = {0};
s8 g_IsModuleAlive[MODULE_COUNT] BSS ALIGNED(8) = {0};
void* D_800FC898 BSS = {0};
u8* g_BlendedTextureScrollingLayer BSS = {0};
ShotDebris f_ShotDebris BSS ALIGNED(8) = {0};
s32 f_SpawnGroundHeight BSS = {0};
OSMesgQueue f_TaskDoneQueue BSS ALIGNED(8) = {0};
OSMesgQueue nuContWaitMesgQ BSS ALIGNED(8) = {0};
s32 g_CoarseVertexOffsets[GROUND_TILE_COUNT][TILE_VERTEX_SEGMENT_COUNT] BSS
    ALIGNED(16) = {0};
Mtx f_EffectMatrices[EFFECT_COUNT] BSS ALIGNED(16) = {0};
Mtx g_ScreenEffectProjection BSS ALIGNED(8) = {0};
u8 D_800FE330 BSS = {0};
s32 D_800FE334 BSS = {0};
FireworkShape* f_FireworkShape BSS = {0};
s16 g_ChosenLabelGrowth BSS = {0};
OSPifRam __osContPifRam BSS ALIGNED(8) = {0};
f32 f_RainDriftZ BSS = {0};
CameraView g_SpareCameraView BSS ALIGNED(8) = {0};
int errno BSS = {0};
u8* g_RingTexels BSS = {0};
ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT] BSS ALIGNED(
    8) = {0};
s32 g_CurrentAnimationEndFrame BSS = {0};
/* Read by no routine */
static u8 D_800FE438[0x18] BSS = {0};
GameBoyTransfer g_GameBoyTransfer BSS ALIGNED(16) = {0};
OSPiHandle __Dom2SpeedParam BSS ALIGNED(0x8) = {0};
void* g_LeaderLineImage BSS = {0};
u16* f_LastFrameBuffer BSS = {0};
u16* nuGfxZBuffer BSS = {0};
NUContData g_MenuControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
u32 nuContDataLockKey BSS = {0};
PlayRecord D_800FED40 BSS ALIGNED(8) = {0};
NUContData g_PreviousControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
OSContStatus nuContStatus[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {0};
PlayRecord D_800FEDD8[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
OSPiHandle* diskrom_handle BSS = {0};
Gfx* g_GroundTileDisplayLists[GROUND_TILE_COUNT] BSS ALIGNED(8) = {0};
/* A second animated icon's two frame buffers, the size of a mode icon, which
 * some screens read other images into */
void* g_SecondAnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT] BSS = {0};
NUScTask f_AudioTask BSS ALIGNED(8) = {0};
u32 __osViIntrCount BSS = {0};
s32 g_CupViewDepth BSS = {0};
GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT] BSS ALIGNED(
    8) = {0};
s32 f_RainCenter[2] BSS = {0};
void* g_MenuBoardImages[2] BSS = {0};
char f_FaultText[FAULT_TEXT_SIZE] BSS ALIGNED(8) = {0};
s32 g_PerspectiveNormalization BSS = {0};
OSMesg __osGbpakTimerMsg BSS = {0};
s32 g_ScreenFadeRed BSS = {0};
s32 g_ScreenFadeGreen BSS = {0};
s32 g_ScreenFadeBlue BSS = {0};
u32 nuGfxCfbCounter BSS = {0};
/* Whether the course select names the courses or the tournaments */
CourseSelectKind g_CourseSelectKind BSS = {0};
void* D_800FF4C4[2] BSS = {0};
u8* f_RainSplashImage BSS = {0};
RoundStats g_RoundStats BSS ALIGNED(8) = {0};
s8 g_WasDonkeyKongUnlocked BSS = {0};
void* g_BackdropPatternImage BSS = {0};
u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT] BSS ALIGNED(8) = {0};
MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
GlyphSprite g_GlyphSprites[GLYPH_SPRITE_COUNT] BSS ALIGNED(16) = {0};
ControllerState g_MergedController BSS = {0};
/* The menu screen's course or mode icons, whose buffers other screens reuse
 * for their own images */
void* g_MenuIconImages[7] BSS ALIGNED(8) = {0};
s32 g_PlayerControllerPorts[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801049D0[0x2C] BSS = {0};
f32 f_RainDriftX BSS = {0};
Mtx f_RingMatrices[RING_COUNT] BSS ALIGNED(8) = {0};
s32 f_GroundCheckDropIndex BSS = {0};
BallSimulation g_TurnBallSimulation BSS ALIGNED(8) = {0};
s32 g_CurrentAnimationStartFrame BSS = {0};
Player g_SavedPlayers[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
volatile u32 nuScRetraceCounter BSS = {0};
u32 nuGfxDisplay BSS = {0};
s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
u8 g_IsGroundTileNearby[GROUND_TILE_COUNT] BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_80104F38[0xC] BSS = {0};
Vec3 f_RainVelocity BSS = {0};
NUContRmbCtl nuContRmbCtl[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {0};
CurrentShot D_80104F78 BSS ALIGNED(8) = {0};
s32 g_MessageCodesStart BSS = {0};
/* Its address is only 4-byte aligned, so it keeps its type's alignment */
Scorecard g_Scorecard BSS = {0};
char g_PrintBuffer[PRINT_BUFFER_SIZE] BSS ALIGNED(8) = {0};
Firework f_Fireworks[FIREWORK_CAPACITY] BSS ALIGNED(8) = {0};
s32 g_HasOwnControllers BSS = {0};
OSContPad nuContData[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {0};
Mtx* g_CurrentJointMatrices BSS = {0};
void* g_DistanceSignImage BSS = {0};
s32 g_ControllerCount BSS = {0};
/* Its address is only 4-byte aligned, so it keeps its type's alignment */
Camera g_MapCamera BSS = {0};
/* The animated icon's two frame buffers, the size of a mode icon, which some
 * screens read other images into */
void* g_AnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT] BSS = {0};
u8 __osMaxControllers BSS = {0};
Mtx D_80105278 BSS ALIGNED(8) = {0};
u8 g_ConnectedControllerBits BSS = {0};
void* g_MenuTitleImages[2] BSS = {0};
void* D_801052C4[2] BSS = {0};
volatile s32 g_MainLoopState BSS = {0};
OSMesgQueue nuGfxMesgQ BSS ALIGNED(8) = {0};
s8 D_801052E8 BSS = {0};
OSTime __osCurrentTime BSS = {0};
u8 f_GameFlagBits[GAME_FLAG_COUNT / 8] BSS ALIGNED(8) = {0};
u32 g_GroundDrawFrame BSS = {0};
u16 g_MirrorTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(
    16) = {0};
s32 g_HoleTimeFrames[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
s32 g_ChosenPlayerCount BSS = {0};
Vec3 f_FramedEye BSS = {0};
void* D_80105B78[18] BSS ALIGNED(8) = {0};
OSPiHandle __CartRomHandle BSS ALIGNED(0x8) = {0};
CourseStatFigures g_CourseStatFigures[COURSE_COUNT + 1] BSS ALIGNED(8) = {0};
TournamentRules D_80105DC0 BSS = {0};
s32 D_80105DC4 BSS = {0};
s32 D_80105DC8 BSS = {0};
s32 D_80105DCC BSS = {0};
/* Read by no routine */
static u8 D_80105DD0[0x40] BSS = {0};
HolePoints g_HolePoints BSS ALIGNED(8) = {0};
Vec3 f_FramedLookAt BSS = {0};
RingShotRecord D_80105F40[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
NUContData g_PreviousMenuControllerData[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
void* D_80106100 BSS = {0};
Mtx f_DivotMarkMatrix BSS ALIGNED(8) = {0};
s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
f32 g_RainSoundVolume BSS = {0};
OSMesgQueue __osPiAccessQueue BSS ALIGNED(0x8) = {0};
NUUcode* nuGfxUcode BSS = {0};
RoundHud g_RoundHud BSS ALIGNED(8) = {0};
OSPiHandle __Dom1SpeedParam BSS ALIGNED(0x8) = {0};
s16 f_NoiseGrids[NOISE_GRID_COUNT][NOISE_GRID_SIZE][NOISE_GRID_SIZE] BSS
    ALIGNED(16) = {0};
s8 g_WasMetalMarioUnlocked BSS = {0};
/* Read by no routine */
static u8 D_8010A2C8[0x8] BSS = {0};
u64 nuMainStack[NU_MAIN_STACK_SIZE / sizeof(u64)] BSS ALIGNED(16) = {0};
u32 nuContNum BSS = {0};
/* Read by no routine */
static u8 D_8010C2D8[0x8] BSS = {0};
u8 nuYieldBuf[NU_GFX_YIELD_BUF_SIZE] BSS ALIGNED(16) = {0};
u8 D_8010CEE0[0x80] BSS ALIGNED(8) = {0};
s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT] BSS ALIGNED(8) = {0};
u8 g_TextPalette[TEXT_PALETTE_COLOR_COUNT][3] BSS = {0};
/* Read by no routine */
static u8 D_8010CFB8[0x8] BSS = {0};
u8 nuRDPOutputBuf[NU_GFX_RDP_OUTPUTBUFF_SIZE] BSS ALIGNED(16) = {0};
s32 g_ModuleRdpRequestFrames[MODULE_COUNT] BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_8012D0C0[0x8] BSS = {0};
Player D_8012D0C8[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
s32 f_LastOpenedAssetId BSS = {0};
void* D_8012D3AC BSS = {0};
Boolean f_HasRumblePak[MAXCONTROLLERS] BSS ALIGNED(8) = {0};
u8* g_SharedEffectImages BSS = {0};
s8 g_HadEveryRingShotStar BSS = {0};
s16 g_BetTallies[MAX_PLAYER_COUNT][BET_COUNT] BSS ALIGNED(8) = {0};
u8 g_TournamentCode[TOURNAMENT_CODE_SIZE] BSS = {0};
OSMesgQueue nuSiMgrMesgQ BSS ALIGNED(8) = {0};
void* g_MenuLabelImages[2] BSS = {0};
void* g_ShotPreviewDashImage BSS = {0};
void* D_8012D430[18] BSS ALIGNED(8) = {0};
volatile u32 nuGfxTaskSpool BSS = {0};
/* Its address is only 4-byte aligned, so it keeps its type's alignment */
u8 D_8012D47C[0x1218] BSS = {0};
/* Its address is only 4-byte aligned, so it keeps its type's alignment */
u8 D_8012E694[0xDEC] BSS = {0};
/* The character that each player has chosen on the menus, or
 * NO_CHOSEN_CHARACTER */
s32 g_ChosenCharacters[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
OSTimer __osGbpakTimer BSS ALIGNED(8) = {0};
void* D_8012F4B0[7] BSS ALIGNED(8) = {0};
u8 __osContLastCmd BSS = {0};
/* Read by no routine */
static u8 D_8012F4D0[0x4] BSS = {0};
volatile u32 nuScGfxTaskStartRetrace BSS = {0};
OSPiHandle* nuPiSramHandle BSS = {0};
OSMesg __osEepromTimerMsg BSS = {0};
void* g_ShadowedFontImage BSS = {0};
char g_SaveBuildDate[SAVE_BUILD_DATE_SIZE] BSS ALIGNED(8) = {0};
TextSlot g_TextSlots[TEXT_SLOT_COUNT] BSS ALIGNED(8) = {0};
SaveData g_SaveData BSS ALIGNED(16) = {0};
OSMesgQueue f_RetraceQueue BSS ALIGNED(8) = {0};
char g_TextName[TEXT_NAME_SIZE] BSS ALIGNED(8) = {0};
u16* g_PlayerFacePalettes[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
OSTimer __osBaseTimer BSS ALIGNED(8) = {0};
s8 g_WasMapleUnlocked BSS = {0};
u8* g_ClubPanelImage BSS = {0};
CurrentShot g_SavedShots[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
void* g_PlayerTagImages BSS = {0};
u32 __osTimerCounter BSS = {0};
unsigned long __muscontrol_flag BSS = {0};
AnimatedBox* f_QueuedBoxes[QUEUED_BOX_CAPACITY] BSS ALIGNED(8) = {0};
NUIdleFunc nuIdleFunc BSS = {0};
MenuMemory g_MenuMemory BSS ALIGNED(8) = {0};
u8* g_NoiseTexels BSS = {0};
CameraMatrices f_CameraMatrices[2 * 2] BSS ALIGNED(16) = {0};
s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1] BSS ALIGNED(8) = {
    0};
u8* g_BlendedTextureStillLayer BSS = {0};
s32 g_BallFlightStep BSS = {0};
/* Read by no routine */
static u8 D_80132CF8[0x8] BSS = {0};
WorkArea g_WorkArea BSS ALIGNED(16) = {0};
/* Read by no routine */
static u8 D_801B54E0[0x8] BSS = {0};
Camera g_Cameras[CAMERA_COUNT] BSS ALIGNED(8) = {0};
char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1] BSS ALIGNED(8) = {0};
OSPiHandle* nuPiCartHandle BSS = {0};
ControllerState g_TurnController BSS = {0};
void* g_PinFlagImages BSS = {0};
u8* g_PlayerFaceImages[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
GameState D_801B55F0 BSS ALIGNED(8) = {0};
BetSet g_BetSet BSS = {0};
Mtx g_ScreenOrthoMatrix BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801B5678[0x8] BSS = {0};
/* Read by no routine: UpdateTurns copies f_ShotLaunch here each frame */
ShotLaunch g_LastShotLaunch BSS ALIGNED(8) = {0};
ALGlobals __libmus_alglobals BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801B56EC[0x4] BSS = {0};
s16 (*f_NoiseGrid)[NOISE_GRID_SIZE] BSS = {0};
/* Read by no routine */
static u8 D_801B56F8[0x8] BSS = {0};
NUDebTaskPerf nuDebTaskPerf[NU_DEB_PERF_BUF_NUM] BSS ALIGNED(16) = {0};
void* g_ImageSlots[IMAGE_SLOT_COUNT] BSS ALIGNED(8) = {0};
NUScTask nuGfxTask[NU_GFX_TASK_NUM] BSS ALIGNED(8) = {0};
s8 g_RingShotCourseOpenedAtStart BSS = {0};
/* Read by no routine */
static u8 D_801B6080[0x8] BSS = {0};
GameState g_GameState BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801B60CC[0x4] BSS = {0};
void* g_NamePlateImage BSS = {0};
/* Read by no routine */
static u8 D_801B60D8[0x8] BSS = {0};
u16 g_BlendedTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(
    16) = {0};
volatile u32 nuScRetraceTotal BSS = {0};
__OSInode __osPfsInodeCache BSS ALIGNED(0x8) = {0};
u16* nuGfxCfb_ptr BSS = {0};
void* D_801B69EC BSS = {0};
/* Read by no routine */
static u8 D_801B69F0[0x680] BSS = {0};
/* The player whose character the character select is choosing */
s32 g_ChoosingPlayerIndex BSS = {0};
__OSEventState __osEventStateTab[OS_NUM_EVENTS] BSS ALIGNED(0x8) = {0};
u8 D_801B70F0[MAXCONTROLLERS] BSS = {0};
OSMesgQueue __osGbpakTimerQ BSS ALIGNED(0x8) = {0};
u16** nuGfxCfb BSS = {0};
TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT] BSS
    ALIGNED(8) = {0};
s16 D_801B71CC BSS = {0};
Player g_Players[MAX_PLAYER_COUNT] BSS ALIGNED(8) = {0};
REVERB_MEM gReverbFx BSS ALIGNED(8) = {0};
u32 __osFinalrom BSS = {0};
Effect f_Effects[EFFECT_COUNT] BSS ALIGNED(16) = {0};
u8* g_ShadowTexture BSS = {0};
u16 g_ScreenEffectPerspNorm BSS = {0};
/* Read by no routine */
static u8 D_801B7EE8[0x8] BSS = {0};
OSPifRam __osPfsPifRam BSS ALIGNED(8) = {0};
ScreenRect g_HudScissor BSS = {0};
SkyState g_SkyState BSS ALIGNED(8) = {0};
InsetView g_InsetView BSS ALIGNED(8) = {0};
u8 nuDramStack[SP_DRAM_STACK_SIZE8] BSS ALIGNED(16) = {0};
NUSched nusched BSS ALIGNED(16) = {0};
OSMesgQueue __osEepromTimerQ BSS ALIGNED(0x8) = {0};
OSPfs nuContPfs[NU_CONT_MAXCONTROLLERS] BSS ALIGNED(8) = {0};
RainSplash* f_RainSplashes BSS = {0};
u16 g_FlameTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] BSS ALIGNED(
    16) = {0};
OSMesgQueue nuSiMesgQ BSS ALIGNED(8) = {0};
Vtx* g_VertexListHead BSS = {0};
Gfx* g_DisplayListHead BSS = {0};
/* Five buffers the size of the green arrows image, which menu screens read
 * other images into */
void* g_MenuArrowSizedImages[MENU_ARROW_SIZED_IMAGE_COUNT] BSS ALIGNED(8) = {0};
OSMesgQueue nuGfxTaskMgrMesgQ BSS ALIGNED(8) = {0};
s8 D_801B9410 BSS = {0};
Mtx f_DropMatrices[DRAW_BUFFER_COUNT] BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801B9498[0x8] BSS = {0};
Gfx g_DisplayLists[2][DISPLAY_LIST_LENGTH] BSS ALIGNED(16) = {0};
s32 g_MenuScreen BSS = {0};
u8* g_CourseSkyImage BSS = {0};
Raindrop* f_Raindrops BSS = {0};
OSMesgQueue __osSiAccessQueue BSS ALIGNED(0x8) = {0};
/* Read by no routine */
static u8 D_801EFFC8[0x8] BSS = {0};
BillboardTransform g_BillboardTransforms[BILLBOARD_COUNT] BSS ALIGNED(16) = {0};
OSMesgQueue nuContDataMutexQ BSS ALIGNED(8) = {0};
/* Read by no routine */
static u8 D_801F43E8[0x8] BSS = {0};
CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT] BSS ALIGNED(16) = {0};
void* g_MenuCourseSceneryAsset BSS = {0};
