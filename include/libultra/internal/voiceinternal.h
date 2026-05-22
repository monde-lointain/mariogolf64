#ifndef VOICE_INTERNAL_H
#define VOICE_INTERNAL_H

// Various prototypes and externs that don't show up in os_voice.h
extern s32 __osPfsLastChannel;

extern u8 __osVoiceContDataCrc(u8* data, u32 length);
extern s32 __osVoiceCheckResult(OSVoiceHandle* hd, u8* stat);
extern s32 __osVoiceGetStatus(OSMesgQueue* mq, s32 port, u8* status);
extern s32 __osVoiceContRead2(OSMesgQueue* mq, int channel, u16 address, u8* buffer);
extern s32 __osVoiceContWrite4(OSMesgQueue* mq, int channel, u16 address, u8 dst[4]);
extern s32 __osVoiceContWrite20(OSMesgQueue* mq, int channel, u16 address, u8* buffer);
extern s32 __osVoiceContRead36(OSMesgQueue* mq, s32 channel, u16 address, u8* buffer);
extern s32 __osVoiceSetADConverter(OSMesgQueue* mq, s32 channel, u8 data);

#endif
