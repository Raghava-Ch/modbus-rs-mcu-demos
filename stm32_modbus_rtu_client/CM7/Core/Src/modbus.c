/**
  ******************************************************************************
  * @file    modbus.c
  * @brief   Modbus client setup, callbacks and polling routines
  ******************************************************************************
  */

#include "modbus.h"
#include "modbus_rs.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>

/* External reference to the UART handle used for Modbus RTU */
extern UART_HandleTypeDef huart3;

/* Global identifier for our Modbus client instance. Kept private to this file. */
static MbusClientId modbus_client_id = MBUS_INVALID_CLIENT_ID;

/* Ring buffer for interrupt-based UART reception */
#define RX_BUF_SIZE 512
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

/* Temporary 1-byte buffer for HAL_UART_Receive_IT */
static uint8_t rx_byte;

/* ========================================================================== */
/*                            TRANSPORT CALLBACKS                             */
/* ========================================================================== */

/**
  * @brief Modbus transport layer connection callback
  */
static enum MbusStatusCode app_transport_connect(void *userdata) {
    printf("[Transport] Connected.\n");
    /* Start listening for incoming bytes via interrupt */
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    return MbusOk;
}

/**
  * @brief Modbus transport layer disconnection callback
  */
static enum MbusStatusCode app_transport_disconnect(void *userdata) {
    printf("[Transport] Disconnected.\n");
    /* Stop the interrupt reception */
    HAL_UART_AbortReceive_IT(&huart3);
    return MbusOk;
}

/**
  * @brief Modbus transport layer send callback (Blocking transmit)
  */
static enum MbusStatusCode app_transport_send(const uint8_t *data, uint16_t len, void *userdata) {
    if (HAL_UART_Transmit(&huart3, (uint8_t *)data, len, HAL_MAX_DELAY) == HAL_OK) {
        return MbusOk;
    }
    return MbusErrSendFailed;
}

/**
  * @brief Modbus transport layer receive callback (Non-blocking receive)
  *        Delivers available bytes from the interrupt ring buffer to the stack.
  */
static enum MbusStatusCode app_transport_recv(uint8_t *buffer, uint16_t buffer_cap, uint16_t *out_len, void *userdata) {
    uint16_t count = 0;
    
    /* Pull as many bytes as we can from the ring buffer, up to buffer_cap */
    while (rx_tail != rx_head && count < buffer_cap) {
        buffer[count++] = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    }

    if (count > 0) {
        *out_len = count;
        return MbusOk;
    }

    *out_len = 0;
    return MbusErrTimeout; 
}

/**
  * @brief Modbus transport layer status callback
  */
static uint8_t app_transport_is_connected(void *userdata) {
    return 1;
}

/* ========================================================================== */
/*                           APPLICATION CALLBACKS                            */
/* ========================================================================== */

/**
  * @brief Modbus application timing callback
  */
static uint64_t app_current_millis(void *userdata) {
    return HAL_GetTick();
}

/**
  * @brief Modbus application Read Coils response callback
  */
static void app_on_read_coils(const struct MbusReadCoilsCtx *ctx) {
    printf("[App] Read Coils Response received (txn_id: %u, unit_id: %u)\n", ctx->txn_id, ctx->unit_id);
    
    uint16_t qty = mbus_coils_quantity(ctx->coils);
    for (uint16_t i = 0; i < qty; ++i) {
        bool val = false;
        if (mbus_coils_value_at_index(ctx->coils, i, &val) == MbusOk) {
            bool value = val ? 1 : 0;
        }
    }
}

/**
  * @brief Modbus application Write Multiple Coils response callback
  */
static void app_on_write_multiple_coils(const struct MbusWriteMultipleCoilsCtx *ctx) {
    printf("[App] Write Multiple Coils Response received (txn_id: %u, unit_id: %u)\n", ctx->txn_id, ctx->unit_id);
    printf("      Successfully wrote %u coils starting at address %u.\n", ctx->quantity, ctx->address);
}

/**
  * @brief Modbus application Write Multiple Registers response callback
  *
  * @param ctx Context containing details about the write operation
  */
static void app_on_write_multiple_registers(const struct MbusWriteMultipleRegistersCtx *ctx) {
    printf("[App] Write Multiple Registers Response received (txn_id: %u, unit_id: %u)\n", ctx->txn_id, ctx->unit_id);
    printf("      Successfully wrote %u registers starting at address %u.\n", ctx->quantity, ctx->address);
}

/**
  * @brief Modbus application Request Failed callback
  */
static void app_on_request_failed(const struct MbusRequestFailedCtx *ctx) {
    printf("[App] Request Failed (txn_id: %u, unit_id: %u, error: %s)\n", 
           ctx->txn_id, ctx->unit_id, mbus_status_str(ctx->error));
}

/* ========================================================================== */
/*                               PUBLIC METHODS                               */
/* ========================================================================== */

void Modbus_Init(void) {
    struct MbusSerialConfig config = {
        .port_name = "UART3", 
        .baud_rate = 115200,
        .mode = MbusSerialRtu,
        .response_timeout_ms = 1000,
        .retries = 3,
        .backoff_strategy = MbusBackoffFixed,
        .backoff_base_delay_ms = 100,
        .backoff_max_delay_ms = 1000,
        .jitter_percent = 0
    };

    struct MbusTransportCallbacks transport = {
        .on_connect = app_transport_connect,
        .on_disconnect = app_transport_disconnect,
        .on_send = app_transport_send,
        .on_recv = app_transport_recv,
        .on_is_connected = app_transport_is_connected
    };

    struct MbusCallbacks callbacks = {
        .on_current_millis = app_current_millis,
        .on_read_coils = app_on_read_coils,
        .on_write_multiple_coils = app_on_write_multiple_coils,
        .on_write_multiple_registers = app_on_write_multiple_registers,
        .on_request_failed = app_on_request_failed
    };

    modbus_client_id = mbus_serial_client_new(&config, &transport, &callbacks);
    if (modbus_client_id != MBUS_INVALID_CLIENT_ID) {
        if (mbus_serial_connect(modbus_client_id) == MbusOk) {
            /* Connection successful. Requests will be queued periodically in Modbus_Poll(). */
        }
    }
}

void Modbus_Poll(void) {
    static uint32_t last_request_time = 0;
    static uint8_t app_state = 0;

    if (modbus_client_id != MBUS_INVALID_CLIENT_ID) {
        /* 1. Drive the Modbus state machine. Must be called continuously. */
        mbus_serial_poll(modbus_client_id);

        /* 2. Application Sequencer: Queue new requests periodically if idle.
         *    Wait 2000ms between requests.
         */
        if (!mbus_serial_has_pending_requests(modbus_client_id)) {
            if (HAL_GetTick() - last_request_time >= 1000) {
                last_request_time = HAL_GetTick();

                if (app_state == 0) {
                    /* State 0: Queue a Write Multiple Coils request (Pattern 1) */
                    uint8_t packed_coils = 0x0D; /* Binary: 0000 1101 */
                    mbus_serial_write_multiple_coils(modbus_client_id, 1, 1, 0, &packed_coils, 8);
                    app_state = 1;
                } else if (app_state == 1) {
                    /* State 1: Queue a Read Coils request */
                    mbus_serial_read_coils(modbus_client_id, 2, 1, 0, 4);
                    app_state = 2;
                } else {
                    /* State 2: Queue a Write Multiple Registers request starting from address 0.
                     *          This writes the 3 provided 16-bit values to the remote device. 
                     */
                    uint16_t holding_regs[3] = {0x1234, 0x5678, 0x9ABC};
                    mbus_serial_write_multiple_registers(modbus_client_id, 3, 1, 0, holding_regs, 3);
                    app_state = 0;
                }
            }
        }
    }
}


/* ========================================================================== */
/*                              UART IRQ CALLBACKS                            */
/* ========================================================================== */

/**
  * @brief UART Receive Complete Callback
  *        Triggered when a single byte is received. Queues it into the ring buffer.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        uint16_t next_head = (rx_head + 1) % RX_BUF_SIZE;
        /* Check if buffer is full before inserting */
        if (next_head != rx_tail) {
            rx_buf[rx_head] = rx_byte;
            rx_head = next_head;
        }
        /* Re-arm the interrupt to receive the next byte */
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

/**
  * @brief UART Error Callback (e.g. framing, noise, overrun)
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        /* On error, clear state and re-arm the interrupt to prevent lock-ups */
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

/**
 * No-op stub for pool lock (used when no external C caller provides it).
 */
void mbus_pool_lock(void) {}

/**
 * No-op stub for pool unlock.
 */
void mbus_pool_unlock(void) {}

/**
 * No-op stub for client lock.
 */
void mbus_client_lock(uint16_t _id) {}

/**
 * No-op stub for client unlock.
 */
void mbus_client_unlock(uint16_t _id) {}