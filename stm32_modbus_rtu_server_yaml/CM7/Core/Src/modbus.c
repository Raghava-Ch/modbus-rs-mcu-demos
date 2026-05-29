#include "mbus_server_app.h"
#include <stddef.h>
#include <stdint.h>
#include "modbus_rs_server.h"
#include "main.h"
#include "stm32h7xx_hal_uart.h"
#include "stm32h7xx_nucleo.h"
/* Ensure HAL status type is defined */

extern UART_HandleTypeDef huart3;

/* Circular buffer for background reception */
#define MODBUS_RX_BUF_SIZE 512
static uint8_t rx_ring_buf[MODBUS_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static uint8_t rx_byte_tmp; // Temporary buffer for the 1-byte HAL receive

/** State struct for Non-blocking LED toggles */
typedef struct {
    uint32_t delay_ms;
    uint32_t last_toggle;
} LED_State;
static LED_State green_led = {0, 0};
static LED_State red_led = {0, 0};
static LED_State yellow_led = {0, 0};

static MbusServerId g_server_id = MBUS_INVALID_SERVER_ID;

static enum MbusStatusCode st_connect(void *userdata) {
    return MbusOk;
}
static enum MbusStatusCode st_disconnect(void *userdata) {
    return MbusOk;
}
static enum MbusStatusCode st_send(const uint8_t *data, uint16_t len, void *userdata) {
    UART_HandleTypeDef *huart = &huart3;

    /* Non-blocking check for busy UART hardware */
    if (huart->gState != HAL_UART_STATE_READY) {
        return MbusErrSendFailed;
    }

    if (HAL_UART_Transmit_IT(huart, (uint8_t *)data, len) != HAL_OK) {
        return MbusErrInvalidDataLen;
    }
    return MbusOk;
}

static enum MbusStatusCode st_recv(uint8_t *buf, uint16_t cap, uint16_t *out_len,
                                   void *userdata) {
    uint16_t count = 0;

    /* Pull available bytes from the ring buffer */
    while (rx_head != rx_tail && count < cap) {
        buf[count++] = rx_ring_buf[rx_tail];
        rx_tail = (rx_tail + 1) % MODBUS_RX_BUF_SIZE;
    }

    *out_len = count;
    return MbusOk;
}

int rx_bytes = 0;
/* This callback is called by the HAL ISR when 1 byte is received */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == huart3.Instance) {
        uint16_t next_head = (rx_head + 1) % MODBUS_RX_BUF_SIZE;

        /* Push to buffer if not full */
        if (next_head != rx_tail) {
            rx_ring_buf[rx_head] = rx_byte_tmp;
            rx_head = next_head;
        }

        /* Re-trigger receive for the next byte */
        HAL_UART_Receive_IT(huart, &rx_byte_tmp, 1);
    }
}

/**
 * @brief  UART error callback
 *         This is called by the HAL when a UART error occurs (like Overrun, Framing, or Noise).
 *         When an Overrun Error (ORE) occurs, the HAL automatically aborts the active reception.
 *         We must catch this and restart the reception process to prevent the UART from hanging forever.
 * @param  huart Pointer to the UART handle
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == huart3.Instance) {
        /* The HAL has cleared the error flags and aborted the RX process.
           We just need to re-trigger the interrupt-based reception so it
           continues to receive the next incoming bytes. */
        HAL_UART_Receive_IT(huart, &rx_byte_tmp, 1);
    }
}

static uint8_t st_is_connected(void *userdata) {
    UART_HandleTypeDef *huart = &huart3 ;
    if (huart->gState == HAL_UART_STATE_READY) {
        return 1; // Connected
    }
    return 0; // Disconnected
}


struct MbusTransportCallbacks tr = {
    .userdata        = NULL,
    .on_connect      = st_connect,
    .on_disconnect   = st_disconnect,
    .on_send         = st_send,
    .on_recv         = st_recv,
    .on_is_connected = st_is_connected,
};

/**
 * @brief Initialize Modbus model, create server instance and start background serial receive
 * @retval 0 on success, -1 on failure
 */
int modbus_init(void)
{
    /* Initialize the Modbus server model and handlers */
    mbus_server_model_init();
    struct MbusServerHandlers handlers = mbus_server_default_handlers(NULL);

    /* Create a serial Modbus server with the default handlers */
    struct MbusServerConfig cfg = { .slave_address = 1u, .response_timeout_ms = 1000u };
//    g_server_id = mbus_serial_rtu_server_new(&tr, &handlers, &cfg);
//    if (g_server_id == MBUS_INVALID_SERVER_ID) {
//        // Handle error
//        return -1;
//    }

    /* Initialize the Nucleo User Button in standard GPIO polling mode */
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);

    /* Abort any background reception the BSP might have silently left pending */
    HAL_UART_AbortReceive(&huart3);

    /* Start the background receive chain. Check return to ensure it actually starts. */
    if (HAL_UART_Receive_IT(&huart3, &rx_byte_tmp, 1) != HAL_OK) {
        return -1;
    }

    /* Connect the server (open transport) */
//    if (mbus_serial_server_connect(g_server_id) != MbusOk) {
//        // Handle error
//        mbus_serial_server_free(g_server_id);
//        g_server_id = MBUS_INVALID_SERVER_ID;
//        return -1;
//    }

    return 0;
}

/**
 * @brief Modbus super-loop task. To be called periodically from main while(1).
 */
void modbus_task(void)
{
    if (g_server_id == MBUS_INVALID_SERVER_ID) {
        return;
    }

    /* Drive the server state machine */
    if (mbus_serial_server_poll(g_server_id) != MbusOk) {
        // Handle disconnection or fatal error
    }

    /* Get current tick count from HAL */
    uint32_t current_tick = HAL_GetTick();

    /*
     * Update User Button state.
     * BSP_PB_GetState returns 1 when pressed and 0 when released.
     * We push this state into the Modbus model so the Master can read it via Discrete Input 0x0000.
     */
    uint8_t btn_state = (BSP_PB_GetState(BUTTON_USER) != 0) ? 1 : 0;
    mbus_server_set_button_user(btn_state);

    /* Non-blocking toggle for GREEN LED */
    if (green_led.delay_ms > 0) {
        if ((current_tick - green_led.last_toggle) >= green_led.delay_ms) {
            BSP_LED_Toggle(LED_GREEN);
            uint8_t state = BSP_LED_GetState(LED_GREEN);
            mbus_server_set_led_green(state);
            green_led.last_toggle = current_tick;
        }
    }

    /* Non-blocking toggle for RED LED */
    if (red_led.delay_ms > 0) {
        if ((current_tick - red_led.last_toggle) >= red_led.delay_ms) {
            BSP_LED_Toggle(LED_RED);
            uint8_t state = BSP_LED_GetState(LED_RED);
            mbus_server_set_led_red(state);
            red_led.last_toggle = current_tick;
        }
    }

}

MbusHookStatus app_on_write_led_green(void* ctx, uint16_t address, uint8_t value) {
    if (value) {
        BSP_LED_On(LED_GREEN);
    } else {
        BSP_LED_Off(LED_GREEN);
    }
    return MBUS_HOOK_OK;
}

MbusHookStatus app_on_write_led_red(void* ctx, uint16_t address, uint8_t value) {
    if (value) {
        BSP_LED_On(LED_RED);
    } else {
        BSP_LED_Off(LED_RED);
    }
    return MBUS_HOOK_OK;
}

MbusHookStatus app_on_write_led_yellow(void* ctx, uint16_t address, uint8_t value) {
    if (value) {
        BSP_LED_On(LED_YELLOW);
    } else {
        BSP_LED_Off(LED_YELLOW);
    }
    return MBUS_HOOK_OK;
}

MbusHookStatus app_on_write_blink_delay(void* ctx, uint16_t address, uint16_t value) {
    green_led.delay_ms = value;
    red_led.delay_ms = value;
    yellow_led.delay_ms = value;
    return MBUS_HOOK_OK;
}

void mbus_app_lock(void) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}
void mbus_app_unlock(void) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}

void mbus_pool_unlock(void) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}

void mbus_pool_lock(void) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}

void mbus_server_unlock(MbusServerId id) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}

void mbus_server_lock(MbusServerId id) {
    // Not mandatory to implement for single-threaded environments,
    // but we can disable interrupts to create critical sections if needed.
}
