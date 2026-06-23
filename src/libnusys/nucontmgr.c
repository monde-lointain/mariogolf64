/*
 * Controller Manager.
 *
 * Drives controller input under the SI Manager. At every retrace it reads all
 * pads into the shared nuContData buffer, optionally invoking a user callback
 * with the fresh data; it also serves on-demand read and query requests posted
 * through the SI message path. A mutex queue guards the shared buffer, and a
 * lock key lets an application freeze the per-retrace updates (see
 * nucontdatalock.c). nuContMgrInit installs the dispatch table below.
 */
#include <nusys.h>

extern OSMesg nuContWaitMesgBuf;
extern OSMesgQueue nuContDataMutexQ;
extern OSMesg nuContDataMutexBuf;

/* Optional per-retrace hook, set via nuContReadFuncSet; NULL when unused. */
NUContReadFunc nuContReadFunc = NULL;

static s32 contRetrace(NUSiCommonMesg* mesg);
static s32 contRead(NUSiCommonMesg* mesg);
static s32 contReadNW(NUSiCommonMesg* mesg);
static s32 contQuery(NUSiCommonMesg* mesg);

/*
 * Dispatch table indexed by the message's minor command number (retrace, read,
 * non-waiting read, query). The trailing NULL terminates the list.
 */
static s32 (*funcList[])(NUSiCommonMesg*) = {contRetrace, contRead, contReadNW,
                                             contQuery, NULL};

NUCallBackList nuContCallBack = {NULL, funcList, NU_SI_MAJOR_NO_CONT};

/*
 * Initialize the Controller Manager and register it with the SI Manager.
 *
 * Sets up the wait and mutex queues, registers the callback table, kicks off an
 * initial status query, then counts the connected standard controllers and
 * returns a bit pattern of their ports (bit 0 = port 1, ...).
 */
u8 nuContMgrInit(void) {
  int cnt;
  u8 pattern;
  u8 bitmask;

  nuContDataUnLock();
  osCreateMesgQueue(&nuContWaitMesgQ, &nuContWaitMesgBuf, 1);
  osCreateMesgQueue(&nuContDataMutexQ, &nuContDataMutexBuf, 1);
  nuSiCallBackAdd(&nuContCallBack);
  nuContQueryRead();

  // Build the connected-controller bit pattern from the query results.
  nuContNum = 0;
  bitmask = 1;
  pattern = 0;
  for (cnt = 0; cnt < NU_CONT_MAXCONTROLLERS; cnt++) {
    if (nuContStatus[cnt].errno) {
      continue;
    }
    // Count only standard controllers toward nuContNum / the pattern.
    if ((nuContStatus[cnt].type & CONT_TYPE_MASK) == CONT_TYPE_NORMAL) {
      nuContNum++;
      pattern |= bitmask;
    }
    bitmask <<= 1;
  }
  return pattern;
}

/* Unregister the Controller Manager from the SI Manager. */
void nuContMgrRemove(void) { nuSiCallBackRemove(&nuContCallBack); }

/*
 * Acquire / release the mutex guarding the shared nuContData buffer. Close
 * takes the mutex (so the buffer can be touched safely); Open returns it.
 */
void nuContDataClose(void) {
  osSendMesg(&nuContDataMutexQ, NULL, OS_MESG_BLOCK);
}

void nuContDataOpen(void) {
  osRecvMesg(&nuContDataMutexQ, NULL, OS_MESG_BLOCK);
}

/*
 * Start a controller read and block until it completes.
 *
 * If lockflag matches the active data-lock key the result is left in the SI
 * buffer and not copied out, honoring a caller that has frozen the shared
 * buffer; otherwise the read data is copied into pad under the buffer mutex.
 */
static s32 contReadData(OSContPad* pad, u32 lockflag) {
  s32 rtn;

  rtn = osContStartReadData(&nuSiMesgQ);
  if (rtn) {
    return rtn;
  }
  osRecvMesg(&nuSiMesgQ, NULL, OS_MESG_BLOCK);
  if (lockflag & nuContDataLockKey) {
    return rtn;
  }
  nuContDataClose();
  osContGetReadData(pad);
  nuContDataOpen();
  return rtn;
}

/* Query handler: refresh the nuContStatus table (NU_CONT_QUERY_MSG). */
static s32 contQuery(NUSiCommonMesg* mesg) {
  s32 rtn;

  rtn = osContStartQuery(&nuSiMesgQ);
  if (rtn) {
    return rtn;
  }
  osRecvMesg(&nuSiMesgQ, NULL, OS_MESG_BLOCK);
  osContGetQuery(nuContStatus);
  return rtn;
}

/*
 * Per-retrace handler: refresh the shared pad buffer and fire the user hook.
 *
 * When the data lock is held the refresh is skipped entirely so the frozen
 * buffer is preserved. Otherwise a wait token is drained, all pads are read
 * into nuContData (lockflag 1, honoring the lock), the optional read callback
 * runs, and the wait token is replaced so blocked readers can proceed.
 */
static s32 contRetrace(NUSiCommonMesg* mesg) {
  if (nuContDataLockKey) {
    return NU_SI_CALLBACK_CONTINUE;
  }
  osRecvMesg(&nuContWaitMesgQ, NULL, OS_MESG_NOBLOCK);
  contReadData(nuContData, 1);
  if (nuContReadFunc != NULL) {
    (*nuContReadFunc)(mesg->mesg);
  }
  osSendMesg(&nuContWaitMesgQ, NULL, OS_MESG_NOBLOCK);
  return NU_SI_CALLBACK_CONTINUE;
}

/* On-demand read into the caller's buffer, ignoring the lock
 * (NU_CONT_READ_MSG). */
static s32 contRead(NUSiCommonMesg* mesg) {
  return contReadData((OSContPad*)mesg->dataPtr, 0);
}

/*
 * Non-waiting on-demand read into the shared buffer (NU_CONT_READ_NW_MSG).
 *
 * Drains a wait token first so it does not block behind the retrace path, reads
 * into nuContData, then runs the user hook on success.
 */
static s32 contReadNW(NUSiCommonMesg* mesg) {
  s32 rtn;

  osRecvMesg(&nuContWaitMesgQ, NULL, OS_MESG_NOBLOCK);
  rtn = contReadData(nuContData, 0);
  if (rtn) {
    return rtn;
  }
  if (nuContReadFunc != NULL) {
    (*nuContReadFunc)(mesg->mesg);
  }
  return rtn;
}
