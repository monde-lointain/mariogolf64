#include "macros.h"
#include "PR/os_internal.h"
#include "controller.h"

extern u8 nintendo[48];
extern u8 mmc_type[20];

s32 osGbpakReadId(OSPfs* pfs, OSGbpakId* id, u8* status) {
    s32 i;
    s32 ret;
    u8 isum;
    u8 buf[96];

    ret = osGbpakGetStatus(pfs, status);

    if (ret == PFS_ERR_NEW_GBCART) {
        ret = osGbpakGetStatus(pfs, status);
    }

    if (ret == PFS_ERR_NEW_GBCART) {
        return PFS_ERR_CONTRFAIL;
    } else if (ret == 0) {
        if (!(*status & OS_GBPAK_POWER)) {
            ERRCK(osGbpakPower(pfs, OS_GBPAK_POWER_ON));
        }

        ERRCK(osGbpakReadWrite(pfs, OS_READ, 0x100U, buf, ARRLEN(buf)));

        ret = osGbpakGetStatus(pfs, status);

        if (ret == PFS_ERR_NEW_GBCART) {
            ret = PFS_ERR_CONTRFAIL;
        }

        if (ret != 0) {
            return ret;
        }

        if (!(*status & OS_GBPAK_RSTB_STATUS)) {
            return PFS_ERR_CONTRFAIL;
        }

        if (bcmp(nintendo, buf + 4, ARRLEN(nintendo))) {
            return 4;
        }
        for (i = 0x34, isum = 0; i < 0x4E; i++) {
            isum += buf[i];
        }

        if ((isum + 0x19) & 0xFF) {
            return 4;
        }

        bcopy(buf, id, 0x50);

        if (id->cart_type < 0x14) {
            pfs->version = (int)mmc_type[id->cart_type];
        }

        pfs->dir_size = (int)id->ram_size;
    }

    return ret;
}
