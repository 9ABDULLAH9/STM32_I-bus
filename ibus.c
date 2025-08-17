/**
 * @file ibus.c
 * @author MertRonom9
 * @brief Source file for the iBus protocol
 * @version 0.1
 * @date 2025-08-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ibus.h"
#include "stm32f4xx_hal.h" /* May vary depending on MCU family */

static volatile uint8_t s_rx_buf[IBUS_FRAME_LEN];
static volatile IBusChannels_t s_channels;
static volatile uint8_t s_fresh = 0;

/**
 * @brief Starts initial reception
 */
static inline void prv_start_rx(void)
{
    /* 32-byte fixed-length interrupt-driven reception */
    (void)HAL_UART_Receive_IT(IBUS_UART, (uint8_t*)s_rx_buf, IBUS_FRAME_LEN);
}

/**
 * @brief Reads a channel from the frame buffer
 *
 * @param b   Frame data
 * @param ch_idx Channel index
 * @return uint16_t Raw channel value
 */
static inline uint16_t prv_read_ch(const uint8_t *b, uint8_t ch_idx)
{
    /* b[2 + 2*idx] low, b[3 + 2*idx] high */
    const uint8_t lo = b[2u + (2u * ch_idx)];
    const uint8_t hi = b[3u + (2u * ch_idx)];
    return (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;
}

/**
 * @brief Verifies the iBus checksum (CRC)
 *
 * @param b   Frame data
 * @return true  If checksum matches
 * @return false Otherwise
 */
static bool prv_crc_ok(const uint8_t *b)
{
    /* iBus: checksum = 0xFFFF - sum(bytes[0..29]); stored little-endian at [30..31] */
    uint16_t calc = 0xFFFFu;
    for (uint8_t i = 0; i < (IBUS_FRAME_LEN - 2u); i++)
    {
        calc = (uint16_t)(calc - b[i]);
    }
    const uint16_t rx = (uint16_t)((uint16_t)b[31] << 8) | (uint16_t)b[30];
    return (calc == rx);
}

/**
 * @brief Verifies the iBus header
 *
 * @param b   Frame data
 * @return true  If header is valid
 * @return false Otherwise
 */
static bool prv_header_ok(const uint8_t *b)
{
    return (b[0] == IBUS_FRAME_LEN) && (b[1] == IBUS_CMD_SERVO);
}

/* If parsing succeeds, updates the global structure (ISR context) */
/**
 * @brief Parses and updates frame data
 *
 * @param b   Frame data
 */
static void prv_parse_and_update(const uint8_t *b)
{
    IBusChannels_t tmp = s_channels; /* Work on a copy to preserve old data if CRC fails */

    /* Channels: CH1..CH10 -> map */
    uint16_t ch[IBUS_NUM_CHANNELS];
    for (uint8_t i = 0; i < IBUS_NUM_CHANNELS; i++)
    {
        ch[i] = prv_read_ch(b, i);
    }

    /* User mapping */
    tmp.roll      = ch[IBUS_MAP_ROLL];
    tmp.pitch     = ch[IBUS_MAP_PITCH];
    tmp.yaw       = ch[IBUS_MAP_YAW];
    tmp.throttle  = ch[IBUS_MAP_THROTTLE];
    tmp.switch1   = ch[IBUS_MAP_SW1];
    tmp.switch2   = ch[IBUS_MAP_SW2];
    tmp.switch3   = ch[IBUS_MAP_SW3];
    tmp.switch4   = ch[IBUS_MAP_SW4];
    tmp.switch5   = ch[IBUS_MAP_SW5];
    tmp.switch6   = ch[IBUS_MAP_SW6];

    tmp.last_update_ms = HAL_GetTick();
    tmp.frame_ok       = true;

    /* Atomic assignment (structure copy) */
    s_channels = tmp;
    s_fresh = 1u;
}

/* ---- Public API ---- */

/**
 * @brief Initializes iBus reception
 */
void IBUS_Init(void)
{
    /* Default safe initialization values */
    IBusChannels_t init = {
        .roll = 1500, .pitch = 1500, .yaw = 1500, .throttle = 1000,
        .switch1 = 1000, .switch2 = 1000, .switch3 = 1000,
        .switch4 = 1000, .switch5 = 1000, .switch6 = 1000,
        .last_update_ms = 0, .frame_ok = false
    };
    s_channels = init;
    s_fresh = 0;

    prv_start_rx();
}

/**
 * @brief Gets the current iBus state
 *
 * @param out Output structure
 */
void IBUS_GetSnapshot(IBusChannels_t *out)
{
    __disable_irq();
    *out = s_channels;
    s_fresh = 0u;            /* mark as read */
    __enable_irq();
}

/**
 * @brief Returns a READ-ONLY pointer to the current iBus state
 *
 * @return const IBusChannels_t*
 */
const IBusChannels_t* IBUS_Peek(void)
{
    return (const IBusChannels_t*)&s_channels;
}

/**
 * @brief Indicates whether a new valid frame has arrived since the last read
 *
 * @return bool  true if fresh, false otherwise (reading resets it)
 */
bool IBUS_TakeFreshFlag(void)
{
    bool had = (s_fresh != 0u);
    s_fresh = 0u;
    return had;
}

/* If you have other UART callbacks, call this from your own RxCplt */
/**
 * @brief Called when a new iBus frame is received
 *
 * @param huart UART handle
 */
void IBUS_OnRxCplt(UART_HandleTypeDef *huart)
{
    if (huart == IBUS_UART)
    {
        const uint8_t *b = (const uint8_t*)s_rx_buf;

        /* Header + CRC check; do NOT update if it fails */
        if (prv_header_ok(b) && prv_crc_ok(b))
        {
            prv_parse_and_update(b);
        }
        else
        {
            /* invalid frame: keep s_channels as is */
        }

        /* Restart for the next frame */
        prv_start_rx();
    }
}

#if IBUS_OVERRIDE_HAL_CALLBACK
/**
 * @brief HAL callback invoked when UART Rx completes
 *
 * @note This function is provided by the HAL library.
 *
 * @param huart UART handle
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    IBUS_OnRxCplt(huart);
}
#endif
