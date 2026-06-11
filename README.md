# modbus-rs MCU Demos

This repository contains controller-based demonstration projects showcasing the integration of the [modbus-rs](https://github.com/Raghava-Ch/modbus-rs) protocol library into bare-metal C environments on STM32 microcontrollers.

These examples demonstrate how to successfully bridge safe Rust-based Modbus logic with hardware-level C code via Foreign Function Interface (FFI) and `cbindgen`.

## 📁 Included Demos

### 1. STM32 Modbus RTU Server (`stm32_modbus_rtu_server_yaml`)
A bare-metal Modbus RTU Server (Slave) implementation.
- **Features**: Maps Modbus frames directly to hardware actions. E.g., turning on/off onboard Nucleo LEDs via Coils, reading the User Button via Discrete Inputs, and controlling blink delays via Holding Registers.
- **Architecture**: Non-blocking super-loop with interrupt-driven UART RX (ring buffer) and non-blocking TX.
- **Configuration**: Data points and mappings are driven by a `server.yaml` specification.

### 2. STM32 Modbus RTU Client (`stm32_modbus_rtu_client`)
A Modbus RTU Client (Master) implementation designed to interact with Modbus servers.
- **Features**: Demonstrates formulating, sending, and parsing Modbus RTU requests and responses utilizing the `modbus-rs` stack in a constrained, bare-metal MCU environment.

## ⚙️ Hardware Target
The demos are natively configured for **STM32H7 Series** microcontrollers (e.g., STM32H7 Nucleo boards), utilizing:
*   **UART Peripheral**: `USART3`
*   **Baud Rate**: 115200 bps (8 data bits, no parity, 1 stop bit)
*   **Pins**: `PD8` (TX), `PD9` (RX)

## 🛠️ Development Environment & Prerequisites
To build, flash, and interact with these demos, the following tools are highly recommended:
1. **STM32CubeIDE** & **STM32CubeMX**: For C code development, building, and hardware configuration.
2. **Rust Toolchain**: For compiling the underlying `modbus-rs` stack into a static archive (`.a`).
3. **CMake / Make**: Included configurations for GCC ARM toolchains (`gcc-arm-none-eabi.cmake`).

## 📖 Getting Started
Please refer to the specific `README.md` inside each demo folder for detailed build instructions, memory mapping, interrupt configurations, and hardware setup.
