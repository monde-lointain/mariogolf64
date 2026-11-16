/*
 * NuSystem's controllers for the port: the host's controllers, read when
 * the game asks, and their rumble, which runs for a number of retraces.
 * No Transfer Pak is ever found, so the game's Game Boy features stay off.
 */

#include "nusys_internal.h"
#include "port/nusys_host.h"

#define RUMBLE_QUEUE_LENGTH 4
// Rumble's frequency at which the motor runs all the time
#define RUMBLE_FREQUENCY_FULL 256

typedef struct Rumble {
    u16 frequency;
    u16 framesLeft;
    u16 phase; // Counts up by frequency; the motor runs on each overflow
    u8 isMotorOn;
    u8 mode;
} Rumble;

static u16 f_PreviousButtons[NU_CONT_MAXCONTROLLERS];
static Rumble f_Rumbles[NU_CONT_MAXCONTROLLERS];
static int f_IsRumbleStopped; // Held off by nuContRmbForceStop
static int f_IsRumbleManaged;

static OSThread f_RumbleThread;
static OSMesgQueue f_RumbleQueue;
static OSMesg f_RumbleMessages[RUMBLE_QUEUE_LENGTH];
static NUScClient f_RumbleClient;

static int IsPortValid(u32 port) {
    return port < NU_CONT_MAXCONTROLLERS;
}

static void SetMotor(int port, Rumble *rumble, int isOn) {
    if (rumble->isMotorOn != isOn) {
        rumble->isMotorOn = (u8)isOn;
        PcSetRumble(port, isOn);
    }
}

// Runs each rumble for a retrace. A frequency below full turns the motor
// on for that share of the retraces.
static void StepRumbles(void) {
    int port;

    for (port = 0; port < NU_CONT_MAXCONTROLLERS; port++) {
        Rumble *rumble = &f_Rumbles[port];
        int isOn = FALSE;

        if (rumble->framesLeft != 0 && !f_IsRumbleStopped) {
            u32 phase = rumble->phase + rumble->frequency;

            isOn = phase >= RUMBLE_FREQUENCY_FULL;
            rumble->phase = (u16)(phase % RUMBLE_FREQUENCY_FULL);
            rumble->framesLeft--;
        }

        SetMotor(port, rumble, isOn);
    }
}

static void RunRumbleThread(void *argument) {
    NUScMsg *message;

    (void)argument;
    while (TRUE) {
        osRecvMesg(&f_RumbleQueue, (OSMesg *)&message, OS_MESG_BLOCK);
        if (*message == NU_SC_RETRACE_MSG && f_IsRumbleManaged) {
            StepRumbles();
        }
    }
}

u8 nuContInit(void) {
    int port;

    for (port = 0; port < NU_CONT_MAXCONTROLLERS; port++) {
        f_PreviousButtons[port] = 0;
        f_Rumbles[port] = (Rumble) {0};
    }

    f_IsRumbleManaged = TRUE;
    f_IsRumbleStopped = FALSE;
    osCreateMesgQueue(&f_RumbleQueue, f_RumbleMessages, RUMBLE_QUEUE_LENGTH);
    nuScAddClient(&f_RumbleClient, &f_RumbleQueue, NU_SC_RETRACE_MSG);
    osCreateThread(
        &f_RumbleThread,
        NU_CONT_THREAD_ID,
        RunRumbleThread,
        NULL,
        NULL,
        NU_CONT_THREAD_PRI
    );
    osStartThread(&f_RumbleThread);
    return PcGetConnectedControllers();
}

// A port's controller, with the buttons pressed since the last read of the
// port as its trigger
void nuContDataGetEx(NUContData *data, u32 port) {
    OSContPad pad = {0};

    if (!IsPortValid(port)) {
        *data = (NUContData) {0};
        return;
    }

    PcReadControllerPad((int)port, &pad);
    data->button = pad.button;
    data->stick_x = pad.stick_x;
    data->stick_y = pad.stick_y;
    data->errno = pad.errno;
    data->trigger = pad.button & ~f_PreviousButtons[port];
    f_PreviousButtons[port] = pad.button;
}

s32 nuContRmbCheck(u32 port) {
    if (!IsPortValid(port) || !(PcGetConnectedControllers() & (1 << port))) {
        return PFS_ERR_NOPACK;
    }

    return PcHasRumble((int)port) ? 0 : PFS_ERR_NOPACK;
}

void nuContRmbModeSet(u32 port, u8 mode) {
    if (IsPortValid(port)) {
        f_Rumbles[port].mode = mode;
    }
}

void nuContRmbStart(u32 port, u16 frequency, u16 frameCount) {
    if (!IsPortValid(port) ||
        f_IsRumbleStopped ||
        (f_Rumbles[port].mode & ~NU_CONT_RMB_MODE_PAUSE) == NU_CONT_RMB_MODE_DISABLE) {
        return;
    }

    f_Rumbles[port].frequency = frequency;
    f_Rumbles[port].framesLeft = frameCount;
    f_Rumbles[port].phase = 0;
}

void nuContRmbStop(u32 port) {
    if (IsPortValid(port)) {
        f_Rumbles[port].framesLeft = 0;
    }
}

// Stops every motor, and keeps them stopped until nuContRmbForceStopEnd
void nuContRmbForceStop(void) {
    int port;

    f_IsRumbleStopped = TRUE;
    for (port = 0; port < NU_CONT_MAXCONTROLLERS; port++) {
        f_Rumbles[port].framesLeft = 0;
        SetMotor(port, &f_Rumbles[port], FALSE);
    }
}

void nuContRmbForceStopEnd(void) {
    f_IsRumbleStopped = FALSE;
}

void nuContRmbMgrRemove(void) {
    nuContRmbForceStop();
    f_IsRumbleManaged = FALSE;
}

s32 nuSiSendMesg(NUScMsg message, void *data) {
    (void)data;
    if (message == NU_CONT_RMB_FORCESTOPEND_MSG) {
        nuContRmbForceStopEnd();
    }

    return 0;
}

void nuContGBPakMgrInit(void) {
}

void nuContGBPakMgrRemove(void) {
}

s32 nuContGBPakOpen(NUContPakFile *file, s32 port) {
    (void)port;
    file->error = PFS_ERR_NOPACK;
    return PFS_ERR_NOPACK;
}

s32 nuContGBPakGetStatus(NUContPakFile *file, u8 *status) {
    (void)status;
    return file->error = PFS_ERR_NOPACK;
}

s32 nuContGBPakReadID(NUContPakFile *file, OSGbpakId *id, u8 *status) {
    (void)id;
    (void)status;
    return file->error = PFS_ERR_NOPACK;
}

s32 nuContGBPakReadWrite(NUContPakFile *file, u16 direction, u16 address, u8 *buffer, u16 size) {
    (void)direction;
    (void)address;
    (void)buffer;
    (void)size;
    return file->error = PFS_ERR_NOPACK;
}

s32 nuContGBPakPower(NUContPakFile *file, s32 isOn) {
    (void)isOn;
    return file->error = PFS_ERR_NOPACK;
}

s32 nuContGBPakCheckConnector(NUContPakFile *file, u8 *status) {
    (void)status;
    return file->error = PFS_ERR_NOPACK;
}

s32 nuContGBPakFread(NUContPakFile *file, u16 address, u8 *buffer, u16 size) {
    return nuContGBPakReadWrite(file, OS_READ, address, buffer, size);
}

s32 nuContGBPakFwrite(NUContPakFile *file, u16 address, u8 *buffer, u16 size) {
    return nuContGBPakReadWrite(file, OS_WRITE, address, buffer, size);
}
