// text

nuPiReadRom = 0x80025C50; // type:func rom:0x1050
nuGfxInit = 0x80028300; // type:func rom:0x3700
nuScCreateScheduler = 0x800283B0; // type:func rom:0x37B0
nuScAddClient = 0x8002891C; // type:func rom:0x3D1C
nuScResetClientMesgType = 0x800289AC; // type:func rom:0x3DAC
nuGfxTaskMgr = 0x80029440; // type:func rom:0x4840
nuGfxTaskMgrInit = 0x800295BC; // type:func rom:0x49BC
nuGfxTaskStart = 0x80029774; // type:func rom:0x4B74
nuBoot = 0x800994B0; // type:func rom:0x748B0
idle = 0x8009951C; // type:func rom:0x7491C
nuGfxThreadStart = 0x800A1500; // type:func rom:0x7C900
nuPiInit = 0x800A16E0; // type:func rom:0x7CAE0
nuPiInitSram = 0x800A1720; // type:func rom:0x7CB20
gfxThread = 0x800A1430; // type:func rom:0x7C830
nuGfxFuncSet = 0x800A1560; // type:func rom:0x7C960
nuGfxSwapCfbFuncSet = 0x800A15A0; // type:func rom:0x7C9A0
nuGfxSwapCfb = 0x800A15E0; // type:func rom:0x7C9E0
nuContInit = 0x800A1890; // type:func rom:0x7CC90
nuContRmbMgrInit = 0x800A1960; // type:func rom:0x7CD60
nuGfxTaskAllEndWait = 0x800A1600; // type:func rom:0x7CA00
nuSiMgrInit = 0x800A278C; // type:func rom:0x7DB8C
nuContMgrInit = 0x800A2B90; // type:func rom:0x7DF90

// bss

nugfx_ucode = 0x800B6680;
nuGfxFunc = 0x800C7DC0;
MainThread = 0x800E4E80; // size:0x1B0
IdleThread = 0x800E4CD0; // size:0x1B0
nuGfxThread = 0x800E72F0; // size:0x1B0
PiMesgQ = 0x800F74A0; // size:0x18
IdleStack = 0x800E7030;
PiMesgBuf = 0x800F74B8;
SramHandle = 0x800F7580; // size:0x74
siMgrThread = 0x800F7620; // size:0x1B0
nuGfxUcode = 0x801061B0;
nuMainStack = 0x8010C2D0;
nuPiSramHandle = 0x8012F4D8; // size:0x74
nuPiCartHandle = 0x801B55C8;
nuSiMesgQ = 0x801B93C0; // size:0x18
