/*
 * SI (serial-interface) manager: owns the controller-port hardware and runs a
 * manager thread that, on each retrace, polls input through the registered SI
 * callbacks and routes request messages to the callback addressed by their
 * major/minor number. Application code talks to it via nuSiSendMesg.
 */

#include <nusys.h>

extern OSMesg nuSiMesgBuf[8];
extern OSThread siMgrThread;
extern u64 siMgrStack[NU_SI_STACK_SIZE / sizeof(u64)];

static void nuSiMgrThread(void* arg);

/* Expose the base of the SI manager's thread stack. */
void* func_800A2780(void) { return siMgrStack; }

/*
 * Initialize the controller subsystem and start the manager thread. Returns
 * the controller-present bit pattern reported by osContInit (bit per port).
 */
u8 nuSiMgrInit(void) {
  u8 pattern;
  OSContStatus status[NU_CONT_MAXCONTROLLERS];

  // Route SI interrupts to the manager's queue and probe attached controllers.
  osCreateMesgQueue(&nuSiMesgQ, nuSiMesgBuf, 8);
  osSetEventMesg(OS_EVENT_SI, &nuSiMesgQ, (OSMesg)0);
  osContInit(&nuSiMesgQ, &pattern, status);

  osCreateThread(&siMgrThread, NU_SI_THREAD_ID, nuSiMgrThread, (void*)NULL,
                 (siMgrStack + NU_SI_STACK_SIZE / sizeof(u64)),
                 NU_SI_THREAD_PRI);
  osStartThread(&siMgrThread);
  return pattern;
}

/*
 * Send a request to the manager thread and block until it has been handled.
 * A throwaway return queue makes the call synchronous; the handler's status is
 * read back out of the (caller-stack) message and returned.
 */
s32 nuSiSendMesg(NUScMsg mesg, void* dataPtr) {
  OSMesg rtnMesgBuf;
  OSMesgQueue rtnMesgQ;
  NUSiCommonMesg siCommonMesg;

  siCommonMesg.mesg = mesg;
  siCommonMesg.dataPtr = dataPtr;
  siCommonMesg.rtnMesgQ = &rtnMesgQ;
  osCreateMesgQueue(&rtnMesgQ, &rtnMesgBuf, 1);
  osSendMesg(&nuSiMgrMesgQ, (OSMesg*)&siCommonMesg, OS_MESG_BLOCK);
  osRecvMesg(&rtnMesgQ, NULL, OS_MESG_BLOCK);
  return siCommonMesg.error;
}

/* Stop the manager thread (handled inside the thread's message loop). */
void nuSiMgrStop(void) { nuSiSendMesg(NU_SI_STOP_MGR_MSG, (void*)NULL); }

/* Resume a previously stopped manager thread. */
void nuSiMgrRestart(void) { osStartThread(&siMgrThread); }

/*
 * Manager thread body: register for retrace events, then dispatch each message
 * either to every retrace callback (input polling) or to the one callback that
 * owns the message's major number.
 */
static void nuSiMgrThread(void* arg) {
  NUScClient siClient;
  OSMesg siMgrMesgBuf[NU_SI_MESG_MAX];
  NUSiCommonMesg* siMesg;
  NUCallBackList** siCallBackListPtr;
  s32 ret;
  u16 majorNo;
  u16 minorNo;

  osCreateMesgQueue(&nuSiMgrMesgQ, siMgrMesgBuf, NU_SI_MESG_MAX);
  nuScAddClient(&siClient, &nuSiMgrMesgQ, NU_SC_RETRACE_MSG);
  while (1) {
    (void)osRecvMesg(&nuSiMgrMesgQ, (OSMesg*)&siMesg, OS_MESG_BLOCK);
    siCallBackListPtr = &nuSiCallBackList;
    switch (siMesg->mesg) {
      case NU_SC_RETRACE_MSG:
        // Per-frame poll: run each callback's func[0] in turn, stopping early
        // once one returns nonzero to claim the frame.
        while (*siCallBackListPtr) {
          if ((*siCallBackListPtr)->func[0]) {
            ret = (*((*siCallBackListPtr)->func[0]))(siMesg);
          }
          if (ret) break;
          siCallBackListPtr = &(*siCallBackListPtr)->next;
        }
        break;

      case NU_SI_STOP_MGR_MSG:
        // Acknowledge the stop, drop the retrace registration, and park the
        // thread; the line after osStopThread runs only on a later restart,
        // re-arming the retrace registration.
        osSendMesg(siMesg->rtnMesgQ, NULL, OS_MESG_BLOCK);
        nuScResetClientMesgType(&siClient, 0);
        osStopThread(NULL);
        nuScResetClientMesgType(&siClient, NU_SC_RETRACE_MSG);
        break;

      default:
        // Addressed request: split the message into major (which callback) and
        // minor (which handler within it) numbers and route accordingly.
        majorNo = siMesg->mesg & NU_SI_MSG_MAJOR_NO_MASK;
        minorNo = siMesg->mesg & NU_SI_MSG_MINOR_NO_MASK;
        while (*siCallBackListPtr) {
          if ((*siCallBackListPtr)->majorNo == majorNo) {
            // Dispatch only if the minor number is within the registered
            // handler count; otherwise it is out of range.
            if ((*siCallBackListPtr)->funcNum > minorNo) {
              siMesg->error = (*((*siCallBackListPtr)->func[minorNo]))(siMesg);
            } else {
#ifdef NU_DEBUG
              osSyncPrintf(
                  "nuSiMgr: majorNo %#x minorNo %#x is over callback func "
                  "num(%d).\n",
                  majorNo, minorNo, (*siCallBackListPtr)->funcNum);
              siMesg->error = -1;
#endif
            }

            // Wake a synchronous sender that supplied a return queue.
            if (siMesg->rtnMesgQ != NULL) {
              osSendMesg(siMesg->rtnMesgQ, NULL, OS_MESG_BLOCK);
            }
            break;
          }
          siCallBackListPtr = &(*siCallBackListPtr)->next;
        }
#ifdef NU_DEBUG
        // No callback claimed this major number.
        if (*siCallBackListPtr == NULL) {
          osSyncPrintf("nuSiMgr: no si callback function mesg %#X\n",
                       siMesg->mesg);
        }
#endif
        break;
    }
  }
}
