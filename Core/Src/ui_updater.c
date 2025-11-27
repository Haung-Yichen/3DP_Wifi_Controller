#include "ui_updater.h"
#include "DIALOG.h"
#include "GUI.h"
#include "MESSAGEBOX.h"
#include "cmsis_os2.h"
#include <stdio.h>

// External page handles from FramewinDLG.c
extern WM_HWIN hPage1;
extern WM_HWIN hPage3;

// Widget IDs from Page1DLG.c
#define ID_TEXT_NOZ_VALUE   (GUI_ID_USER + 0x04)
#define ID_TEXT_BED_VALUE   (GUI_ID_USER + 0x07)
#define ID_PROGBAR_0        (GUI_ID_USER + 0x08)
#define ID_TEXT_TIME        (GUI_ID_USER + 0x09)
#define ID_TEXT_STATUS      (GUI_ID_USER + 0x0A)

// Widget IDs from Page3DLG.c
#define ID_TEXT_WEIGHT      (GUI_ID_USER + 0x03)

void UI_Update_Status(const char* status) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_TEXT_STATUS);
        if (hItem != 0) {
            char statusStr[32];
            snprintf(statusStr, sizeof(statusStr), "Status: %s", status);
            TEXT_SetText(hItem, statusStr);
        }
    }
}

void UI_Update_NozzleTemp(int temp) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_TEXT_NOZ_VALUE);
        if (hItem != 0) {
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%d C", temp);
            TEXT_SetText(hItem, tempStr);
        }
    }
}

void UI_Update_BedTemp(const char* temp) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_TEXT_BED_VALUE);
        if (hItem != 0) {
            // The original code used "N/A C", so we'll just use the string directly for now.
            // If it needs to be formatted with a value, this can be changed.
            if(temp[0] == 'N')
            {
                TEXT_SetText(hItem, "N/A C");
            }
            else
            {
                char tempStr[16];
                snprintf(tempStr, sizeof(tempStr), "%s C", temp);
                TEXT_SetText(hItem, tempStr);
            }
        }
    }
}

void UI_Update_BedTemp_Int(int temp) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_TEXT_BED_VALUE);
        if (hItem != 0) {
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%d C", temp);
            TEXT_SetText(hItem, tempStr);
        }
    }
}

void UI_Update_Progress(int progress) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_PROGBAR_0);
        if (hItem != 0) {
            PROGBAR_SetValue(hItem, progress);
        }
    }
}

void UI_Update_RemainingTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    if (hPage1 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage1, ID_TEXT_TIME);
        if (hItem != 0) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "Time Left: %02d:%02d:%02d",
                     hours, minutes, seconds);
            TEXT_SetText(hItem, timeStr);
        }
    }
}

void UI_Update_FilamentWeight(int weight) {
    if (hPage3 != 0) {
        WM_HWIN hItem = WM_GetDialogItem(hPage3, ID_TEXT_WEIGHT);
        if (hItem != 0) {
            char weightStr[16];
            snprintf(weightStr, sizeof(weightStr), "%d g", weight);
            TEXT_SetText(hItem, weightStr);
        }
    }
}

static WM_HWIN hAutoCloseMessageBox = 0;
static int autoCloseCounter = 0;

static void AutoCloseMessageBox_Callback(WM_MESSAGE *pMsg) {
    switch (pMsg->MsgId) {
        case WM_TIMER:
            autoCloseCounter++;
            if (autoCloseCounter >= 30) { // 30 * 100ms = 3 seconds
                GUI_EndDialog(pMsg->hWin, 0);
                hAutoCloseMessageBox = 0;
            } else {
                WM_RestartTimer(pMsg->Data.v, 100);
            }
            break;
            
        case WM_NOTIFY_PARENT:
            if (pMsg->Data.v == WM_NOTIFICATION_RELEASED) {
                GUI_EndDialog(pMsg->hWin, 0);
                hAutoCloseMessageBox = 0;
            }
            break;
            
        default:
            MESSAGEBOX_Callback(pMsg);
            break;
    }
}

void UI_Show_FileUploadSuccess(void) {
    // Close existing message box if any
    if (hAutoCloseMessageBox != 0) {
        GUI_EndDialog(hAutoCloseMessageBox, 0);
        hAutoCloseMessageBox = 0;
    }
    
    // Create message box
    hAutoCloseMessageBox = MESSAGEBOX_Create("File uploaded!", "Success", 0);
    
    if (hAutoCloseMessageBox != 0) {
        // Setup callback
        WM_SetCallback(hAutoCloseMessageBox, AutoCloseMessageBox_Callback);
        
        // Center the message box on screen
        int xSize = WM_GetWindowSizeX(hAutoCloseMessageBox);
        int ySize = WM_GetWindowSizeY(hAutoCloseMessageBox);
        int xPos = (LCD_GetXSize() - xSize) / 2;
        int yPos = (LCD_GetYSize() - ySize) / 2;
        WM_MoveTo(hAutoCloseMessageBox, xPos, yPos);
        
        // Start timer for auto-close
        autoCloseCounter = 0;
        WM_CreateTimer(hAutoCloseMessageBox, 0, 100, 0);
        
        // Ensure it's on top
        WM_BringToTop(hAutoCloseMessageBox);
    }
}
