/*
 * The shot being set up, defined volatile under KMC; see
 * current_shot.h.
 */

#define READS_CURRENT_SHOT_VOLATILE
#include "shot/current_shot.h"

CURRENT_SHOT_VOLATILE CurrentShot g_CurrentShot;
