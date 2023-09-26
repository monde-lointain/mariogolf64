// text

__assert               = 0x800298E0; // type:func rom:0x4CE0

guNormalize            = 0x80029900; // type:func rom:0x4D00

guMtxF2L               = 0x80067B00; // type:func rom:0x42F00
guMtxL2F               = 0x80067C0C; // type:func rom:0x4300C
guMtxIdentF            = 0x80067CB4; // type:func rom:0x430B4
guMtxIdent             = 0x80067D04; // type:func rom:0x43104

osSetIntMask           = 0x800A2F60; // type:func rom:0x7E360

osCreatePiManager      = 0x800A3000; // type:func rom:0x7E400
__osEPiRawStartDma     = 0x800A3190; // type:func rom:0x7E590
osEPiStartDma          = 0x800A3380; // type:func rom:0x7E780
osEPiLinkHandle        = 0x800A3420; // type:func rom:0x7E820

osCartRomInit          = 0x800A3470; // type:func rom:0x7E870

__osDevMgrMain         = 0x800A35F0; // type:func rom:0x7E9F0

__osPiCreateAccessQueue = 0x800A39B0; // type:func rom:0x7EDB0
__osPiGetAccess        = 0x800A3A04; // type:func rom:0x7EE04
__osPiRelAcces         = 0x800A3A70; // type:func rom:0x7EE70

osAiSetFrequency       = 0x800A3AC0; // type:func rom:0x7EEC0

alEnvmixerPull         = 0x800A50D0; // type:func rom:0x804D0
alEnvmixerParam        = 0x800A55D0; // type:func rom:0x809D0
_pullSubFrame          = 0x800A56A4; // type:func rom:0x80AA4

alSynNew               = 0x80051E34;

alInit                 = 0x800A6E30; // type:func rom:0x82230
alClose                = 0x800A6E5C; // type:func rom:0x8225C
alLink                 = 0x800A6E8C; // type:func rom:0x8228C
alUnlink               = 0x800A6EAC; // type:func rom:0x822AC
alHeapInit             = 0x800A6EE0; // type:func rom:0x822E0
alHeapDBAlloc          = 0x800A6F20; // type:func rom:0x82320
alCopy                 = 0x800A6F70; // type:func rom:0x82370

osInvalDCache          = 0x800A6FB0; // type:func rom:0x823B0

osWritebackDCache      = 0x800A70E0; // type:func rom:0x824E0
osWritebackDCacheAll   = 0x800A7160; // type:func rom:0x82560

osContInit             = 0x800A7410; // type:func rom:0x82810

osVirtualToPhysical    = 0x800A7720; // type:func rom:0x82B20

guAlignF               = 0x800A7780; // type:func rom:0x82B80

cosf                   = 0x800A7B20; // type:func rom:0x82F20

guLookAtF              = 0x800A7C70; // type:func rom:0x83070
guLookAtReflectF       = 0x800A8DE0; // type:func rom:0x841E0
guLookAtReflect        = 0x800A9358; // type:func rom:0x84758
guMtxCatF              = 0x800A93D0; // type:func rom:0x847D0
guMtxXFMF              = 0x800A94AC; // type:func rom:0x848AC
guMtxCatL              = 0x800A9560; // type:func rom:0x84960
guMtxXFML              = 0x800A95D4; // type:func rom:0x849D4
guOrthoF               = 0x800A9660; // type:func rom:0x84A60
guOrtho                = 0x800A9790; // type:func rom:0x84B90
guPerspectiveF         = 0x800A98E0; // type:func rom:0x84CE0
guPerspective          = 0x800A8A90; // type:func rom:0x84E90
guPositionF            = 0x800A9C60; // type:func rom:0x85060
guPosition             = 0x800A9E38; // type:func rom:0x85238
guRandom               = 0x800AA020; // type:func rom:0x85420
guRotateF              = 0x800AA050; // type:func rom:0x85450
guRotate               = 0x800AA1AC; // type:func rom:0x855AC
guRotateRPYF           = 0x800AA330; // type:func rom:0x85730
guRotateRPY            = 0x800AA4B8; // type:func rom:0x858B8
guScaleF               = 0x800AA650; // type:func rom:0x85A50
guScale                = 0x800AA6B0; // type:func rom:0x85AB0

sinf                   = 0x800AA730; // type:func rom:0x85B30

guTranslateF           = 0x800AA8D0; // type:func rom:0x85CD0
guTranslate            = 0x800AA924; // type:func rom:0x85D24

bcopy                  = 0x800AA9A0; // type:func rom:0x85DA0
bzero                  = 0x800AACC0; // type:func rom:0x860C0

sprintf                = 0x800AADF0; // type:func rom:0x861F0
proutSprintf           = 0x800AAE48; // type:func rom:0x86248

strchr                 = 0x800AAD60; // type:func rom:0x86160
strlen                 = 0x800AADA0; // type:func rom:0x861A0
memcpy                 = 0x800AADC4; // type:func rom:0x861C4

osCreateMesgQueue      = 0x800AAEB0; // type:func rom:0x862B0
osJamMesg              = 0x800AAEE0; // type:func rom:0x862E0
osRecvMesg             = 0x800AB020; // type:func rom:0x86420
osSendMesg             = 0x800AB150; // type:func rom:0x86550
osSetEventMesg         = 0x800AB280; // type:func rom:0x86680

osDpSetStatus          = 0x800AB330; // type:func rom:0x86730
osDpGetCounters        = 0x800AB340; // type:func rom:0x86740
osGetCount             = 0x800AB390; // type:func rom:0x86790
osSpTaskLoad           = 0x800AB3A0; // type:func rom:0x867A0

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

osCreateViManager      = 0x800ACE10; // type:func rom:0x88210
osViSetEvent           = 0x800AD150; // type:func rom:0x86B40
osViSetMode            = 0x800AD1B0; // type:func rom:0x885B0
osViSwapBuffer         = 0x800AD3C0; // type:func rom:0x887C0
osViSetSpecialFeatures = 0x800AD200; // type:func rom:0x88600
__osViSwapContext      = 0x800AD410; // type:func rom:0x88810
osViBlack              = 0x800AD720; // type:func rom:0x88B20

osSyncPrintf           = 0x800AAE80; // type:func rom:0x86280
rmonPrintf             = 0x800AAE94; // type:func rom:0x86294

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

_Printf                = 0x800B0B30; // type:func rom:0x8BF30

__osGetCause           = 0x800B1620; // type:func rom:0x8CA20
__osGetSR              = 0x800B1630; // type:func rom:0x8CA30
__osSetCompare         = 0x800B1640; // type:func rom:0x8CA40
__osSetFpcCsr          = 0x800B1650; // type:func rom:0x8CA50
__osSetSR              = 0x800B1660; // type:func rom:0x8CA60
__osSetWatchLo         = 0x800B1670; // type:func rom:0x8CA70
__osSpSetStatus        = 0x800B16B0; // type:func rom:0x8CAB0
__osSpSetPc            = 0x800B16C0; // type:func rom:0x8CAC0

__osSiRawReadIo        = 0x800B1780; // type:func rom:0x8CB80
__osSiRawWriteIo       = 0x800B17D0; // type:func rom:0x8CBD0
__osViInit             = 0x800B1970; // type:func rom:0x8CD70
__osSiDeviceBusy       = 0x800B2B30; // type:func rom:0x8DF30

osInvalICache          = 0x800A7060; // type:func rom:0x82460

osContStartReadData    = 0x800A7230; // type:func rom:0x82630
osContGetReadData      = 0x800A72B8; // type:func rom:0x826B8

guAlignF               = 0x800A7780; // type:func rom:0x82B80
guLookAtHiliteF        = 0x800A8380; // type:func rom:0x83780
guLookAtHilite         = 0x800A8D04; // type:func rom:0x84104

osCreateScheduler      = 0x800AB650; // type:func rom:0x86A50

osUnmapTLBAll          = 0x800ACD00; // type:func rom:0x88100

__osContRamWrite       = 0x800AF610; // type:func rom:0x8AA10
osGbpakInit            = 0x800AD780; // type:func rom:0x88B80
osGbpakGetStatus       = 0x800AD980; // type:func rom:0x88D80
osGbpakPower           = 0x800ADAA0; // type:func rom:0x88EA0
osGbpakReadWrite       = 0x800ADBC0; // type:func rom:0x88FC0
osGbpakReadId          = 0x800ADD90; // type:func rom:0x89190
osGbpakCheckConnector  = 0x800ADF20; // type:func rom:0x89320

osAfterPreNMI          = 0x800AF880; // type:func rom:0x8AC80

__createSpeedParam     = 0x800AFAD0; // type:func rom:0x8AED0

__osSpDeviceBusy       = 0x800B1680; // type:func rom:0x8CA80
__osSpRawStartDma      = 0x800B16F0; // type:func rom:0x8CAF0
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

__osPiTable              = 0x800C7E8C; // rom:0xA328C

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
