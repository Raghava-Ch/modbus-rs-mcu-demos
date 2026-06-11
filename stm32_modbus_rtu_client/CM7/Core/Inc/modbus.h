/**
  ******************************************************************************
  * @file    modbus.h
  * @brief   Modbus configuration and initialization API
  ******************************************************************************
  */
#ifndef MODBUS_H
#define MODBUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Initialize the Modbus client and queue the startup operations.
  */
void Modbus_Init(void);

/**
  * @brief Continually drive the Modbus state machine. Place in the main while(1) loop.
  */
void Modbus_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_H */