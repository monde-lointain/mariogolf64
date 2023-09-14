// text

__osPiCreateAccessQueue = 0x800A39B0; // type:func rom:0x7EDB0
__osPiGetAccess        = 0x800A3A04; // type:func rom:0x7EE04
__osPiRelAcces         = 0x800A3A70; // type:func rom:0x7EE70

osAiSetFrequency       = 0x800A3AC0; // type:func rom:0x7EEC0

alCopy                 = 0x800A6F70; // type:func rom:0x82370

osWritebackDCache      = 0x800A70E0; // type:func rom:0x824E0
osWritebackDCacheAll   = 0x800A7160; // type:func rom:0x82560

osVirtualToPhysical    = 0x800A7720; // type:func rom:0x82B20

osCreatePiManager      = 0x800A3000; // type:func rom:0x7E400
__osEPiRawStartDma     = 0x800A3190; // type:func rom:0x7E590

__osDevMgrMain         = 0x800A35F0; // type:func rom:0x7E9F0

bzero                  = 0x800AACC0; // type:func rom:0x860C0

osCreateMesgQueue      = 0x800AAEB0; // type:func rom:0x862B0
osJamMesg              = 0x800AAEE0; // type:func rom:0x862E0
osRecvMesg             = 0x800AB020; // type:func rom:0x86420
osSendMesg             = 0x800AB150; // type:func rom:0x86550
osSetEventMesg         = 0x800AB280; // type:func rom:0x86680

osGetCount             = 0x800AB390; // type:func rom:0x86790

osCreateThread         = 0x800AC200; // type:func rom:0x87600
osDestroyThread        = 0x800AC2D0; // type:func rom:0x876D0
osGetThreadPri         = 0x800AC3B0; // type:func rom:0x877B0
osSetThreadPri         = 0x800AC3D0; // type:func rom:0x877D0
osStartThread          = 0x800AC4A0; // type:func rom:0x878A0
osStopThread           = 0x800AC5C0; // type:func rom:0x879C0
__osDequeueThread      = 0x800AC680; // type:func rom:0x87A80
osYieldThread          = 0x800AC6C0; // type:func rom:0x87AC0

osGetTime              = 0x800AC710; // type:func rom:0x87B10
osSetTimer             = 0x800AC7A0; // type:func rom:0x87BA0

__osTimerServicesInit  = 0x800AC840; // type:func rom:0x87C40
__osTimerInterrupt     = 0x800AC894; // type:func rom:0x87C94
__osSetTimerIntr       = 0x800AC9D0; // type:func rom:0x87DD0
__osInsertTimer        = 0x800ACA30; // type:func rom:0x87E30

__osProbeTLB           = 0x800ACC00; // type:func rom:0x88000

osViSetEvent           = 0x800AD150; // type:func rom:0x86B40
osViSetSpecialFeatures = 0x800AD200; // type:func rom:0x88600
__osViSwapContext      = 0x800AD410; // type:func rom:0x88810

__osInitialize_common  = 0x800AF8A0; // type:func rom:0x8ACA0

__osExceptionPreamble  = 0x800AFB90; // type:func rom:0x8AF90
__osException          = 0x800AFBA0; // type:func rom:0x8AFA0
send_mesg              = 0x800B011C; // type:func rom:0x8B51C
handle_CpU             = 0x800B01D8; // type:func rom:0x8B5D8
__osEnqueueAndYield    = 0x800B020C; // type:func rom:0x8B60C
__osEnqueueThread      = 0x800B0314; // type:func rom:0x8B714
__osPopThread          = 0x800B035C; // type:func rom:0x8B75C
__osDispatchThread     = 0x800B036C; // type:func rom:0x8B76C

__osDisableInt         = 0x800B0500; // type:func rom:0x8B900
__osRestoreInt         = 0x800B0570; // type:func rom:0x8B970

__osSetGlobalIntMask   = 0x800B0590; // type:func rom:0x8B990
__osResetGlobalIntMask = 0x800B05D0; // type:func rom:0x8B9D0

__osPiRawStartDma      = 0x800B0620; // type:func rom:0x8BA20

__osEPiRawWriteIo      = 0x800B0880; // type:func rom:0x8BC80

__osGetCause           = 0x800B1620; // type:func rom:0x8CA20
__osGetSR              = 0x800B1630; // type:func rom:0x8CA30
__osSetCompare         = 0x800B1640; // type:func rom:0x8CA40
__osSetFpcCsr          = 0x800B1650; // type:func rom:0x8CA50
__osSetSR              = 0x800B1660; // type:func rom:0x8CA60
__osSetWatchLo         = 0x800B1670; // type:func rom:0x8CA70

__osSiRawReadIo        = 0x800B1780; // type:func rom:0x8CB80
__osSiRawWriteIo       = 0x800B17D0; // type:func rom:0x8CBD0
__osSiDeviceBusy       = 0x800B2B30; // type:func rom:0x8DF30

osInvalICache          = 0x800A7060; // type:func rom:0x82460

osUnmapTLBAll          = 0x800ACD00; // type:func rom:0x88100

__createSpeedParam     = 0x800AFAD0; // type:func rom:0x8AED0

osMapTLBRdb            = 0x800B1910; // type:func rom:0x8CD10

__udivdi3              = 0x800B3BC0; // type:func rom:0x8EFC0


// data

__osPiDevMgr             = 0x800C7E70; // rom:0xA3270 size:0x1C
__osPiAccessQueueEnabled = 0x800C7EB0; // rom:0xA32B0

__osPreNMI               = 0x800C81F0; // rom:0xA35F0

dp_busy                  = 0x800C8204; // rom:0xA3604

__osThreadTail           = 0x800C8220; // rom:0xA3620 size:0x8
__osRunQueue             = 0x800C8228; // rom:0xA3628
__osActiveQueue          = 0x800C822C; // rom:0xA362C
__osRunningThread        = 0x800C8230; // rom:0xA3630
__osFaultedThread        = 0x800C8234; // rom:0xA3634

__osTimerList            = 0x800C8240; // rom:0xA3640

dpCount                  = 0x8000F3F8; // rom:0xA3608
D_8000F3FC               = 0x8000F3FC; // rom:0xA360C

osClockRate              = 0x800C9460; // rom:0xA4860 size:0x8
osViClock                = 0x800C9468; // rom:0xA4868

__osShutdown             = 0x800C946C; // rom:0xA486C
__OSGlobalIntMask        = 0x800C9470; // rom:0xA4870
__osViCurr               = 0x800C9560; // rom:0xA4960
__osViNext               = 0x800C9564; // rom:0xA4964

// bss

piThread                 = 0x800F97E0; // rom:0xD4BE0 size:0x1B0
__Dom2SpeedParam         = 0x800FEC98; // rom:0xDA098 size:0x74
__osViIntrCount          = 0x800FF1E0; // rom:0xDA5E0
__CartRomHandle          = 0x80105BC0; // rom:0xE0FC0 size:0x74
__Dom1SpeedParam         = 0x80106248; // rom:0xE1648 size:0x74
__osEventStateTab        = 0x801B7078; // rom:0x192478 size:0x78
__osFinalrom             = 0x801B74F8; // rom:0x1928F8
