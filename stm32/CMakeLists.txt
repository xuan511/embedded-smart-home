cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/arm-none-eabi.cmake" CACHE FILEPATH "Arm GNU bare-metal toolchain")
project(home_stm32 C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(STAGE 1 CACHE STRING "1=link only; 2=test data+LED; 3=real sensors+LED; 4=real loads")
if(NOT STAGE MATCHES "^[1-4]$")
    message(FATAL_ERROR "STAGE must be 1, 2, 3, or 4")
endif()
set(LED_PORT C CACHE STRING "Confirmed LED GPIO port A/B/C, stages 2 and 3 only")
set(LED_PIN 13 CACHE STRING "Confirmed LED GPIO pin 0..15")
set(LED_ACTIVE_LOW 1 CACHE STRING "Confirmed LED polarity: 1 active low; 0 active high")
if(LED_PORT STREQUAL "A")
    set(LED_GPIO_BASE 0x40010800u)
elseif(LED_PORT STREQUAL "B")
    set(LED_GPIO_BASE 0x40010c00u)
elseif(LED_PORT STREQUAL "C")
    set(LED_GPIO_BASE 0x40011000u)
else()
    message(FATAL_ERROR "LED_PORT must be A, B, or C")
endif()
if(NOT LED_PIN MATCHES "^[0-9]+$" OR LED_PIN GREATER 15)
    message(FATAL_ERROR "LED_PIN must be 0..15")
endif()
if(NOT LED_ACTIVE_LOW MATCHES "^[01]$")
    message(FATAL_ERROR "LED_ACTIVE_LOW must be 0 or 1")
endif()
# Reject pins whose other fixed assignments would make a configured LED unsafe.
if((STAGE EQUAL 2 OR STAGE EQUAL 3) AND
   ((LED_PORT STREQUAL "A" AND (LED_PIN EQUAL 9 OR LED_PIN EQUAL 10 OR LED_PIN EQUAL 13 OR LED_PIN EQUAL 14 OR LED_PIN EQUAL 15)) OR
    (LED_PORT STREQUAL "B" AND (LED_PIN EQUAL 0 OR LED_PIN EQUAL 1 OR LED_PIN EQUAL 3 OR LED_PIN EQUAL 4 OR LED_PIN EQUAL 6 OR LED_PIN EQUAL 7))))
    message(FATAL_ERROR "LED pin conflicts with fixed UART/debug/I2C/load assignments")
endif()
set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
add_executable(home_stm32
    startup_stm32f103x8.S src/main.c src/board.c src/sensors.c src/stm32_controller.c
    "${REPO_ROOT}/components/home_core/home_core.c"
    "${REPO_ROOT}/components/home_link/home_link.c")
target_include_directories(home_stm32 PRIVATE include
    "${REPO_ROOT}/components/home_core/include" "${REPO_ROOT}/components/home_link/include")
target_compile_definitions(home_stm32 PRIVATE STAGE=${STAGE} LED_GPIO_BASE=${LED_GPIO_BASE}
    LED_PIN=${LED_PIN} LED_ACTIVE_LOW=${LED_ACTIVE_LOW})
target_compile_options(home_stm32 PRIVATE -mcpu=cortex-m3 -mthumb -Os -g3 -Wall -Wextra -Werror
    -ffunction-sections -fdata-sections -fno-common -fstack-usage)
target_link_options(home_stm32 PRIVATE -mcpu=cortex-m3 -mthumb
    "-T${CMAKE_CURRENT_LIST_DIR}/stm32f103c8.ld" -Wl,--gc-sections
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/home_stm32.map" -Wl,--print-memory-usage -Wl,-z,noexecstack
    --specs=nano.specs --specs=nosys.specs -nostartfiles)
target_link_libraries(home_stm32 PRIVATE m c gcc)
set_target_properties(home_stm32 PROPERTIES SUFFIX ".elf")
add_custom_command(TARGET home_stm32 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:home_stm32>" "${CMAKE_CURRENT_BINARY_DIR}/home_stm32.bin"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:home_stm32>" "${CMAKE_CURRENT_BINARY_DIR}/home_stm32.hex"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:home_stm32>"
    VERBATIM)
