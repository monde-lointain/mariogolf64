#include "system/crash_screen.h"

#include <ultra64.h>

#include "PR/R4300.h"
#include "debug_text.h"
#include "sound.h"
#include "system/c_language.h"
#include "system/compiler.h"

#define FAULT_MESSAGE 0x10

#define FAULT_SOUND_CHANNEL 11

#define FAULT_TITLE_ROW 5
#define FAULT_PC_ROW 6
#define FAULT_TEXT_COLUMN 0
#define FIELD_TEXT_COLUMN 3

#define FAULT_THREAD_STACK_SIZE 0x2000

typedef struct RegisterFieldValue {
    u32 mask;
    u32 value;
    char *text;
} RegisterFieldValue;

static OSThread *f_FaultedThread;
static s32 f_UnusedFaultThreadValue;

static OSThread f_FaultThread UNUSED;
static u8 f_FaultThreadStack[FAULT_THREAD_STACK_SIZE] UNUSED;

static OSMesgQueue f_FaultMessageQueue;

OSThread *__osGetCurrFaultedThread(void);
void RunFaultThread(void *arg);
void PrintFaultContext(OSThread *thread);
void PrintRegisterFields(u32 registerValue, char *name, RegisterFieldValue *fields);

FILE_STATIC char f_FaultText[FAULT_TEXT_SIZE];

#define PrintFaultMessage(column, row) PrintMessage((s8 *)f_FaultText, (column), (row))

static s32 f_UnusedFaultContextValue = 0;
static s32 f_FaultTextRow = 7;

const char g_EmptyString[] = "";

void RunBootHook(void) {
    if (FALSE) {
        osSyncPrintf("\n=>mp\n");
        osSyncPrintf("\n=>mp-start f thread\n");
        osCreateThread(NULL, 0, RunFaultThread, NULL, NULL, 0);
    }
}

void RunFaultThread(void *arg UNUSED) {
    OSMesg message;

    osSetEventMesg(OS_EVENT_FAULT, &f_FaultMessageQueue, (OSMesg)FAULT_MESSAGE);
    f_UnusedFaultThreadValue = 0;
    while (TRUE) {
        osSyncPrintf("\n=>fp-wait for msg\n");
        osRecvMesg(&f_FaultMessageQueue, &message, OS_MESG_BLOCK);
        osSyncPrintf("\n=> fltproc - got a flt msg.\n");
        f_FaultedThread = __osGetCurrFaultedThread();
        if (f_FaultedThread != NULL) {
            PlaySound(SOUND_CONTACT_WARIO, FAULT_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintFaultContext(f_FaultedThread);
        }
    }
}

static RegisterFieldValue f_CauseFields[] = {
    {CAUSE_BD, CAUSE_BD, "BD"},
    {CAUSE_IP8, CAUSE_IP8, "IP8"},
    {CAUSE_IP7, CAUSE_IP7, "IP7"},
    {CAUSE_IP6, CAUSE_IP6, "IP6"},
    {CAUSE_IP5, CAUSE_IP5, "IP5"},
    {CAUSE_IP4, CAUSE_IP4, "IP4"},
    {CAUSE_IP3, CAUSE_IP3, "IP3"},
    {CAUSE_SW2, CAUSE_SW2, "IP2"},
    {CAUSE_SW1, CAUSE_SW1, "IP1"},
    {CAUSE_EXCMASK, EXC_INT, "Interrupt"},
    {CAUSE_EXCMASK, EXC_MOD, "TLB modification exception"},
    {CAUSE_EXCMASK, EXC_RMISS, "TLB exception on load or instruction fetch"},
    {CAUSE_EXCMASK, EXC_WMISS, "TLB exception on store"},
    {CAUSE_EXCMASK, EXC_RADE, "Address error on load or instruction fetch"},
    {CAUSE_EXCMASK, EXC_WADE, "Address error on store"},
    {CAUSE_EXCMASK, EXC_IBE, "Bus error exception on instruction fetch"},
    {CAUSE_EXCMASK, EXC_DBE, "Bus error exception on data reference"},
    {CAUSE_EXCMASK, EXC_SYSCALL, "System call exception"},
    {CAUSE_EXCMASK, EXC_BREAK, "Breakpoint exception"},
    {CAUSE_EXCMASK, EXC_II, "Reserved instruction exception"},
    {CAUSE_EXCMASK, EXC_CPU, "Coprocessor unusable exception"},
    {CAUSE_EXCMASK, EXC_OV, "Arithmetic overflow exception"},
    {CAUSE_EXCMASK, EXC_TRAP, "Trap exception"},
    {CAUSE_EXCMASK, EXC_VCEI, "Virtual coherency exception on intruction fetch"},
    {CAUSE_EXCMASK, EXC_FPE, "Floating point exception (see fpcsr)"},
    {CAUSE_EXCMASK, EXC_WATCH, "Watchpoint exception"},
    {CAUSE_EXCMASK, EXC_VCED, "Virtual coherency exception on data reference"},
    {0, 0, ""},
};

static RegisterFieldValue f_StatusFields[] = {
    {SR_CU3, SR_CU3, "CU3"},
    {SR_CU2, SR_CU2, "CU2"},
    {SR_CU1, SR_CU1, "CU1"},
    {SR_CU0, SR_CU0, "CU0"},
    {SR_RP, SR_RP, "RP"},
    {SR_FR, SR_FR, "FR"},
    {SR_RE, SR_RE, "RE"},
    {SR_BEV, SR_BEV, "BEV"},
    {SR_TS, SR_TS, "TS"},
    {SR_SR, SR_SR, "SR"},
    {SR_CH, SR_CH, "CH"},
    {SR_CE, SR_CE, "CE"},
    {SR_DE, SR_DE, "DE"},
    {SR_IBIT8, SR_IBIT8, "IM8"},
    {SR_IBIT7, SR_IBIT7, "IM7"},
    {SR_IBIT6, SR_IBIT6, "IM6"},
    {SR_IBIT5, SR_IBIT5, "IM5"},
    {SR_IBIT4, SR_IBIT4, "IM4"},
    {SR_IBIT3, SR_IBIT3, "IM3"},
    {SR_IBIT2, SR_IBIT2, "IM2"},
    {SR_IBIT1, SR_IBIT1, "IM1"},
    {SR_KX, SR_KX, "KX"},
    {SR_SX, SR_SX, "SX"},
    {SR_UX, SR_UX, "UX"},
    {SR_KSU_MASK, SR_KSU_USR, "USR"},
    {SR_KSU_MASK, SR_KSU_SUP, "SUP"},
    {SR_KSU_MASK, SR_KSU_KER, "KER"},
    {SR_ERL, SR_ERL, "ERL"},
    {SR_EXL, SR_EXL, "EXL"},
    {SR_IE, SR_IE, "IE"},
    {0, 0, ""},
};

static RegisterFieldValue f_FloatStatusFields[] = {
    {FPCSR_FS, FPCSR_FS, "FS"},
    {FPCSR_C, FPCSR_C, "C"},
    {FPCSR_CE, FPCSR_CE, "Unimplemented operation"},
    {FPCSR_CV, FPCSR_CV, "Invalid operation"},
    {FPCSR_CZ, FPCSR_CZ, "Division by zero"},
    {FPCSR_CO, FPCSR_CO, "Overflow"},
    {FPCSR_CU, FPCSR_CU, "Underflow"},
    {FPCSR_CI, FPCSR_CI, "Inexact operation"},
    {FPCSR_EV, FPCSR_EV, "EV"},
    {FPCSR_EZ, FPCSR_EZ, "EZ"},
    {FPCSR_EO, FPCSR_EO, "EO"},
    {FPCSR_EU, FPCSR_EU, "EU"},
    {FPCSR_EI, FPCSR_EI, "EI"},
    {FPCSR_FV, FPCSR_FV, "FV"},
    {FPCSR_FZ, FPCSR_FZ, "FZ"},
    {FPCSR_FO, FPCSR_FO, "FO"},
    {FPCSR_FU, FPCSR_FU, "FU"},
    {FPCSR_FI, FPCSR_FI, "FI"},
    {FPCSR_RM_MASK, FPCSR_RM_RN, "RN"},
    {FPCSR_RM_MASK, FPCSR_RM_RZ, "RZ"},
    {FPCSR_RM_MASK, FPCSR_RM_RP, "RP"},
    {FPCSR_RM_MASK, FPCSR_RM_RM, "RM"},
    {0, 0, ""},
};

void PrintFaultText(void) {
    osSyncPrintf(f_FaultText);
}

void PrintFaultContext(OSThread *thread) {
    __OSThreadContext *context = &thread->context;

    if (thread != NULL) {
        f_UnusedFaultContextValue = 0;
    }
    else {
        f_UnusedFaultContextValue = 0;
    }

    sprintf(f_FaultText, "\nFault in thread %d:\n\n", thread->id);
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, FAULT_TITLE_ROW);
    sprintf(f_FaultText, "epc\t\t0x%08x\n", context->pc);
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, FAULT_PC_ROW);
    PrintRegisterFields(context->cause, "cause", f_CauseFields);
    PrintRegisterFields(context->sr, "sr", f_StatusFields);
    sprintf(f_FaultText, "badvaddr\t0x%08.8x\n\n", context->badvaddr);
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "at 0x%08.8x v0 0x%08.8x v1 0x%08.8x\n",
        context->at,
        context->v0,
        context->v1
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "a0 0x%08.8x a1 0x%08.8x a2 0x%08.8x\n",
        context->a0,
        context->a1,
        context->a2
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "a3 0x%08.8x t0 0x%08.8x t1 0x%08.8x\n",
        context->a3,
        context->t0,
        context->t1
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "t2 0x%08.8x t3 0x%08.8x t4 0x%08.8x\n",
        context->t2,
        context->t3,
        context->t4
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "t5 0x%08.8x t6 0x%08.8x t7 0x%08.8x\n",
        context->t5,
        context->t6,
        context->t7
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "s0 0x%08.8x s1 0x%08.8x s2 0x%08.8x\n",
        context->s0,
        context->s1,
        context->s2
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "s3 0x%08.8x s4 0x%08.8x s5 0x%08.8x\n",
        context->s3,
        context->s4,
        context->s5
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "s6 0x%08.8x s7 0x%08.8x t8 0x%08.8x\n",
        context->s6,
        context->s7,
        context->t8
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(
        f_FaultText,
        "t9 0x%08.8x gp 0x%08.8x sp 0x%08.8x\n",
        context->t9,
        context->gp,
        context->sp
    );
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    sprintf(f_FaultText, "s8 0x%08.8x ra 0x%08.8x\n\n", context->s8, context->ra);
    PrintFaultText();
    PrintFaultMessage(FAULT_TEXT_COLUMN, f_FaultTextRow++);
    PrintRegisterFields(context->fpcsr, "fpcsr", f_FloatStatusFields);
    sprintf(
        f_FaultText,
        "\nf0  %.7e f2  %.7e f4  %.7e f6  %.7e\n",
        context->fp0.f.f_even,
        context->fp2.f.f_even,
        context->fp4.f.f_even,
        context->fp6.f.f_even
    );
    PrintFaultText();
    sprintf(
        f_FaultText,
        "f8  %.7e f10 %.7e f12 %.7e f14 %.7e\n",
        context->fp8.f.f_even,
        context->fp10.f.f_even,
        context->fp12.f.f_even,
        context->fp14.f.f_even
    );
    PrintFaultText();
    sprintf(
        f_FaultText,
        "f16 %.7e f18 %.7e f20 %.7e f22 %.7e\n",
        context->fp16.f.f_even,
        context->fp18.f.f_even,
        context->fp20.f.f_even,
        context->fp22.f.f_even
    );
    PrintFaultText();
    sprintf(
        f_FaultText,
        "f24 %.7e f26 %.7e f28 %.7e f30 %.7e\n\n",
        context->fp24.f.f_even,
        context->fp26.f.f_even,
        context->fp28.f.f_even,
        context->fp30.f.f_even
    );
    PrintFaultText();
    sprintf(f_FaultText, "d0  %.15e\td2  %.15e\n", context->fp0.d, context->fp2.d);
    PrintFaultText();
    sprintf(f_FaultText, "d4  %.15e\td6  %.15e\n", context->fp4.d, context->fp6.d);
    PrintFaultText();
    sprintf(f_FaultText, "d8  %.15e\td10 %.15e\n", context->fp8.d, context->fp10.d);
    PrintFaultText();
    sprintf(f_FaultText, "d12 %.15e\td14 %.15e\n", context->fp12.d, context->fp14.d);
    PrintFaultText();
    sprintf(f_FaultText, "d16 %.15e\td18 %.15e\n", context->fp16.d, context->fp18.d);
    PrintFaultText();
    sprintf(f_FaultText, "d20 %.15e\td22 %.15e\n", context->fp20.d, context->fp22.d);
    PrintFaultText();
    sprintf(f_FaultText, "d24 %.15e\td26 %.15e\n", context->fp24.d, context->fp26.d);
    PrintFaultText();
    sprintf(f_FaultText, "d28 %.15e\td30 %.15e\n", context->fp28.d, context->fp30.d);
    PrintFaultText();
}

void PrintRegisterFields(u32 registerValue, char *name, RegisterFieldValue *fields) {
    Boolean isFirst = TRUE;

    sprintf(f_FaultText, "%s\t\t0x%08x ", name, (unsigned int)registerValue);
    PrintFaultText();
    sprintf(f_FaultText, "<");
    PrintFaultText();
    while (fields->mask != 0) {
        if ((registerValue & fields->mask) == fields->value) {
            if (isFirst) {
                isFirst = FALSE;
            }
            else {
                sprintf(f_FaultText, ",");
                PrintFaultText();
            }

            sprintf(f_FaultText, "%s", fields->text);
            PrintFaultText();
            PrintFaultMessage(FIELD_TEXT_COLUMN, f_FaultTextRow++);
        }

        fields++;
    }

    sprintf(f_FaultText, ">\n");
    PrintFaultText();
}
