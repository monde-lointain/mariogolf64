/*
 * The player's controllers, standing in for the N64's: SDL gamepads fill
 * the ports in the order they connect, and the keyboard plays the first
 * port too.
 *
 * The platform's thread reads the devices once a retrace with
 * UpdateControllers, and the game's threads read what it found through
 * nusys_host.h's PcReadControllerPad and its siblings, which may be called
 * from any thread.
 */

#ifndef PORT_PLATFORM_INPUT_H
#define PORT_PLATFORM_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <ultra64.h>

#include "input_recorder.h"
#include "input_script.h"
#include "pad_mapping.h"

// Starts and ends the platform's use of the gamepads; SDL's gamepad
// subsystem must be up
void InitControllers(void);
void ShutdownControllers(void);

// Maps the controllers with these bindings from the next update on, in
// place of the defaults; from the platform's thread
void SetPadBindings(const PadBindings *bindings);

// Takes a gamepad that SDL found into the first free port, or lets one go
void ConnectGamepad(uint32_t instanceId);
void DisconnectGamepad(uint32_t instanceId);

// Reads every port's controller, and starts or stops the rumble that the
// game asked for since the last time
void UpdateControllers(void);

// Plays a script into the first port from now on, or stops with NULL. The
// game's reads of the port count its frames from 0, and each read gets
// what the script holds at that frame, or the devices' state before the
// script's first line. The script must outlive its use.
void PlayInputScript(const InputScript *script);

// The game's frames counted since the script was last set: its reads of the
// first port
long CountGameFrames(void);

// Records what the game reads from the first port from now on, by the same
// frames, after any script has played; NULL stops. The recorder must
// outlive its use, and a failed write is logged once.
void RecordFirstPort(InputRecorder *recorder);

#endif
