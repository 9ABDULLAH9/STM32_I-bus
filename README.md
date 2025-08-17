

# STM32 iBus Receiver (FlySky i-Bus) — Interrupt-Driven Minimal Library

Small, ISR-safe iBus parser for STM32 HAL.  
Reads 10 channels (CH1..CH10) and maps them to: `roll, pitch, yaw, throttle, switch1..switch6`.  
CRC is checked; if invalid, the public state is **not** updated.

---
## Features

- Fixed-length 32-byte frame parse (interrupt-driven).
- Header and checksum verification (iBus checksum = `0xFFFF - sum(bytes[0..29])`).
- Atomic snapshot API for safe access from `main()`.
- Pluggable callback strategy (use library’s HAL callback or your own).
- Simple channel mapping via macros (`IBUS_MAP_*`).

---
## Hardware & UART

- **Signal:** FlySky i-Bus (non-inverted TTL).
- **Baud:** Typically **115200**. Parity varies by receiver (commonly **8E1** or **8N1**) — check your RX datasheet.
- **Wiring:** Receiver i-Bus pin → STM32 **UART RX** (3.3 V). **Do not** connect a 5 V signal directly.

⚠️ **Warning:**  
    The library `ibus.c` currently includes a device-specific header (`stm32f4xx.h`).   If you are using another STM32 family (e.g., F1, F3, H7, L4...), you must adjust the include to match your device header (`stm32f1xx.h`, `stm32h7xx.h`, etc.).  
This is the only place you need to change—no other library code modifications are required.

> The library uses a single UART in interrupt mode to receive an entire 32-byte iBus frame in one shot.

---
## Getting Started

### 1) Add Files

Copy into your project:
- `Core/Inc/ibus.h`
- `Core/Src/ibus.c`

### 2) CubeMX / HAL Setup

Enable a UART (e.g., **USART1**) with:
- Baud: **115200**
- Word length/parity: match your receiver (e.g., 8E1 or 8N1)
- Mode: **RX** (or **RX/TX** if shared)

Generate code so you have `MX_USARTx_UART_Init()` and a `huartx` handle (e.g., `huart1`).

---
### 3) Configure Which UART iBus Uses

By default the library uses `huart1`. To change, edit or define in `ibus.h`:

```c
#ifndef IBUS_UART
#define IBUS_UART  (&huart1)
#endif
```

4) Choose Your Callback Strategy

Option A — Use library’s HAL callback (default)

```c
#ifndef IBUS_OVERRIDE_HAL_CALLBACK
#define IBUS_OVERRIDE_HAL_CALLBACK  1
#endif
```

This enables the library’s HAL callback:

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    IBUS_OnRxCplt(huart);
}
```

So new frames are parsed automatically.


---

**Option B — Use your own global callback**

Disable override:

```c
#define IBUS_OVERRIDE_HAL_CALLBACK  0
```

Call from your global callback:
```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // ... your other UART handling ...
    IBUS_OnRxCplt(huart); // hand off to iBus
}
```
---

## main.c Integration (STM32CubeIDE layout)

The snippets below show exactly **where** to place each piece inside the standard CubeIDE “USER CODE” blocks. Adjust `USARTx` and your UART handle as needed.

### 1) Includes  
Add inside `/* USER CODE BEGIN Includes */`:

```c
#include "ibus.h"
```

### 2) USER CODE BEGIN 0 (optional helpers / globals)  
Place any helpers here (optional, but convenient):

```c
/* USER CODE BEGIN 0 */

// Optional normalization helpers
static inline float ibus_norm_stick(uint16_t raw) {
    float y = ((float)raw - 1500.0f) / 500.0f;
    if (y < -1.0f) y = -1.0f;
    if (y > +1.0f) y = +1.0f;
    return y;
}

static inline float ibus_norm_throttle(uint16_t raw) {
    float y = ((float)raw - 1000.0f) / 1000.0f;
    if (y < 0.0f) y = 0.0f;
    if (y > 1.0f) y = 1.0f;
    return y;
}

/* USER CODE END 0 */
```

### 3) USER CODE BEGIN 2 (start iBus reception)  
Call **after** `MX_USARTx_UART_Init()` is done:

```c
/* USER CODE BEGIN 2 */
IBUS_Init();  // starts interrupt-driven reception of the fixed 32-byte iBus frame
/* USER CODE END 2 */
```

### 4) while(1) (consume fresh frames)  
Process new, validated frames using a “fresh flag”/snapshot pattern:

```c
while (1)
{
  /* USER CODE BEGIN WHILE */

  if (IBUS_TakeFreshFlag())  // true once per newly received valid frame
  {
      IBusChannels_t ch;
      IBUS_GetSnapshot(&ch); // atomic copy of latest channels

      // Age/staleness check (tune threshold for your loop rate)
      uint32_t age = HAL_GetTick() - ch.last_update_ms;
      uint8_t stale = (age > 50U);

      if (!stale && ch.frame_ok) {
          // Normalize
          float thr01 = ibus_norm_throttle(ch.throttle);
          float r = ibus_norm_stick(ch.roll);
          float p = ibus_norm_stick(ch.pitch);
          float y = ibus_norm_stick(ch.yaw);

          // Switch example
          uint8_t sw1_on = (ch.switch1 > 1500U);

          // TODO: use r/p/y/thr01/sw1_on in your control logic
      } else {
          // TODO: failsafe behavior (hold/zero commands, etc.)
      }
  }

  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  // Optional: periodic polling if you prefer (not required)
  /* USER CODE END 3 */
}

```

## Channel Mapping

These macros define how iBus channel numbers (CH1..CH10) are mapped onto logical names.  
If your transmitter uses a different stick order (e.g., TAER instead of AETR), you can **simply change the definitions here** instead of modifying the rest of the library.

```c
#define IBUS_MAP_ROLL       0u
#define IBUS_MAP_PITCH      1u
#define IBUS_MAP_THROTTLE   2u
#define IBUS_MAP_YAW        3u
#define IBUS_MAP_SW1        4u
#define IBUS_MAP_SW2        5u
#define IBUS_MAP_SW3        6u
#define IBUS_MAP_SW4        7u
#define IBUS_MAP_SW5        8u
#define IBUS_MAP_SW6        9u
```
By default: CH1=Roll, CH2=Pitch, CH3=Throttle, CH4=Yaw, CH5–CH10=Switches

**⚠️ Channel Mapping Warning**
By default, the library assumes the AETR channel order:

CH1 = Roll

CH2 = Pitch

CH3 = Throttle

CH4 = Yaw

CH5–CH10 = Switches

If your transmitter uses a different stick order (e.g., TAER, RETA, etc.), you must update the IBUS_MAP_* macros in ibus.h accordingly.
Otherwise, controls will not match your sticks and may cause unsafe behavior.

This gives you a single point of configuration for channel order, keeping your application code unchanged.

## Data & Scaling

- Raw iBus units are typically 1000..2000.

- Normalize examples:

    - Sticks: (val - 1500) / 500 → ~[-1..+1]

    - Throttle: (val - 1000) / 1000 → [0..1]

- Switches: ~1000 (OFF) / ~2000 (ON) — verify on your transmitter.

---

## Stale / Failsafe Handling

```c
uint32_t age = HAL_GetTick() - ibus.last_update_ms;
uint8_t is_stale = (age > 50); // choose threshold for your control loop
```

If stale, apply your failsafe (zero commands, hold last, etc.).

### API Reference

- void IBUS_Init(void);

- void IBUS_GetSnapshot(IBusChannels_t *out);

- const IBusChannels_t* IBUS_Peek(void);

- bool IBUS_TakeFreshFlag(void);

-  void IBUS_OnRxCplt(UART_HandleTypeDef *huart);


## Troubleshooting

- No updates: Check UART wiring/baud/parity and correct IBUS_UART.

- Fresh flag never set: Parity mismatch or header/CRC failing (frame not accepted).

- Callback conflicts: Disable override and call IBUS_OnRxCplt() manually from your global callback.

## Installation
Clone the repository and install the required dependencies:
```bash
git clone https://github.com/9ABDULLAH9/STM32_I-bus.git
```

## Contributing
To contribute, fork the repository, create a new branch, commit your changes, and open a pull request.

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.
