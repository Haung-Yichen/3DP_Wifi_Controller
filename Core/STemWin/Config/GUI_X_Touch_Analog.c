#include "GUI.h"
#include "bsp_xpt2046_lcd.h"
#include "bsp_ili9341_lcd.h"

// 全局變量儲存觸控狀態和原始 ADC 值
static int LastTouchX = 0;
static int LastTouchY = 0;
static uint8_t TouchDetected = 0;

/*********************************************************************
*
*       GUI_TOUCH_X_ActivateX
*
* Purpose:
*   Called from GUI, if touch support is enabled.
*   Switches on voltage on X-axis, prepares measurement of Y-axis.
*   Voltage on Y-axis is switched off.
*/
void GUI_TOUCH_X_ActivateX(void)
{
    // XPT2046 使用差分測量，無需單獨激活 X 軸
}

/*********************************************************************
*
*       GUI_TOUCH_X_ActivateY
*
* Purpose:
*   Called from GUI, if touch support is enabled.
*   Switches on voltage on Y-axis, prepares measurement of X-axis.
*   Voltage on X-axis is switched off.
*/
void GUI_TOUCH_X_ActivateY(void)
{
    // XPT2046 使用差分測量，無需單獨激活 Y 軸
}

/*********************************************************************
*
*       GUI_TOUCH_X_MeasureX
*
* Purpose:
*   Return the X-value (raw ADC value) of the currently selected position.
*   Return value is either the X ADC value (0-4095), or -1 if not pressed.
*
* Notes:
*   Called periodically by the GUI. Returns consistent measurements.
*/
int GUI_TOUCH_X_MeasureX(void)
{
    strType_XPT2046_Coordinate touchCoordinate;

    // 檢查觸控屏是否被按下
    if (XPT2046_TouchDetect() == TOUCH_PRESSED)
    {
        // 獲取平滑的原始 ADC 值
        if (XPT2046_ReadAdc_Smooth_XY(&touchCoordinate))
        {
            // 更新最新原始 ADC 值
            LastTouchX = touchCoordinate.x;
            LastTouchY = touchCoordinate.y;
            TouchDetected = 1;
            return LastTouchX; // 返回 X 軸原始 ADC 值 (0-4095)
        }
    }
    else
    {
        TouchDetected = 0; // 無觸控
    }

    return -1; // 未按下或無有效數據時返回 -1
}

/*********************************************************************
*
*       GUI_TOUCH_X_MeasureY
*
* Purpose:
*   Return the Y-value (raw ADC value) of the currently selected position.
*   Return value is either the Y ADC value (0-4095), or -1 if not pressed.
*
* Notes:
*   Called periodically by the GUI. Returns consistent measurements.
*/
int GUI_TOUCH_X_MeasureY(void)
{
    strType_XPT2046_Coordinate touchCoordinate;

    // 檢查觸控屏是否被按下
    if (XPT2046_TouchDetect() == TOUCH_PRESSED)
    {
        // 獲取平滑的原始 ADC 值
        if (XPT2046_ReadAdc_Smooth_XY(&touchCoordinate))
        {
            // 更新最新原始 ADC 值
            LastTouchX = touchCoordinate.x;
            LastTouchY = touchCoordinate.y;
            TouchDetected = 1;
            return LastTouchY; // 返回 Y 軸原始 ADC 值 (0-4095)
        }
    }
    else
    {
        TouchDetected = 0; // 無觸控
    }

    return -1; // 未按下或無有效數據時返回 -1
}