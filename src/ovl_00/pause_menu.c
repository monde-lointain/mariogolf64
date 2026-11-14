#include "pause_menu.h"

#include <framebuffer_count.h>
#include <ultra64.h>

#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course.h"
#include "current_shot.h"
#include "game_state.h"
#include "hole_setup.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "particle_effects.h"
#include "play.h"
#include "player.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "sound.h"
#include "surface.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "terrain.h"
#include "text.h"
#include "turn.h"

#define ASSET_CHOICE_BOX_CLOUD_IMAGE 0x71F

enum PauseMenuRequest {
    PAUSE_MENU_REQUEST_FREE = -1,
    PAUSE_MENU_REQUEST_NONE,
    PAUSE_MENU_REQUEST_LOAD
};

typedef s32 PauseMenuRequest;

#define PAUSE_LAYOUT_NONE (-1)
#define PAUSE_LAYOUT_SPEED_GOLF 0
#define PAUSE_LAYOUT_RING_SHOT 1
#define PAUSE_LAYOUT_TRAINING 2
#define PAUSE_LAYOUT_TRAINING_MINI_GOLF 3
#define PAUSE_LAYOUT_DRIVING_RANGE 4
#define PAUSE_LAYOUT_VERSUS 5
#define PAUSE_LAYOUT_STROKE 6
#define PAUSE_LAYOUT_BROUGHT_OVER_SPEED_GOLF 7
#define PAUSE_LAYOUT_BROUGHT_OVER 8
#define PAUSE_LAYOUT_CLUB_TOURNAMENT_SPEED_GOLF 9
#define PAUSE_LAYOUT_CLUB_TOURNAMENT 10

#define PAUSE_MENU_SONG_VOLUME 0x60

enum PauseOptionKind {
    PAUSE_OPTION_KIND_PLAIN,
    PAUSE_OPTION_KIND_WEATHER,
    PAUSE_OPTION_KIND_GRID_COLOR,
    PAUSE_OPTION_KIND_WIND_SPEED,
    PAUSE_OPTION_KIND_WIND_DIRECTION,
    PAUSE_OPTION_KIND_LIE,
    PAUSE_OPTION_KIND_GIVE_UP = 7
};

typedef u16 PauseOptionKind;

#define IsValueOptionKind(kind) \
    ((kind) == PAUSE_OPTION_KIND_WEATHER || \
        (kind) == PAUSE_OPTION_KIND_GRID_COLOR || \
        (kind) == PAUSE_OPTION_KIND_WIND_SPEED || \
        (kind) == PAUSE_OPTION_KIND_WIND_DIRECTION || \
        (kind) == PAUSE_OPTION_KIND_LIE)

enum PauseValueText {
    PAUSE_VALUE_TEXT_CLEAR,
    PAUSE_VALUE_TEXT_RAIN,
    PAUSE_VALUE_TEXT_BLACK,
    PAUSE_VALUE_TEXT_WHITE
};

#define PAUSE_OPTION_GIVE_UP 2

#define PAUSE_LAYOUT_OPTIONS_MAX 9
#define MESSAGE_FIRST_PAUSE_OPTION 0x77

enum PauseMessage {
    PAUSE_MESSAGE_COURSE_VIEW,
    PAUSE_MESSAGE_SCORE_CARD,
    PAUSE_MESSAGE_GIVE_UP,
    PAUSE_MESSAGE_SAVE_AND_QUIT,
    PAUSE_MESSAGE_QUIT_GAME,
    PAUSE_MESSAGE_RETURN_TO_MENU,
    PAUSE_MESSAGE_HOLE_OUT,
    PAUSE_MESSAGE_CHANGE_WIND_DIRECTION,
    PAUSE_MESSAGE_CHANGE_WIND_SPEED,
    PAUSE_MESSAGE_START_OVER,
    PAUSE_MESSAGE_CHANGE_WEATHER,
    PAUSE_MESSAGE_CHANGE_TERRAIN,
    PAUSE_MESSAGE_GRID_COLOR,
    PAUSE_MESSAGE_CONTROLS,
    PAUSE_MESSAGE_FIRST_STROKE,
    PAUSE_MESSAGE_BASIC_CONTROLS
};

typedef s32 PauseMessage;
typedef s16 PauseMessage16;

typedef struct PauseOption {
    PauseMessage16 message;
    PauseAction16 action;
    PauseOptionKind kind;
} PauseOption;

#define CHOICE_BOX_OPTIONS_MAX 4
#define MENU_SOUND_VOLUME 100

enum ChoiceBoxState {
    CHOICE_BOX_CLOSED = -1,
    CHOICE_BOX_CHOOSING,
    CHOICE_BOX_CHOSEN,
    CHOICE_BOX_CLOSING
};

typedef s32 ChoiceBoxState;

enum ChoiceLabel {
    CHOICE_LABEL_YES,
    CHOICE_LABEL_NO,
    CHOICE_LABEL_CONTINUE,
    CHOICE_LABEL_MULLIGAN,
    CHOICE_LABEL_HOLE_OUT,
    CHOICE_LABEL_START_OVER,
    CHOICE_LABEL_RING_SHOT_HOLE_OUT,
    CHOICE_LABEL_RETURN_TO_MENU
};

typedef struct ChoiceBox {
    s32 optionCount;
    s32 halfWidth;
    Boolean isVertical;
    u8 labels[CHOICE_BOX_OPTIONS_MAX];
    u8 results[CHOICE_BOX_OPTIONS_MAX];
} ChoiceBox;

#define CHOICE_BOX_CENTER_X (SCREEN_WIDTH / 2)
#define CHOICE_BOX_MARGIN 32
#define CHOICE_BOX_TOP 72
#define CHOICE_BOX_BOTTOM 164
#define CHOICE_BOX_CLOUD_WIDTH 128
#define CHOICE_BOX_CLOUD_HEIGHT 64
#define CHOICE_BOX_CLOUD_PART_COUNT 1
#define CHOICE_BOX_CLOUD_PART_SIZE 0x1000
#define CHOICE_BOX_CLOUD_DTDY 0x2C8
#define CHOICE_LABELS_MIDDLE_Y 113
#define CHOICE_LABEL_SPACING 16
#define CHOICE_CURSOR_X 124
#define CHOICE_CURSOR_SWAY 8.0f
#define COLOR_WAVE_MIDDLE 127
#define COLOR_WAVE_AMPLITUDE 127.0f

Vec2 g_LiePanelPosition;
Vec2 g_WindPanelPosition;

static s32 f_PauseMenuDrawnMarker = 0;
static s32 f_PauseSelection = 0;

static ChoiceBox f_ChoiceBoxes[] = {
    {
        3,
        36,
        TRUE,
        {CHOICE_LABEL_CONTINUE, CHOICE_LABEL_MULLIGAN, CHOICE_LABEL_HOLE_OUT},
        {CHOICE_RESULT_CONTINUE, CHOICE_RESULT_MULLIGAN, CHOICE_RESULT_HOLE_OUT},
    },
    {
        3,
        42,
        TRUE,
        {CHOICE_LABEL_CONTINUE, CHOICE_LABEL_MULLIGAN, CHOICE_LABEL_RETURN_TO_MENU},
        {CHOICE_RESULT_HOLE_OUT, CHOICE_RESULT_MULLIGAN, CHOICE_RESULT_RETURN_TO_MENU},
    },
    {
        3,
        66,
        TRUE,
        {CHOICE_LABEL_START_OVER, CHOICE_LABEL_RING_SHOT_HOLE_OUT, CHOICE_LABEL_RETURN_TO_MENU},
        {CHOICE_RESULT_START_OVER, CHOICE_RESULT_RING_SHOT_HOLE_OUT, CHOICE_RESULT_RETURN_TO_MENU},
    },
    {
        2,
        36,
        TRUE,
        {CHOICE_LABEL_MULLIGAN, CHOICE_LABEL_HOLE_OUT},
        {CHOICE_RESULT_MULLIGAN, CHOICE_RESULT_HOLE_OUT},
    },
};

static char *f_ChoiceBoxLabels[] =
    {"Yes", "No", "Continue", "Mulligan", "Hole Out", "Start Over", "Hole Out", "Return to Menu"};

static PauseOption f_PauseOptions[] = {
    {PAUSE_MESSAGE_COURSE_VIEW, PAUSE_ACTION_FREE_LOOK, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_SCORE_CARD, PAUSE_ACTION_SCORECARD, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_GIVE_UP, PAUSE_ACTION_GIVE_UP, PAUSE_OPTION_KIND_GIVE_UP},
    {PAUSE_MESSAGE_SAVE_AND_QUIT, PAUSE_ACTION_SAVE_AND_QUIT, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_QUIT_GAME, PAUSE_ACTION_QUIT_GAME, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_RETURN_TO_MENU, PAUSE_ACTION_RETURN_TO_MENU, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_HOLE_OUT, PAUSE_ACTION_HOLE_OUT, PAUSE_OPTION_KIND_PLAIN},
    {
        PAUSE_MESSAGE_CHANGE_WIND_DIRECTION,
        PAUSE_ACTION_CHANGE_WIND_DIRECTION,
        PAUSE_OPTION_KIND_WIND_DIRECTION,
    },
    {PAUSE_MESSAGE_CHANGE_WIND_SPEED, PAUSE_ACTION_CHANGE_WIND_SPEED, PAUSE_OPTION_KIND_WIND_SPEED},
    {PAUSE_MESSAGE_START_OVER, PAUSE_ACTION_START_OVER, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_CHANGE_WEATHER, PAUSE_ACTION_CHANGE_WEATHER, PAUSE_OPTION_KIND_WEATHER},
    {PAUSE_MESSAGE_CHANGE_TERRAIN, PAUSE_ACTION_CHANGE_TERRAIN, PAUSE_OPTION_KIND_LIE},
    {PAUSE_MESSAGE_GRID_COLOR, PAUSE_ACTION_GRID_COLOR, PAUSE_OPTION_KIND_GRID_COLOR},
    {PAUSE_MESSAGE_CONTROLS, PAUSE_ACTION_CONTROLS, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_QUIT_GAME, PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME, PAUSE_OPTION_KIND_PLAIN},
    {PAUSE_MESSAGE_QUIT_GAME, PAUSE_ACTION_QUIT_CLUB_TOURNAMENT, PAUSE_OPTION_KIND_PLAIN},
};

static char *f_PauseValueTexts[] = {"Clear", "Rain", "Black", "White"};

static s8 f_PauseLayoutOptions[][PAUSE_LAYOUT_OPTIONS_MAX] = {
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_QUIT_GAME,
    },
    {
        PAUSE_ACTION_START_OVER,
        PAUSE_ACTION_HOLE_OUT,
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_RETURN_TO_MENU,
    },
    {
        PAUSE_ACTION_START_OVER,
        PAUSE_ACTION_HOLE_OUT,
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_CHANGE_WIND_DIRECTION,
        PAUSE_ACTION_CHANGE_WIND_SPEED,
        PAUSE_ACTION_CHANGE_WEATHER,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_RETURN_TO_MENU,
    },
    {
        PAUSE_ACTION_START_OVER,
        PAUSE_ACTION_HOLE_OUT,
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_CHANGE_WEATHER,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_RETURN_TO_MENU,
    },
    {
        PAUSE_ACTION_HOLE_OUT,
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_CHANGE_TERRAIN,
        PAUSE_ACTION_CHANGE_WIND_DIRECTION,
        PAUSE_ACTION_CHANGE_WIND_SPEED,
        PAUSE_ACTION_CHANGE_WEATHER,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_RETURN_TO_MENU,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_GIVE_UP,
        PAUSE_ACTION_SAVE_AND_QUIT,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_GIVE_UP,
        PAUSE_ACTION_SAVE_AND_QUIT,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_GIVE_UP,
        PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_QUIT_CLUB_TOURNAMENT,
    },
    {
        PAUSE_ACTION_FREE_LOOK,
        PAUSE_ACTION_SCORECARD,
        PAUSE_ACTION_GRID_COLOR,
        PAUSE_ACTION_CONTROLS,
        PAUSE_ACTION_GIVE_UP,
        PAUSE_ACTION_QUIT_CLUB_TOURNAMENT,
    },
};

static s8 f_PauseLayoutOptionCounts[] = {5, 6, 9, 7, 9, 6, 6, 5, 6, 5, 6};

static s32 f_PauseLayout = PAUSE_LAYOUT_NONE;
static Boolean f_IsPauseMenuLoaded = FALSE;
static PauseMenuRequest f_PauseMenuRequest = PAUSE_MENU_REQUEST_NONE;
static s32 f_PauseArrowsTick = 0;
static Radians f_PauseGlowAngle = 0.0f;
static Radians f_PauseCursorAngle = 0.0f;
static ChoiceBoxState f_ChoiceBoxState = CHOICE_BOX_CLOSED;
static s32 f_UnusedWordAfterChoiceBoxState UNUSED = 0;
static ChoiceBoxId f_ChoiceBoxId = CHOICE_BOX_ID_NONE;
static s32 f_ChoiceBoxSelection = 0;
static Radians f_ChoiceGlowAngle = 0.0f;
static Radians f_ChoiceCursorAngle = 0.0f;

static ImageAsset *f_PauseCursorImage;
static ImageAsset *f_MenuCloudImage;
static ImageAsset *f_PauseArrowsImage;
static ImageAsset *f_ChoiceBoxCursorImage;

Boolean f_IsGiveUpAllowed;

void HideRoundHud(void) {
    g_RoundHud.isLetterboxShown = FALSE;
    g_RoundHud.isClubPanelShown = FALSE;
    g_RoundHud.isWindIndicatorShown = FALSE;
    g_RoundHud.areLieArrowsShown = FALSE;
    g_RoundHud.areWindSpeedArrowsShown = FALSE;
    g_RoundHud.areWindDirectionArrowsShown = FALSE;
    g_RoundHud.isHoleInfoShown = FALSE;
    g_RoundHud.isLieIconShown = FALSE;
    g_RoundHud.isClubHeadIconShown = FALSE;
    g_RoundHud.isUnreadItemShown = FALSE;
    g_RoundHud.isProgressShown = FALSE;
}

void ResumeRound(void) {
    g_RoundHud.isLetterboxShown = TRUE;
    g_RoundHud.isClubPanelShown = TRUE;
    g_RoundHud.isWindIndicatorShown = TRUE;
    g_RoundHud.areLieArrowsShown = FALSE;
    g_RoundHud.areWindSpeedArrowsShown = FALSE;
    g_RoundHud.areWindDirectionArrowsShown = FALSE;
    g_RoundHud.isHoleInfoShown = TRUE;
    g_RoundHud.isLieIconShown = TRUE;
    g_RoundHud.isClubHeadIconShown = TRUE;
    g_RoundHud.isUnreadItemShown = TRUE;
    g_RoundHud.isProgressShown = TRUE;
    SetMenuBackdrop(MENU_BACKDROP_NONE);
    g_PauseScreen = PAUSE_SCREEN_NONE;
    SetFramebufferCount(TRIPLE_BUFFERING);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
}

void RequestPauseMenu(Boolean canGiveUp, Boolean isClosing) {
    f_PauseMenuRequest = isClosing == FALSE ? PAUSE_MENU_REQUEST_LOAD : PAUSE_MENU_REQUEST_FREE;
    f_IsGiveUpAllowed = canGiveUp;
}

void LoadPauseMenu(void) {
    EncodedAsset asset;

    if (!f_IsPauseMenuLoaded) {
        f_IsPauseMenuLoaded = TRUE;
        f_PauseSelection = 0;
        if (g_GameState.isClubTournament) {
            if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                f_PauseLayout = PAUSE_LAYOUT_CLUB_TOURNAMENT_SPEED_GOLF;
            }
            else {
                f_PauseLayout = PAUSE_LAYOUT_CLUB_TOURNAMENT;
            }
        }
        else if (g_HasBroughtOverCharacter) {
            if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                f_PauseLayout = PAUSE_LAYOUT_BROUGHT_OVER_SPEED_GOLF;
            }
            else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                f_PauseLayout = PAUSE_LAYOUT_RING_SHOT;
            }
            else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                if (g_GameState.isMiniGolf == TRUE) {
                    f_PauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
                }
                else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
                    f_PauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
                }
                else {
                    f_PauseLayout = PAUSE_LAYOUT_TRAINING;
                }
            }
            else {
                f_PauseLayout = PAUSE_LAYOUT_BROUGHT_OVER;
            }
        }
        else {
            switch (g_GameState.gameMode) {
                case GAME_MODE_SPEED_GOLF:
                    f_PauseLayout = PAUSE_LAYOUT_SPEED_GOLF;
                    break;
                case GAME_MODE_TRAINING:
                    if (g_GameState.isMiniGolf == TRUE) {
                        f_PauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
                    }
                    else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
                        f_PauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
                    }
                    else {
                        f_PauseLayout = PAUSE_LAYOUT_TRAINING;
                    }

                    break;
                case GAME_MODE_RING_SHOT:
                    f_PauseLayout = PAUSE_LAYOUT_RING_SHOT;
                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    f_PauseLayout = PAUSE_LAYOUT_VERSUS;
                    break;
                case GAME_MODE_RECORD_REPLAY:
                    f_PauseLayout = PAUSE_LAYOUT_NONE;
                    break;
                case GAME_MODE_STROKE:
                case GAME_MODE_UNLISTED_STROKE:
                case GAME_MODE_TOURNAMENT:
                case GAME_MODE_UNRECORDED_STROKE:
                    f_PauseLayout = PAUSE_LAYOUT_STROKE;
                    break;
            }
        }

        f_PauseCursorImage = AllocMemory(OpenEncodedAsset(ASSET_POINTING_HAND, &asset));
        ReadEncodedAsset(f_PauseCursorImage, &asset);
        f_MenuCloudImage = AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset));
        ReadEncodedAsset(f_MenuCloudImage, &asset);
        g_WindPanelPosition[AXIS_X] = SCREEN_WIDTH;
        g_WindPanelPosition[AXIS_Y] = SCREEN_HEIGHT;
        g_LiePanelPosition[AXIS_X] = SCREEN_HEIGHT;
        f_PauseArrowsImage = AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS_COPY, &asset));
        ReadEncodedAsset(f_PauseArrowsImage, &asset);
        SetSongVolume(PAUSE_MENU_SONG_VOLUME);
    }
}

void FreePauseMenu(void) {
    if (f_IsPauseMenuLoaded == TRUE) {
        f_IsPauseMenuLoaded = FALSE;
        FreeMemory(&f_PauseCursorImage);
        FreeMemory(&f_MenuCloudImage);
        FreeMemory(&f_PauseArrowsImage);
    }

    SetSongVolume(SOUND_FULL_SCALE);
}

#define PAUSE_PANEL_SHOWN_X 72.0f
#define PAUSE_PANEL_SHOWN_Y 44.0f
#define PAUSE_PANEL_HIDDEN_X 224.0f
#define PAUSE_PANEL_HIDDEN_Y 184.0f
#define PAUSE_PANEL_EASING 0.5f

#define PAUSE_WIND_SPEED_STEP 0x71C7
#define PAUSE_WIND_SPEED_FIRST_STEP 0xAAAA
#define PAUSE_WIND_SPEED_CALM 0x93E8
#define PAUSE_WIND_SPEED_MAX 0x99CF0
#define PAUSE_WIND_SPEED_RAISE_LIMIT 0x9FFFF
#define PAUSE_WIND_TURN_PER_STICK 5
#define PAUSE_WIND_TURN_STEP 0xF0

#define PAUSE_LIE_TEE_UP 0
#define PAUSE_LIE_FAIRWAY 1
#define PAUSE_LIE_FAIRWAY_FAIR_LIE 2
#define PAUSE_LIE_FAIRWAY_POOR_LIE 3
#define PAUSE_LIE_SEMI_ROUGH 4
#define PAUSE_LIE_ROUGH 5
#define PAUSE_LIE_ROUGH_POOR_LIE 6
#define PAUSE_LIE_DEEP_ROUGH 7
#define PAUSE_LIE_BUNKER 8
#define PAUSE_LIE_BUNKER_FAIR_LIE 9
#define PAUSE_LIE_BUNKER_POOR_LIE 10
#define PAUSE_LIE_COUNT 11
#define PAUSE_ROUGH_LIES_OFFSET_X (-0x2D000)
#define PAUSE_DEEP_ROUGH_OFFSET_X (-0x5A000)
#define PAUSE_BUNKER_LIES_OFFSET_X 0x2D000
#define PAUSE_TEE_UP_HEIGHT 307.2f

PauseAction UpdatePauseMenu(ControllerState *controller) {
    PauseAction action;
    PauseOptionKind kind;
    s32 lie;
    Boolean hasLieChanged;
    Surface surface;

    if (controller->repeatedButtons & A_BUTTON) {
        action = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]].action;
        switch (action) {
            case PAUSE_ACTION_QUIT_GAME:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_PROMPT;
                OpenPrompt(PROMPT_QUIT_GAME);
                break;
            case PAUSE_ACTION_QUIT_BROUGHT_OVER_GAME:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_PROMPT;
                OpenPrompt(PROMPT_QUIT_BROUGHT_OVER_GAME);
                break;
            case PAUSE_ACTION_QUIT_CLUB_TOURNAMENT:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_PROMPT;
                OpenPrompt(PROMPT_QUIT_CLUB_TOURNAMENT);
                break;
            case PAUSE_ACTION_SAVE_AND_QUIT:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_LOADING_SAVE_MENU;
                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                g_PendingSubModuleId = MODULE_SAVE_MENU;
                break;
            case PAUSE_ACTION_CONTROLS:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_LOADING_CONTROLS;
                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                g_PendingSubModuleId = MODULE_SAVE_MENU;
                break;
            case PAUSE_ACTION_GIVE_UP:
                if (g_IsDebugEnabled || f_IsGiveUpAllowed) {
                    PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                    RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                    g_PauseScreen = PAUSE_SCREEN_PROMPT;
                    OpenPrompt(PROMPT_GIVE_UP);
                }

                break;
            case PAUSE_ACTION_SCORECARD:
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                g_PauseScreen = PAUSE_SCREEN_LOADING_SCORECARD;
                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                g_PendingSubModuleId = MODULE_SCORECARD;
                break;
            case PAUSE_ACTION_FREE_LOOK:
                RequestPauseMenu(FALSE, CLOSING_PAUSE_MENU);
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                StartFreeLook();
                break;
            case PAUSE_ACTION_START_OVER:
                if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                    PlaySound(
                        SOUND_RING_SHOT_RETRY,
                        RING_SHOT_RETRY_SOUND_CHANNEL,
                        SOUND_FULL_SCALE
                    );
                }
            case PAUSE_ACTION_HOLE_OUT:
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                break;
        }

        return action;
    }

    if (controller->repeatedButtons & U_JPAD) {
        f_PauseSelection--;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }
    else if (controller->repeatedButtons & D_JPAD) {
        f_PauseSelection++;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }

    if (f_PauseSelection > f_PauseLayoutOptionCounts[f_PauseLayout] - 1) {
        f_PauseSelection = 0;
    }

    if (f_PauseSelection < 0) {
        f_PauseSelection = f_PauseLayoutOptionCounts[f_PauseLayout] - 1;
    }

    kind = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]].kind;
    f_PauseMenuDrawnMarker = 0;
    g_RoundHud.areLieArrowsShown = FALSE;
    g_RoundHud.areWindSpeedArrowsShown = FALSE;
    g_RoundHud.areWindDirectionArrowsShown = FALSE;
    if (kind == PAUSE_OPTION_KIND_WIND_SPEED || kind == PAUSE_OPTION_KIND_WIND_DIRECTION) {
        g_WindPanelPosition[AXIS_X] +=
            (PAUSE_PANEL_SHOWN_X - g_WindPanelPosition[AXIS_X]) * PAUSE_PANEL_EASING;
        g_WindPanelPosition[AXIS_Y] +=
            (PAUSE_PANEL_SHOWN_Y - g_WindPanelPosition[AXIS_Y]) * PAUSE_PANEL_EASING;
    }
    else {
        g_WindPanelPosition[AXIS_X] = PAUSE_PANEL_HIDDEN_X;
        g_WindPanelPosition[AXIS_Y] = PAUSE_PANEL_HIDDEN_Y;
    }

    if (kind == PAUSE_OPTION_KIND_LIE) {
        g_LiePanelPosition[AXIS_X] +=
            (PAUSE_PANEL_SHOWN_X - g_LiePanelPosition[AXIS_X]) * PAUSE_PANEL_EASING;
        g_LiePanelPosition[AXIS_Y] +=
            (PAUSE_PANEL_SHOWN_Y - g_LiePanelPosition[AXIS_Y]) * PAUSE_PANEL_EASING;
    }
    else {
        g_LiePanelPosition[AXIS_X] = PAUSE_PANEL_HIDDEN_X;
        g_LiePanelPosition[AXIS_Y] = -PAUSE_PANEL_HIDDEN_Y;
    }

    if (kind == PAUSE_OPTION_KIND_LIE) {
        hasLieChanged = FALSE;
        switch (GetSurfaceTraits(g_CurrentShot.surface)->kind) {
            case SURFACE_KIND_TEE_UP:
                lie = PAUSE_LIE_TEE_UP;
                break;
            case SURFACE_KIND_FAIRWAY:
                lie = PAUSE_LIE_FAIRWAY;
                break;
            case SURFACE_KIND_FAIRWAY_FAIR_LIE:
                lie = PAUSE_LIE_FAIRWAY_FAIR_LIE;
                break;
            case SURFACE_KIND_FAIRWAY_POOR_LIE:
                lie = PAUSE_LIE_FAIRWAY_POOR_LIE;
                break;
            case SURFACE_KIND_SEMI_ROUGH:
                lie = PAUSE_LIE_SEMI_ROUGH;
                break;
            case SURFACE_KIND_ROUGH:
                lie = PAUSE_LIE_ROUGH;
                break;
            case SURFACE_KIND_ROUGH_POOR_LIE:
                lie = PAUSE_LIE_ROUGH_POOR_LIE;
                break;
            case SURFACE_KIND_DEEP_ROUGH:
                lie = PAUSE_LIE_DEEP_ROUGH;
                break;
            case SURFACE_KIND_BUNKER:
                lie = PAUSE_LIE_BUNKER;
                break;
            case SURFACE_KIND_BUNKER_FAIR_LIE:
                lie = PAUSE_LIE_BUNKER_FAIR_LIE;
                break;
            default:
                lie = PAUSE_LIE_BUNKER_POOR_LIE;
                break;
        }

        g_RoundHud.areLieArrowsShown = TRUE;
        if (controller->repeatedButtons & L_JPAD) {
            hasLieChanged = TRUE;
            lie = (lie + PAUSE_LIE_COUNT - 1) % PAUSE_LIE_COUNT;
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }
        else if (controller->repeatedButtons & R_JPAD) {
            hasLieChanged = TRUE;
            lie = (lie + 1) % PAUSE_LIE_COUNT;
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }

        if (hasLieChanged) {
            g_Players[0].ballPosition[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
            g_CurrentShot.ballPosition[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
            if (lie < PAUSE_LIE_SEMI_ROUGH) {
                g_Players[0].ballPosition[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
                g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
            }
            else if (lie < PAUSE_LIE_DEEP_ROUGH) {
                g_Players[0].ballPosition[AXIS_X] =
                    g_HoleSetup.teePosition[AXIS_X] + PAUSE_ROUGH_LIES_OFFSET_X;
                g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
            }
            else if (lie == PAUSE_LIE_DEEP_ROUGH) {
                g_Players[0].ballPosition[AXIS_X] =
                    g_HoleSetup.teePosition[AXIS_X] + PAUSE_DEEP_ROUGH_OFFSET_X;
                g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
            }
            else {
                g_Players[0].ballPosition[AXIS_X] =
                    g_HoleSetup.teePosition[AXIS_X] + PAUSE_BUNKER_LIES_OFFSET_X;
                g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
            }

            g_CurrentShot.ballPosition[AXIS_Y] = GetGroundHeight(
                g_CurrentShot.ballPosition[AXIS_X],
                g_CurrentShot.ballPosition[AXIS_Z]
            );
            switch (lie) {
                case PAUSE_LIE_TEE_UP:
                    g_CurrentShot.ballPosition[AXIS_Y] -= PAUSE_TEE_UP_HEIGHT;
                    surface = SURFACE_TEE_UP;
                    break;
                case PAUSE_LIE_FAIRWAY:
                    surface = SURFACE_FAIRWAY;
                    break;
                case PAUSE_LIE_FAIRWAY_FAIR_LIE:
                    surface = SURFACE_FAIRWAY_FAIR_LIE;
                    break;
                case PAUSE_LIE_FAIRWAY_POOR_LIE:
                    surface = SURFACE_FAIRWAY_POOR_LIE;
                    break;
                case PAUSE_LIE_SEMI_ROUGH:
                    surface = SURFACE_SEMI_ROUGH;
                    break;
                case PAUSE_LIE_ROUGH:
                    surface = SURFACE_ROUGH;
                    break;
                case PAUSE_LIE_ROUGH_POOR_LIE:
                    surface = SURFACE_ROUGH_POOR_LIE;
                    break;
                case PAUSE_LIE_DEEP_ROUGH:
                    surface = SURFACE_DEEP_ROUGH;
                    break;
                case PAUSE_LIE_BUNKER:
                    surface = SURFACE_BUNKER;
                    break;
                case PAUSE_LIE_BUNKER_FAIR_LIE:
                    surface = SURFACE_BUNKER_FAIR_LIE;
                    break;
                case PAUSE_LIE_BUNKER_POOR_LIE:
                default:
                    surface = SURFACE_BUNKER_POOR_LIE;
                    break;
            }

            g_Players[0].ballSurface = surface;
            g_CurrentShot.surface = surface;
            g_Players[0].ballPosition[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
            PlaceBallAtRest(0, &g_TurnBallSimulation);
            StartShotPanel();
        }
    }
    else if (kind == PAUSE_OPTION_KIND_WIND_DIRECTION) {
        g_RoundHud.areWindDirectionArrowsShown = TRUE;
        g_HoleSetup.conditions.windDirection += controller->stickX * PAUSE_WIND_TURN_PER_STICK;
        if (controller->stickX == 0) {
            if (controller->buttons & L_JPAD) {
                g_HoleSetup.conditions.windDirection -= PAUSE_WIND_TURN_STEP;
            }

            if (controller->buttons & R_JPAD) {
                g_HoleSetup.conditions.windDirection += PAUSE_WIND_TURN_STEP;
            }
        }
    }
    else if (kind == PAUSE_OPTION_KIND_WIND_SPEED) {
        g_RoundHud.areWindSpeedArrowsShown = TRUE;
        if ((controller->repeatedButtons & L_JPAD) && g_HoleSetup.conditions.windSpeed > 0) {
            g_HoleSetup.conditions.windSpeed -= PAUSE_WIND_SPEED_STEP;
            PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }

        if ((controller->repeatedButtons & R_JPAD) &&
            g_HoleSetup.conditions.windSpeed <= PAUSE_WIND_SPEED_RAISE_LIMIT) {
            if (g_HoleSetup.conditions.windSpeed < PAUSE_WIND_SPEED_FIRST_STEP) {
                g_HoleSetup.conditions.windSpeed += PAUSE_WIND_SPEED_FIRST_STEP;
            }
            else {
                g_HoleSetup.conditions.windSpeed += PAUSE_WIND_SPEED_STEP;
            }

            PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }

        if (g_HoleSetup.conditions.windSpeed <= PAUSE_WIND_SPEED_CALM) {
            g_HoleSetup.conditions.windSpeed = 0;
        }

        if (g_HoleSetup.conditions.windSpeed > PAUSE_WIND_SPEED_MAX) {
            g_HoleSetup.conditions.windSpeed = PAUSE_WIND_SPEED_MAX;
        }
    }
    else if (kind == PAUSE_OPTION_KIND_GRID_COLOR) {
        if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
            g_Players[g_CurrentShot.playerIndex].gridColor ^= GRID_COLOR_FLIP;
            if (g_CurrentShot.playerIndex == 0) {
                GetSaveData()->options[SAVE_OPTION_GRID_COLOR] =
                    g_Players[g_CurrentShot.playerIndex].gridColor;
                WriteSaveData();
            }

            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }
    }
    else if (kind == PAUSE_OPTION_KIND_WEATHER) {
        if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
            if (g_HoleSetup.conditions.isRaining == TRUE) {
                StopSound(RAIN_SOUND_CHANNEL);
                g_HoleSetup.conditions.isRaining = FALSE;
            }
            else {
                PlaySound(RAIN_SOUND, RAIN_SOUND_CHANNEL, RAIN_SOUND_START_VOLUME);
                g_HoleSetup.conditions.isRaining = TRUE;
                g_RainSoundVolume = RAIN_SOUND_START_VOLUME;
            }
        }
    }

    return PAUSE_ACTION_NONE;
}

#define PAUSE_ARROWS_TICKS_PER_FRAME 4
#define PAUSE_ARROWS_TICK_COUNT (PAUSE_ARROWS_TICKS_PER_FRAME * 2)
#define PAUSE_LEFT_ARROW_T TexelCoordinate(GREEN_ARROW_SIZE * 3)
#define PAUSE_RIGHT_ARROW_T TexelCoordinate(GREEN_ARROW_SIZE * 2)

void DrawPauseArrows(Gfx **displayListHead, s32 x, s32 y, s32 gap) {
    Gfx *gfx = *displayListHead;
    s32 top;
    s32 bottom;

    f_PauseArrowsTick = (f_PauseArrowsTick + 1) % PAUSE_ARROWS_TICK_COUNT;
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    LoadSmallPalette(&gfx, 0, ImagePalette(f_PauseArrowsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_PauseArrowsImage) +
            (f_PauseArrowsTick / PAUSE_ARROWS_TICKS_PER_FRAME * GREEN_ARROW_FRAME_SIZE),
        G_IM_FMT_CI,
        GREEN_ARROW_SIZE,
        GREEN_ARROW_FRAME_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    top = QuarterPixels(y - (GREEN_ARROW_SIZE / 2));
    bottom = QuarterPixels(y + (GREEN_ARROW_SIZE / 2));
    gDPPipeSync(gfx++);
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(x - (gap / 2) - (GREEN_ARROW_SIZE / 2)),
        top,
        QuarterPixels(x - (gap / 2) + (GREEN_ARROW_SIZE / 2)),
        bottom,
        G_TX_RENDERTILE,
        0,
        PAUSE_LEFT_ARROW_T,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(x + (gap / 2) - (GREEN_ARROW_SIZE / 2)),
        top,
        QuarterPixels(x + (gap / 2) + (GREEN_ARROW_SIZE / 2)),
        bottom,
        G_TX_RENDERTILE,
        0,
        PAUSE_RIGHT_ARROW_T,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    *displayListHead = gfx;
}

#define PAUSE_CLOUD_LEFT 32
#define PAUSE_CLOUD_RIGHT 288
#define PAUSE_CLOUD_TOP 32
#define PAUSE_CLOUD_PART_WIDTH 256
#define PAUSE_CLOUD_PART_HEIGHT 32
#define PAUSE_CLOUD_PART_SIZE 0x1000
#define PAUSE_LABEL_X 80
#define PAUSE_LABELS_TOP 48
#define PAUSE_LABEL_SPACING 18
#define PauseLabelY(row) (PAUSE_LABELS_TOP + ((row) * PAUSE_LABEL_SPACING))
#define PAUSE_LABEL_FLAGS TEXT_FLAG_DRAWN_BY_OWNER
#define PAUSE_VALUE_X 212
#define PAUSE_RED_WAVE_MIDDLE 95
#define PAUSE_RED_WAVE_AMPLITUDE 95.0f
#define PAUSE_LABEL_RED 0x5F
#define PAUSE_LABEL_GREEN 0x3F
#define PAUSE_LABEL_BLUE 0xFF
#define PAUSE_CURSOR_RIGHT 80
#define PAUSE_CURSOR_SWAY 8.0f
#define PAUSE_TEE_X 56
#define PAUSE_TEE_LEAN_SWAY 5
#define PAUSE_TEES_T TexelCoordinate(POINTING_HAND_SIZE)

void DrawPauseMenu(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 cloudMiddle;
    s32 labelsHeight;
    s32 glowRed;
    s32 glowGreen;
    s32 glowBlue;
    s32 red;
    s32 green;
    s32 blue;
    s32 optionIndex;
    PauseMessage message;
    PauseOptionKind kind;
    PauseOptionKind selectedKind;
    s32 sway;
    s32 teeFrame;

    if (f_PauseMenuRequest == PAUSE_MENU_REQUEST_LOAD) {
        LoadPauseMenu();
        f_PauseMenuRequest = PAUSE_MENU_REQUEST_NONE;
    }
    else if (f_PauseMenuRequest == PAUSE_MENU_REQUEST_FREE) {
        FreePauseMenu();
        f_PauseMenuRequest = PAUSE_MENU_REQUEST_NONE;
    }

    if (!f_IsPauseMenuLoaded) {
        return;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 220, 220, 230, 210);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    labelsHeight = f_PauseLayoutOptionCounts[f_PauseLayout] * PAUSE_LABEL_SPACING;
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(f_MenuCloudImage),
        G_IM_FMT_I,
        PAUSE_CLOUD_PART_WIDTH,
        PAUSE_CLOUD_PART_HEIGHT,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    cloudMiddle = (labelsHeight + PAUSE_CLOUD_TOP) * 4;
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(PAUSE_CLOUD_LEFT),
        QuarterPixels(PAUSE_CLOUD_TOP),
        QuarterPixels(PAUSE_CLOUD_RIGHT),
        cloudMiddle,
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(f_MenuCloudImage) + PAUSE_CLOUD_PART_SIZE,
        G_IM_FMT_I,
        PAUSE_CLOUD_PART_WIDTH,
        PAUSE_CLOUD_PART_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(PAUSE_CLOUD_LEFT),
        cloudMiddle,
        QuarterPixels(PAUSE_CLOUD_RIGHT),
        QuarterPixels(labelsHeight + PAUSE_CLOUD_TOP + PAUSE_CLOUD_PART_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    f_PauseGlowAngle += M_PI / 18;
    glowRed = (s32)(sinf(f_PauseGlowAngle) * PAUSE_RED_WAVE_AMPLITUDE) + PAUSE_RED_WAVE_MIDDLE;
    glowGreen = (s32)(sinf(f_PauseGlowAngle + M_PI) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    glowBlue =
        (s32)(sinf(f_PauseGlowAngle + (M_PI / 2)) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    SetUpTextDrawing(&gfx);
    for (optionIndex = 0; optionIndex != f_PauseLayoutOptionCounts[f_PauseLayout]; optionIndex++) {
        message = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][optionIndex]].message;
        kind = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][optionIndex]].kind;
        if (f_PauseSelection == optionIndex) {
            red = glowRed;
            green = glowGreen;
            blue = glowBlue;
        }
        else {
            red = PAUSE_LABEL_RED;
            green = PAUSE_LABEL_GREEN;
            blue = PAUSE_LABEL_BLUE;
        }

        if (f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][optionIndex]].kind ==
            PAUSE_OPTION_KIND_GIVE_UP) {
            if (!f_IsGiveUpAllowed) {
                message = PAUSE_MESSAGE_FIRST_STROKE;
            }
        }

        SetTextSlot(
            PAUSE_LABEL_X,
            PauseLabelY(optionIndex),
            message + MESSAGE_FIRST_PAUSE_OPTION,
            TEXT_SLOT_LARGE_FONT,
            optionIndex,
            PAUSE_LABEL_FLAGS,
            red,
            green,
            blue,
            OPAQUE_ALPHA
        );
        gDPSetPrimColor(gfx++, 0, 0, red, green, blue, OPAQUE_ALPHA);
        DrawLargeTextSlot(&gfx, optionIndex);
        if (kind == PAUSE_OPTION_KIND_GRID_COLOR) {
            if (g_Players[g_CurrentShot.playerIndex].gridColor == GRID_COLOR_BLACK) {
                DrawLargeText(
                    &gfx,
                    PAUSE_VALUE_X,
                    PauseLabelY(optionIndex),
                    f_PauseValueTexts[PAUSE_VALUE_TEXT_BLACK]
                );
            }
            else {
                DrawLargeText(
                    &gfx,
                    PAUSE_VALUE_X,
                    PauseLabelY(optionIndex),
                    f_PauseValueTexts[PAUSE_VALUE_TEXT_WHITE]
                );
            }
        }

        if (kind == PAUSE_OPTION_KIND_WEATHER) {
            if (g_HoleSetup.conditions.isRaining == TRUE) {
                DrawLargeText(
                    &gfx,
                    PAUSE_VALUE_X,
                    PauseLabelY(optionIndex),
                    f_PauseValueTexts[PAUSE_VALUE_TEXT_RAIN]
                );
            }
            else {
                DrawLargeText(
                    &gfx,
                    PAUSE_VALUE_X,
                    PauseLabelY(optionIndex),
                    f_PauseValueTexts[PAUSE_VALUE_TEXT_CLEAR]
                );
            }
        }
    }

    f_PauseCursorAngle += M_PI / 12;
    if (f_PauseCursorAngle > M_PI) {
        f_PauseCursorAngle -= M_PI;
    }

    sway = sinf(f_PauseCursorAngle) * PAUSE_CURSOR_SWAY;
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    teeFrame = sway >= PAUSE_TEE_LEAN_SWAY;
    LoadSmallPalette(&gfx, 0, ImagePalette(f_PauseCursorImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(f_PauseCursorImage)),
        G_IM_FMT_CI,
        POINTING_HAND_SIZE,
        0,
        0,
        0,
        POINTING_HAND_SIZE - 1,
        POINTING_HAND_LOAD_HEIGHT - 1,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    if (f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection] == PAUSE_OPTION_GIVE_UP &&
        !f_IsGiveUpAllowed) {
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0
        );
        gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, OPAQUE_ALPHA);
    }

    selectedKind = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]].kind;
    if (IsValueOptionKind(selectedKind)) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(PAUSE_TEE_X),
            QuarterPixels((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 8),
            QuarterPixels(PAUSE_CURSOR_RIGHT),
            QuarterPixels(
                (f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 8 + POINTING_HAND_SIZE
            ),
            G_TX_RENDERTILE,
            0,
            PAUSE_TEES_T + (teeFrame * TexelCoordinate(POINTING_HAND_SIZE)),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(PAUSE_TEE_X - sway),
            QuarterPixels((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 4),
            QuarterPixels(PAUSE_CURSOR_RIGHT - sway),
            QuarterPixels(
                (f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 4 + POINTING_HAND_SIZE
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    f_PauseMenuDrawnMarker = 0xFFFFFF;
    *displayListHead = gfx;
}

void LoadChoiceBoxImages(void) {
    EncodedAsset asset;

    f_ChoiceBoxCursorImage = AllocMemory(OpenEncodedAsset(ASSET_POINTING_HAND, &asset));
    ReadEncodedAsset(f_ChoiceBoxCursorImage, &asset);
    f_MenuCloudImage = AllocMemory(OpenEncodedAsset(ASSET_CHOICE_BOX_CLOUD_IMAGE, &asset));
    ReadEncodedAsset(f_MenuCloudImage, &asset);
}

void FreeChoiceBoxImages(void) {
    FreeMemory(&f_ChoiceBoxCursorImage);
    FreeMemory(&f_MenuCloudImage);
}

void OpenChoiceBox(ChoiceBoxId choiceBoxId) {
    f_ChoiceBoxId = choiceBoxId;
    if (choiceBoxId == CHOICE_BOX_ID_NONE) {
        f_ChoiceBoxState = CHOICE_BOX_CLOSED;
        FreeChoiceBoxImages();
        PlaySound(SOUND_PROMPT_CLOSE, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }
    else {
        f_ChoiceBoxSelection = 0;
        f_ChoiceBoxState = CHOICE_BOX_CHOOSING;
        LoadChoiceBoxImages();
        PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }
}

ChoiceResult UpdateChoiceBox(ControllerState *controller) {
    ChoiceBox *box = &f_ChoiceBoxes[f_ChoiceBoxId];

    if (f_ChoiceBoxId == CHOICE_BOX_ID_NONE) {
        return CHOICE_RESULT_NONE;
    }

    switch (f_ChoiceBoxState) {
        case CHOICE_BOX_CHOSEN:
            f_ChoiceBoxState = CHOICE_BOX_CLOSING;
            return CHOICE_RESULT_NONE;
        case CHOICE_BOX_CLOSING:
            OpenChoiceBox(CHOICE_BOX_ID_NONE);
            return box->results[f_ChoiceBoxSelection];
        case CHOICE_BOX_CHOOSING:
            if (controller->pressedButtons == A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                f_ChoiceBoxState = CHOICE_BOX_CHOSEN;
                return CHOICE_RESULT_NONE;
            }

            if (!box->isVertical) {
                if (controller->repeatedButtons & L_JPAD) {
                    f_ChoiceBoxSelection--;
                    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
                }

                if (controller->repeatedButtons & R_JPAD) {
                    f_ChoiceBoxSelection++;
                    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
                }
            }
            else {
                if (controller->repeatedButtons & U_JPAD) {
                    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
                    f_ChoiceBoxSelection--;
                }

                if (controller->repeatedButtons & D_JPAD) {
                    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
                    f_ChoiceBoxSelection++;
                }
            }

            if (f_ChoiceBoxSelection > box->optionCount - 1) {
                f_ChoiceBoxSelection = 0;
            }

            if (f_ChoiceBoxSelection < 0) {
                f_ChoiceBoxSelection = box->optionCount - 1;
            }

            return CHOICE_RESULT_NONE;
    }

    return CHOICE_RESULT_NONE;
}

void DrawChoiceBox(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    ChoiceBox *box = &f_ChoiceBoxes[f_ChoiceBoxId];
    s32 halfWidth;
    s32 red;
    s32 green;
    s32 blue;
    s32 textWidth;
    s32 labelWidth;
    s32 chosenWidth;
    s32 i;
    s32 cursorX;
    s32 cursorY;
    s32 labelsTop;

    halfWidth = box->halfWidth + CHOICE_BOX_MARGIN;
    f_ChoiceGlowAngle += M_PI / 18;
    red = (s32)(sinf(f_ChoiceGlowAngle) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    blue = (s32)(sinf(f_ChoiceGlowAngle + (M_PI / 2)) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    green = (s32)(sinf(f_ChoiceGlowAngle + M_PI) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    textWidth = 0;
    if (f_ChoiceBoxId != CHOICE_BOX_ID_NONE) {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0x1F, 0x1F, 0x1F, 0xBF);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xBF);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            PRIMITIVE,
            0,
            0,
            0,
            TEXEL0,
            0,
            0,
            0,
            PRIMITIVE,
            0,
            0,
            0,
            TEXEL0
        );
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        for (i = 0; i != box->optionCount; i++) {
            labelWidth = MeasureText(
                f_ChoiceBoxLabels[box->labels[i]],
                FONT_SIZE_LARGE,
                MEASURE_TEXT_PROPORTIONAL
            );
            if (textWidth < labelWidth) {
                textWidth = labelWidth;
            }

            halfWidth = (textWidth / 2) + CHOICE_BOX_MARGIN;
        }

        for (i = 0; i != CHOICE_BOX_CLOUD_PART_COUNT; i++) {
            LoadNibbleTextureBlock(
                &gfx,
                ImageBody(f_MenuCloudImage) + (i * CHOICE_BOX_CLOUD_PART_SIZE),
                G_IM_FMT_I,
                CHOICE_BOX_CLOUD_WIDTH,
                CHOICE_BOX_CLOUD_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(CHOICE_BOX_CENTER_X - halfWidth),
                QuarterPixels(CHOICE_BOX_TOP),
                QuarterPixels(CHOICE_BOX_CENTER_X + halfWidth),
                QuarterPixels(CHOICE_BOX_BOTTOM),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(CHOICE_BOX_CLOUD_WIDTH / 2, halfWidth),
                CHOICE_BOX_CLOUD_DTDY
            );
        }

        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        if (box->isVertical) {
            labelsTop =
                CHOICE_LABELS_MIDDLE_Y - ((box->optionCount - 1) * (CHOICE_LABEL_SPACING / 2));
            for (i = 0; i != box->optionCount; i++) {
                if (i == f_ChoiceBoxSelection) {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, OPAQUE_ALPHA);
                    gDPPipeSync(gfx++);
                }
                else {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, 0x5F, 0x3F, 0xFF, OPAQUE_ALPHA);
                    gDPPipeSync(gfx++);
                }

                DrawCenteredLargeText(
                    &gfx,
                    CHOICE_BOX_CENTER_X,
                    labelsTop + (i * CHOICE_LABEL_SPACING),
                    f_ChoiceBoxLabels[box->labels[i]]
                );
            }

            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            f_ChoiceCursorAngle += M_PI / 12;
            if (f_ChoiceCursorAngle > M_PI) {
                f_ChoiceCursorAngle -= M_PI;
            }

            chosenWidth = MeasureText(
                f_ChoiceBoxLabels[box->labels[f_ChoiceBoxSelection]],
                FONT_SIZE_LARGE,
                MEASURE_TEXT_PROPORTIONAL
            );
            cursorX = (s32)(sinf(f_ChoiceCursorAngle) * CHOICE_CURSOR_SWAY) + CHOICE_CURSOR_X;
            cursorX -= chosenWidth / 2;
            LoadSmallPalette(&gfx, 0, ImagePalette(f_ChoiceBoxCursorImage));
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageTexels(f_ChoiceBoxCursorImage)),
                G_IM_FMT_CI,
                POINTING_HAND_SIZE,
                0,
                0,
                0,
                POINTING_HAND_SIZE - 1,
                POINTING_HAND_LOAD_HEIGHT - 1,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            cursorY = labelsTop + (f_ChoiceBoxSelection * CHOICE_LABEL_SPACING);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(cursorX),
                QuarterPixels(cursorY - 2),
                QuarterPixels(cursorX + POINTING_HAND_SIZE),
                QuarterPixels(cursorY + POINTING_HAND_SIZE - 2),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    *displayListHead = gfx;
}
