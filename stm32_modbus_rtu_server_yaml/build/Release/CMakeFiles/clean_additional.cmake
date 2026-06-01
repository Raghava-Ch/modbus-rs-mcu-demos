# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "/Volumes/Work/ModBus/stm32/ModbusServer/CM4/build"
  "/Volumes/Work/ModBus/stm32/ModbusServer/CM7/build"
  )
endif()
