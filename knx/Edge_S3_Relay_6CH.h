#pragma once


#define paramDelay(time) (uint32_t)( \
            (time & 0xC000) == 0xC000 ? (time & 0x3FFF) * 100 : \
            (time & 0xC000) == 0x0000 ? (time & 0x3FFF) * 1000 : \
            (time & 0xC000) == 0x4000 ? (time & 0x3FFF) * 60000 : \
            (time & 0xC000) == 0x8000 ? ((time & 0x3FFF) > 1000 ? 3600000 : \
                                         (time & 0x3FFF) * 3600000 ) : 0 )

// Parameter with single occurrence
#define Parameter_format                     0      // uint8_t
#define Global_flags                         1      // uint8_t
#define Channel_1_Enabled                    4      // uint8_t
#define Channel_1_Startup_ON                 5      // uint8_t
#define Channel_1_OFF_on_loss                6      // uint8_t
#define Channel_1_Loss_timeout_seconds       8      // uint16_t
#define Channel_1_Pulse_milliseconds        10      // uint16_t
#define Channel_2_Enabled                   12      // uint8_t
#define Channel_2_Startup_ON                13      // uint8_t
#define Channel_2_OFF_on_loss               14      // uint8_t
#define Channel_2_Loss_timeout_seconds      16      // uint16_t
#define Channel_2_Pulse_milliseconds        18      // uint16_t
#define Channel_3_Enabled                   20      // uint8_t
#define Channel_3_Startup_ON                21      // uint8_t
#define Channel_3_OFF_on_loss               22      // uint8_t
#define Channel_3_Loss_timeout_seconds      24      // uint16_t
#define Channel_3_Pulse_milliseconds        26      // uint16_t
#define Channel_4_Enabled                   28      // uint8_t
#define Channel_4_Startup_ON                29      // uint8_t
#define Channel_4_OFF_on_loss               30      // uint8_t
#define Channel_4_Loss_timeout_seconds      32      // uint16_t
#define Channel_4_Pulse_milliseconds        34      // uint16_t
#define Channel_5_Enabled                   36      // uint8_t
#define Channel_5_Startup_ON                37      // uint8_t
#define Channel_5_OFF_on_loss               38      // uint8_t
#define Channel_5_Loss_timeout_seconds      40      // uint16_t
#define Channel_5_Pulse_milliseconds        42      // uint16_t
#define Channel_6_Enabled                   44      // uint8_t
#define Channel_6_Startup_ON                45      // uint8_t
#define Channel_6_OFF_on_loss               46      // uint8_t
#define Channel_6_Loss_timeout_seconds      48      // uint16_t
#define Channel_6_Pulse_milliseconds        50      // uint16_t

// Parameter format
#define ParamParameter_format                    (knx.paramByte(Parameter_format))
// Global flags
#define ParamGlobal_flags                        (knx.paramByte(Global_flags))
// Channel 1 Enabled
#define ParamChannel_1_Enabled                   (knx.paramByte(Channel_1_Enabled))
// Channel 1 Startup ON
#define ParamChannel_1_Startup_ON                (knx.paramByte(Channel_1_Startup_ON))
// Channel 1 OFF on loss
#define ParamChannel_1_OFF_on_loss               (knx.paramByte(Channel_1_OFF_on_loss))
// Channel 1 Loss timeout seconds
#define ParamChannel_1_Loss_timeout_seconds      (knx.paramWord(Channel_1_Loss_timeout_seconds))
// Channel 1 Pulse milliseconds
#define ParamChannel_1_Pulse_milliseconds        (knx.paramWord(Channel_1_Pulse_milliseconds))
// Channel 2 Enabled
#define ParamChannel_2_Enabled                   (knx.paramByte(Channel_2_Enabled))
// Channel 2 Startup ON
#define ParamChannel_2_Startup_ON                (knx.paramByte(Channel_2_Startup_ON))
// Channel 2 OFF on loss
#define ParamChannel_2_OFF_on_loss               (knx.paramByte(Channel_2_OFF_on_loss))
// Channel 2 Loss timeout seconds
#define ParamChannel_2_Loss_timeout_seconds      (knx.paramWord(Channel_2_Loss_timeout_seconds))
// Channel 2 Pulse milliseconds
#define ParamChannel_2_Pulse_milliseconds        (knx.paramWord(Channel_2_Pulse_milliseconds))
// Channel 3 Enabled
#define ParamChannel_3_Enabled                   (knx.paramByte(Channel_3_Enabled))
// Channel 3 Startup ON
#define ParamChannel_3_Startup_ON                (knx.paramByte(Channel_3_Startup_ON))
// Channel 3 OFF on loss
#define ParamChannel_3_OFF_on_loss               (knx.paramByte(Channel_3_OFF_on_loss))
// Channel 3 Loss timeout seconds
#define ParamChannel_3_Loss_timeout_seconds      (knx.paramWord(Channel_3_Loss_timeout_seconds))
// Channel 3 Pulse milliseconds
#define ParamChannel_3_Pulse_milliseconds        (knx.paramWord(Channel_3_Pulse_milliseconds))
// Channel 4 Enabled
#define ParamChannel_4_Enabled                   (knx.paramByte(Channel_4_Enabled))
// Channel 4 Startup ON
#define ParamChannel_4_Startup_ON                (knx.paramByte(Channel_4_Startup_ON))
// Channel 4 OFF on loss
#define ParamChannel_4_OFF_on_loss               (knx.paramByte(Channel_4_OFF_on_loss))
// Channel 4 Loss timeout seconds
#define ParamChannel_4_Loss_timeout_seconds      (knx.paramWord(Channel_4_Loss_timeout_seconds))
// Channel 4 Pulse milliseconds
#define ParamChannel_4_Pulse_milliseconds        (knx.paramWord(Channel_4_Pulse_milliseconds))
// Channel 5 Enabled
#define ParamChannel_5_Enabled                   (knx.paramByte(Channel_5_Enabled))
// Channel 5 Startup ON
#define ParamChannel_5_Startup_ON                (knx.paramByte(Channel_5_Startup_ON))
// Channel 5 OFF on loss
#define ParamChannel_5_OFF_on_loss               (knx.paramByte(Channel_5_OFF_on_loss))
// Channel 5 Loss timeout seconds
#define ParamChannel_5_Loss_timeout_seconds      (knx.paramWord(Channel_5_Loss_timeout_seconds))
// Channel 5 Pulse milliseconds
#define ParamChannel_5_Pulse_milliseconds        (knx.paramWord(Channel_5_Pulse_milliseconds))
// Channel 6 Enabled
#define ParamChannel_6_Enabled                   (knx.paramByte(Channel_6_Enabled))
// Channel 6 Startup ON
#define ParamChannel_6_Startup_ON                (knx.paramByte(Channel_6_Startup_ON))
// Channel 6 OFF on loss
#define ParamChannel_6_OFF_on_loss               (knx.paramByte(Channel_6_OFF_on_loss))
// Channel 6 Loss timeout seconds
#define ParamChannel_6_Loss_timeout_seconds      (knx.paramWord(Channel_6_Loss_timeout_seconds))
// Channel 6 Pulse milliseconds
#define ParamChannel_6_Pulse_milliseconds        (knx.paramWord(Channel_6_Pulse_milliseconds))

// Communication objects with single occurrence
#define KoChannel_1_Switch 1
#define KoChannel_1_Block 2
#define KoChannel_1_Status 3
#define KoChannel_2_Switch 4
#define KoChannel_2_Block 5
#define KoChannel_2_Status 6
#define KoChannel_3_Switch 7
#define KoChannel_3_Block 8
#define KoChannel_3_Status 9
#define KoChannel_4_Switch 10
#define KoChannel_4_Block 11
#define KoChannel_4_Status 12
#define KoChannel_5_Switch 13
#define KoChannel_5_Block 14
#define KoChannel_5_Status 15
#define KoChannel_6_Switch 16
#define KoChannel_6_Block 17
#define KoChannel_6_Status 18

// Channel 1 Switch
#define KoChannel_1_Switch                    (knx.getGroupObject(KoChannel_1_Switch))
// Channel 1 Block
#define KoChannel_1_Block                     (knx.getGroupObject(KoChannel_1_Block))
// Channel 1 Status
#define KoChannel_1_Status                    (knx.getGroupObject(KoChannel_1_Status))
// Channel 2 Switch
#define KoChannel_2_Switch                    (knx.getGroupObject(KoChannel_2_Switch))
// Channel 2 Block
#define KoChannel_2_Block                     (knx.getGroupObject(KoChannel_2_Block))
// Channel 2 Status
#define KoChannel_2_Status                    (knx.getGroupObject(KoChannel_2_Status))
// Channel 3 Switch
#define KoChannel_3_Switch                    (knx.getGroupObject(KoChannel_3_Switch))
// Channel 3 Block
#define KoChannel_3_Block                     (knx.getGroupObject(KoChannel_3_Block))
// Channel 3 Status
#define KoChannel_3_Status                    (knx.getGroupObject(KoChannel_3_Status))
// Channel 4 Switch
#define KoChannel_4_Switch                    (knx.getGroupObject(KoChannel_4_Switch))
// Channel 4 Block
#define KoChannel_4_Block                     (knx.getGroupObject(KoChannel_4_Block))
// Channel 4 Status
#define KoChannel_4_Status                    (knx.getGroupObject(KoChannel_4_Status))
// Channel 5 Switch
#define KoChannel_5_Switch                    (knx.getGroupObject(KoChannel_5_Switch))
// Channel 5 Block
#define KoChannel_5_Block                     (knx.getGroupObject(KoChannel_5_Block))
// Channel 5 Status
#define KoChannel_5_Status                    (knx.getGroupObject(KoChannel_5_Status))
// Channel 6 Switch
#define KoChannel_6_Switch                    (knx.getGroupObject(KoChannel_6_Switch))
// Channel 6 Block
#define KoChannel_6_Block                     (knx.getGroupObject(KoChannel_6_Block))
// Channel 6 Status
#define KoChannel_6_Status                    (knx.getGroupObject(KoChannel_6_Status))


// enumeration types


#ifdef MAIN_FirmwareRevision
#ifndef FIRMWARE_REVISION
#define FIRMWARE_REVISION MAIN_FirmwareRevision
#endif
#endif
#ifdef MAIN_FirmwareName
#ifndef FIRMWARE_NAME
#define FIRMWARE_NAME MAIN_FirmwareName
#endif
#endif
