/**
 * @file ibus.h
 * @author MertRonom9
 * @brief Header file for the iBus protocol
 * @version 0.1
 * @date 2025-08-17
 *
 * @copyright Copyright (c) 2025
 */

#ifndef __IBUS_H__
#define __IBUS_H__

#include "usart.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- User configuration ----
 * IBUS_UART: UART handle to which iBus is connected.
 * Default: huart1. Change according to your project.
 */
#ifndef IBUS_UART
#define IBUS_UART                (&huart1)
#endif

/* iBus constants */
#define IBUS_FRAME_LEN           32u           /* fixed length */
#define IBUS_CMD_SERVO           0x40u         /* channel data command */
#define IBUS_NUM_CHANNELS        10u           /* we only use 10 channels */

/* Channel -> function mapping (change if necessary) */
/* CH1..CH10 -> roll, pitch, throttle, yaw, sw1..sw6 */
#define IBUS_MAP_ROLL            0u
#define IBUS_MAP_PITCH           1u
#define IBUS_MAP_THROTTLE        2u
#define IBUS_MAP_YAW             3u
#define IBUS_MAP_SW1             4u
#define IBUS_MAP_SW2             5u
#define IBUS_MAP_SW3             6u
#define IBUS_MAP_SW4             7u
#define IBUS_MAP_SW5             8u
#define IBUS_MAP_SW6             9u

/* Should the library provide its own HAL_UART_RxCpltCallback?
 * If you are using the same callback for other UARTs in your project,
 * set this to 0 and call IBUS_OnRxCplt(huart) from your own callback.
 */
#ifndef IBUS_OVERRIDE_HAL_CALLBACK
#define IBUS_OVERRIDE_HAL_CALLBACK  1
#endif

/* ---- Channel structure ----
 * Raw iBus units are stored (typically in the 1000..2000us range).
 * If CRC fails, values are not updated for that frame.
 */
typedef struct
{
    uint16_t roll;
    uint16_t pitch;
    uint16_t throttle;
    uint16_t yaw;
    uint16_t switch1;
    uint16_t switch2;
    uint16_t switch3;
    uint16_t switch4;
    uint16_t switch5;
    uint16_t switch6;

    uint32_t last_update_ms;   /* HAL_GetTick() */
    bool     frame_ok;         /* was the last frame valid */
} IBusChannels_t;

/* ---- API ---- */

/* Puts UART into interrupt mode for 32-byte iBus reception. */
void IBUS_Init(void);

/* Copies the current structure in a lock-safe way (ISR-atomic). Resets the fresh flag. */
void IBUS_GetSnapshot(IBusChannels_t *out);

/* If you only want to peek: returns a READ-ONLY pointer to the ISR-updated structure. */
const IBusChannels_t* IBUS_Peek(void);

/* Has a new and valid frame arrived since the last read?
 * (reset when read)
 */
bool IBUS_TakeFreshFlag(void);

/* If you have your own global RxCplt callback, call this from there. */
void IBUS_OnRxCplt(UART_HandleTypeDef *huart);

#endif /* __IBUS_H__ */
