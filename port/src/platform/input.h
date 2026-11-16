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

// What a host controller holds at one moment, in SDL's terms: a bit for
// each SDL_GamepadButton, and each SDL_GamepadAxis's value
typedef struct HostPadState {
    uint32_t buttons;
    int16_t axes[6];
} HostPadState;

// Makes an N64 controller's state from a host controller's
void MapHostPad(const HostPadState *host, OSContPad *pad);

// Scales a stick's position, each axis from -32768 to 32767, to the N64
// stick's range, from -80 to 80, with up positive and a dead zone around
// the center
void MapHostStick(int16_t x, int16_t y, s8 *stickX, s8 *stickY);

// Adds the keyboard's keys to a host controller's state; keys is
// SDL_GetKeyboardState's array
void AddKeyboardToHostPad(const bool *keys, HostPadState *host);

// Starts and ends the platform's use of the gamepads; SDL's gamepad
// subsystem must be up
void InitControllers(void);
void ShutdownControllers(void);

// Takes a gamepad that SDL found into the first free port, or lets one go
void ConnectGamepad(uint32_t instanceId);
void DisconnectGamepad(uint32_t instanceId);

// Reads every port's controller, and starts or stops the rumble that the
// game asked for since the last time
void UpdateControllers(void);

#endif
