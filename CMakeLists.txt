cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

# Set the project name
set(CMAKE_PROJECT_NAME 3DP_Wifi_Controller)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Core/led/bsp_led.c
        Core/FatFs/drivers/sd_diskio.c
        Core/FatFs/drivers/sd_diskio.h
        Core/FatFs/diskio.c
        Core/FatFs/ff.c
        Core/FatFs/ff_gen_drv.c
        Core/FatFs/ffconf_template.h
        Core/sdio/bsp_sdio_sdcard.c
        Core/sdio/bsp_sdio_sdcard.h
        Core/sdio/sdio_test.h
        Core/sdio/sdio_test.c
        Core/Inc/cmdHandler.h
        Core/Src/cmdHandler.c
        Core/Inc/cmdList.h
        Core/sdio/Fatfs_SDIO.c
        Core/sdio/Fatfs_SDIO.h
        Core/FatFs/ff_print_err.h
        Core/FatFs/ff_print_err.c
        Core/Inc/esp32.h
        Core/Src/esp32.c
        Core/lcd/bsp_ili9341_lcd.c
        Core/lcd/bsp_ili9341_lcd.h
        Core/lcd/bsp_xpt2046_lcd.c
        Core/lcd/bsp_xpt2046_lcd.h
        Core/STemWin/Config/GUI_X_Touch_Analog.c
        Core/STemWin/Config/GUIConf.c
        Core/STemWin/Config/LCDConf.h
        Core/STemWin/Config/LCDConf_FlexColor.c
        Core/STemWin/inc/BUTTON.h
        Core/STemWin/inc/BUTTON_Private.h
        Core/STemWin/inc/CALENDAR.h
        Core/STemWin/inc/CHECKBOX.h
        Core/STemWin/inc/CHECKBOX_Private.h
        Core/STemWin/inc/CHOOSECOLOR.h
        Core/STemWin/inc/CHOOSEFILE.h
        Core/STemWin/inc/DIALOG.h
        Core/STemWin/inc/DIALOG_Intern.h
        Core/STemWin/inc/DROPDOWN.h
        Core/STemWin/inc/DROPDOWN_Private.h
        Core/STemWin/inc/EDIT.h
        Core/STemWin/inc/EDIT_Private.h
        Core/STemWin/inc/FRAMEWIN.h
        Core/STemWin/inc/FRAMEWIN_Private.h
        Core/STemWin/inc/GRAPH.h
        Core/STemWin/inc/GRAPH_Private.h
        Core/STemWin/inc/GUI__BiDi2_Brackets_800.h
        Core/STemWin/inc/GUI__BiDi2_Types_800.h
        Core/STemWin/inc/GUI_ARRAY.h
        Core/STemWin/inc/GUI_ARRAY_Private.h
        Core/STemWin/inc/GUI_BMP_Private.h
        Core/STemWin/inc/GUI_Debug.h
        Core/STemWin/inc/GUI_FontIntern.h
        Core/STemWin/inc/GUI_GCache_Private.h
        Core/STemWin/inc/GUI_GIF_Private.h
        Core/STemWin/inc/GUI_HOOK.h
        Core/STemWin/inc/GUI_JPEG_Private.h
        Core/STemWin/inc/GUI_Private.h
        Core/STemWin/inc/GUI_SetOrientation.h
        Core/STemWin/inc/GUI_SetOrientationCX.h
        Core/STemWin/inc/GUI_SIM_Win32.h
        Core/STemWin/inc/GUI_SPRITE_Private.h
        Core/STemWin/inc/GUI_VNC.h
        Core/STemWin/inc/GUIDRV_DCache.h
        Core/STemWin/inc/GUIDRV_DCache_Private.h
        Core/STemWin/inc/GUIDRV_Dist.h
        Core/STemWin/inc/GUIDRV_FlexColor_Private.h
        Core/STemWin/inc/GUIDRV_Lin.h
        Core/STemWin/inc/GUIDRV_Lin_Opt_8.h
        Core/STemWin/inc/GUIDRV_Lin_Opt_16.h
        Core/STemWin/inc/GUIDRV_Lin_Opt_24.h
        Core/STemWin/inc/GUIDRV_Lin_Opt_32.h
        Core/STemWin/inc/GUIDRV_Lin_Private.h
        Core/STemWin/inc/GUIDRV_NoOpt_1_8.h
        Core/STemWin/inc/GUIDRV_Template.h
        Core/STemWin/inc/GUIDRV_TemplateI.h
        Core/STemWin/inc/GUIDRV_TemplateI_Private.h
        Core/STemWin/inc/GUIMTDRV_TangoC32.h
        Core/STemWin/inc/GUITDRV_ADS7846.h
        Core/STemWin/inc/HEADER.h
        Core/STemWin/inc/HEADER_Private.h
        Core/STemWin/inc/ICONVIEW.h
        Core/STemWin/inc/ICONVIEW_Private.h
        Core/STemWin/inc/IMAGE.h
        Core/STemWin/inc/IMAGE_Private.h
        Core/STemWin/inc/IP_FS.h
        Core/STemWin/inc/KNOB.h
        Core/STemWin/inc/KNOB_Private.h
        Core/STemWin/inc/LCD_ConfDefaults.h
        Core/STemWin/inc/LCD_Private.h
        Core/STemWin/inc/LCD_Protected.h
        Core/STemWin/inc/LCD_SIM.h
        Core/STemWin/inc/LISTBOX.h
        Core/STemWin/inc/LISTBOX_Private.h
        Core/STemWin/inc/LISTVIEW.h
        Core/STemWin/inc/LISTVIEW_Private.h
        Core/STemWin/inc/LISTWHEEL.h
        Core/STemWin/inc/LISTWHEEL_Private.h
        Core/STemWin/inc/MENU.h
        Core/STemWin/inc/MENU_Private.h
        Core/STemWin/inc/MESSAGEBOX.h
        Core/STemWin/inc/MULTIEDIT.h
        Core/STemWin/inc/MULTIPAGE.h
        Core/STemWin/inc/MULTIPAGE_Private.h
        Core/STemWin/inc/PROGBAR.h
        Core/STemWin/inc/PROGBAR_Private.h
        Core/STemWin/inc/RADIO.h
        Core/STemWin/inc/RADIO_Private.h
        Core/STemWin/inc/SCROLLBAR.h
        Core/STemWin/inc/SCROLLBAR_Private.h
        Core/STemWin/inc/SEGGER.h
        Core/STemWin/inc/SLIDER.h
        Core/STemWin/inc/SLIDER_Private.h
        Core/STemWin/inc/SPINBOX.h
        Core/STemWin/inc/SPINBOX_Private.h
        Core/STemWin/inc/SWIPELIST.h
        Core/STemWin/inc/SWIPELIST_Private.h
        Core/STemWin/inc/TEXT.h
        Core/STemWin/inc/TEXT_Private.h
        Core/STemWin/inc/TREEVIEW.h
        Core/STemWin/inc/TREEVIEW_Private.h
        Core/STemWin/inc/WIDGET.h
        Core/STemWin/inc/WINDOW_Private.h
        Core/STemWin/inc/WM.h
        Core/STemWin/inc/WM_GUI.h
        Core/STemWin/inc/WM_Intern.h
        Core/STemWin/OS/GUI_X_FreeRTOS.c
        Core/STemWin/UITask.h
        Core/STemWin/UITask.c
        Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_sram.c
        Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_fsmc.c
        Core/FatFs/option/unicode.c
        Core/STemWin_Task/FramewinDLG.h
        Core/STemWin_Task/Page1DLG.c
        Core/STemWin_Task/Page2DLG.c
        Core/STemWin_Task/Page3DLG.c
        Core/STemWin_Task/Page4DLG.c
        Core/Inc/printerController.h
        Core/Src/printerController.c
        Core/Inc/fileTask.h
        Core/Src/fileTask.c
        Core/sha256/sha256.h
        Core/sha256/sha256.c
        Core/hx711/hx711.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
        DEBUG
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        stm32cubemx
        # Add user defined libraries
)

