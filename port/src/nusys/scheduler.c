/*
 * NuSystem's scheduler for the port. It passes each retrace and the reset
 * button to its clients, and finishes the audio tasks that the music player
 * hands it: the port's libmus mixes on the CPU, so the tasks hold nothing
 * to run. The graphics tasks never come here; nuGfxTaskStart runs them.
 */

#include "nusys_internal.h"

#define EVENT_STACK_SIZE 0x2000

NUSched nusched;
volatile u32 nuScRetraceCounter;
u8 nuScPreNMIFlag;

static NUScPreNMIFunc f_PreNmiFunction;

// Sends the message to every client that asked for its type
static void NotifyClients(NUScMsg type, NUScMsg *message) {
    NUScClient *client;

    for (client = nusched.clientList; client != NULL; client = client->next) {
        if (client->msgType & type) {
            osSendMesg(client->msgQ, message, OS_MESG_NOBLOCK);
        }
    }
}

static void HandleReset(void) {
    nuScPreNMIFlag = NU_SC_PRENMI_GET;
    if (f_PreNmiFunction != NULL) {
        f_PreNmiFunction();
    }

    NotifyClients(NU_SC_PRENMI_MSG, &nusched.prenmiMsg);
}

static void RunEventThread(void *argument) {
    OSMesg event;

    (void)argument;
    while (TRUE) {
        osRecvMesg(&nusched.retraceMQ, &event, OS_MESG_BLOCK);
        if (event == SCHEDULER_EVENT_RETRACE) {
            nuScRetraceCounter++;
            nusched.retraceCount++;
            NotifyClients(NU_SC_RETRACE_MSG, &nusched.retraceMsg);
        }
        else if (event == SCHEDULER_EVENT_PRENMI) {
            HandleReset();
        }
    }
}

// An audio task is done as soon as it arrives
static void RunAudioThread(void *argument) {
    NUScTask *task;

    (void)argument;
    while (TRUE) {
        osRecvMesg(&nusched.audioRequestMQ, (OSMesg *)&task, OS_MESG_BLOCK);
        if (task->msgQ != NULL) {
            osSendMesg(task->msgQ, task->msg, OS_MESG_BLOCK);
        }
    }
}

void NuStartScheduler(void) {
    nusched.retraceMsg = NU_SC_RETRACE_MSG;
    nusched.prenmiMsg = NU_SC_PRENMI_MSG;
    nusched.clientList = NULL;
    nusched.frameRate = 1;
    nusched.frameBufferNum = NU_GFX_FRAMEBUFFER_NUM;
    osCreateMesgQueue(&nusched.retraceMQ, nusched.retraceMsgBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.audioRequestMQ, nusched.audioRequestBuf, NU_SC_MAX_MESGS);
    osCreateMesgQueue(&nusched.graphicsRequestMQ, nusched.graphicsRequestBuf, NU_SC_MAX_MESGS);
    osViSetEvent(&nusched.retraceMQ, SCHEDULER_EVENT_RETRACE, 1);
    osSetEventMesg(OS_EVENT_PRENMI, &nusched.retraceMQ, SCHEDULER_EVENT_PRENMI);

    osCreateThread(
        &nusched.schedulerThread,
        NU_SC_HANDLER_THREAD_ID,
        RunEventThread,
        NULL,
        NULL,
        NU_SC_HANDLER_PRI
    );
    osStartThread(&nusched.schedulerThread);
    osCreateThread(
        &nusched.audioThread,
        NU_SC_AUDIO_THREAD_ID,
        RunAudioThread,
        NULL,
        NULL,
        NU_SC_AUDIO_PRI
    );
    osStartThread(&nusched.audioThread);
}

void nuScAddClient(NUScClient *client, OSMesgQueue *queue, NUScMsg type) {
    client->msgQ = queue;
    client->msgType = type;
    client->next = nusched.clientList;
    nusched.clientList = client;
    // A client added after the reset button hears of it at once
    if ((type & NU_SC_PRENMI_MSG) && nuScPreNMIFlag != NU_SC_PRENMI_YET) {
        osSendMesg(queue, &nusched.prenmiMsg, OS_MESG_NOBLOCK);
    }
}

void nuScRemoveClient(NUScClient *client) {
    NUScClient **link;

    for (link = &nusched.clientList; *link != NULL; link = &(*link)->next) {
        if (*link == client) {
            *link = client->next;
            return;
        }
    }
}

void nuPreNMIFuncSet(NUScPreNMIFunc function) {
    f_PreNmiFunction = function;
}
