#include <nusys.h>
#undef nuGfxInit

extern void func_80029250(void *, s32);
extern u32 D_800B6688;
extern u32 D_800FED14;
extern u32 D_800B6680;
extern u32 D_801061B0;
extern u32 D_B6698;

void nuGfxInit(void) {
    Gfx gfxList[0x100];
    Gfx *gfxList_ptr;

    nuGfxThreadStart();
    func_80029250(&D_800B6688, 3);
    D_800FED14 = 0x80000400;
    nuGfxSwapCfbFuncSet(nuGfxSwapCfb);
    D_801061B0 = (u32)&D_800B6680;
    nuGfxTaskMgrInit();
    gfxList_ptr = gfxList;
    gSPDisplayList(gfxList_ptr++, (u32)&D_B6698);
    gDPFullSync(gfxList_ptr++);
    gSPEndDisplayList(gfxList_ptr++);
    nuGfxTaskStart(gfxList, (s32)(gfxList_ptr - gfxList) * sizeof(Gfx), 0, 0);
    nuGfxTaskAllEndWait();
}
