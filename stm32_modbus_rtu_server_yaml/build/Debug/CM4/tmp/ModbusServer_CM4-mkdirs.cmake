# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Volumes/Work/ModBus/stm32/ModbusServer/CM4")
  file(MAKE_DIRECTORY "/Volumes/Work/ModBus/stm32/ModbusServer/CM4")
endif()
file(MAKE_DIRECTORY
  "/Volumes/Work/ModBus/stm32/ModbusServer/CM4/build"
  "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4"
  "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/tmp"
  "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/src/ModbusServer_CM4-stamp"
  "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/src"
  "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/src/ModbusServer_CM4-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/src/ModbusServer_CM4-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Volumes/Work/ModBus/stm32/ModbusServer/build/Debug/CM4/src/ModbusServer_CM4-stamp${cfgdir}") # cfgdir has leading slash
endif()
