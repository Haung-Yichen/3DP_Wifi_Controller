cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

set(MX_Defines_Syms
		USE_HAL_DRIVER
		STM32F103xE
		STM32_THREAD_SAFE_STRATEGY=4
		$<$<CONFIG:Debug>:DEBUG>
)

set(MX_Include_Dirs
		${CMAKE_SOURCE_DIR}/Core/Inc
		${CMAKE_SOURCE_DIR}/Core/led
		${CMAKE_SOURCE_DIR}/Core/FatFs
		${CMAKE_SOURCE_DIR}/Core/FatFs/drivers
		${CMAKE_SOURCE_DIR}/Core/sdio
		${CMAKE_SOURCE_DIR}/Core/lcd
		${CMAKE_SOURCE_DIR}/Core/hx711
		${CMAKE_SOURCE_DIR}/Core/sha256
		${CMAKE_SOURCE_DIR}/Core/STemWin
		${CMAKE_SOURCE_DIR}/Core/STemWin/Config
		${CMAKE_SOURCE_DIR}/Core/STemWin/inc
		${CMAKE_SOURCE_DIR}/Core/STemWin/Lib
		${CMAKE_SOURCE_DIR}/Core/STemWin/OS
		${CMAKE_SOURCE_DIR}/Core/STemWin_Task
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
		${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
		${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

set(MX_Application_Src
		${CMAKE_SOURCE_DIR}/Core/Src/main.c
		${CMAKE_SOURCE_DIR}/Core/Src/gpio.c
		${CMAKE_SOURCE_DIR}/Core/Src/freertos.c
		${CMAKE_SOURCE_DIR}/Core/Src/dma.c
		${CMAKE_SOURCE_DIR}/Core/Src/usart.c
		${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_it.c
		${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_hal_msp.c
		${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_hal_timebase_tim.c
		${CMAKE_SOURCE_DIR}/Core/Src/sysmem.c
		${CMAKE_SOURCE_DIR}/Core/Src/syscalls.c
		${CMAKE_SOURCE_DIR}/startup_stm32f103xe.s
)

set(STM32_Drivers_Src
		${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f1xx.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_sd.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_sdmmc.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
		${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
)

set(FreeRTOS_Src
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/croutine.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/list.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/queue.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/timers.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
		${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
)

# === 新增的 STemWin 靜態庫路徑與庫名 ===
set(MX_LINK_DIRS
		${CMAKE_SOURCE_DIR}/Core/STemWin/Lib
)

set(MX_LINK_LIBS
		STM32_Drivers
		FreeRTOS
#		${CMAKE_SOURCE_DIR}/Core/STemWin/Lib/STemWin532_CM3_OS_Keil.lib
		${CMAKE_SOURCE_DIR}/Core/STemWin/Lib/STemWin_CM3_OS_wc16.a
		${CMAKE_SOURCE_DIR}/Core/STemWin/Lib/STemWin_CM3_OS_wc16_ARGB.a
)

add_compile_options(-fshort-wchar)
add_link_options("-Wl,--no-wchar-size-warning")
add_compile_definitions(DEBUG)

add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs} ${CMOX_INCLUDE_DIR})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

add_library(FreeRTOS OBJECT)
target_sources(FreeRTOS PRIVATE ${FreeRTOS_Src})
target_link_libraries(FreeRTOS PUBLIC stm32cubemx)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src} ${CMOX_Sources})

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_LIBS})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
	message(ERROR "Generated code requires C11 or higher")
endif()
