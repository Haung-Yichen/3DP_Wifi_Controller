/**
* @file    cmdList.h
 * @author  Huang YiChen
 * @date    2025-09-05
 * @brief   定義本系統與網頁端交互所需的命令。
 *
 * 本檔案統一規範系統與網頁端溝通使用的命令格式，亦供系統內部
 * UI 模組引用。所有需要攜帶參數的命令，皆應以 <參數> 形式直接
 * 接續於命令之後。為確保兼容性（特別是檔案哈希校驗碼），參數
 * 長度限制為 300。一般系統設定（如溫度參數）不會超過三位數。
 *
 * 查詢命令規範：
 * - 若使用查詢命令，系統在相關資料準備完成後，會直接回傳對應參數值。
 * - 例如：向系統發送 `cReqBedTemp`，則系統回傳 `30`，代表熱床溫度為 30 度。
 * - 所有查詢結果均不附帶換行字元 ('\n')。
 */

#ifndef INC_COMMAND_LIST_H_
#define INC_COMMAND_LIST_H_

/*            命令            */
#define CMD_WIFI_STATUS         (const char*)"cWifiStatus"        //wifi狀態設定
#define CMD_CLIENT_STATUS       (const char*)"cClintStatus"       //網頁連接狀況
#define CMD_Start_Transmisson   (const char*)"cStartTransmission" //開始傳送(檔案)
#define CMD_Transmisson_Over    (const char*)"cTransmissionOver"  //傳送完畢(檔案)
#define CMD_SET_FILENAME        (const char*)"cSetFilename"       //設置檔名
#define CMD_Start_To_Print      (const char*)"cStartToPrint"      //開始列印
#define CMD_Pause_Printing      (const char*)"cPausePrinting"     //暫停列印S
#define CMD_Stop_printing       (const char*)"cStopPrinting"      //停止列印
#define CMD_Go_Home             (const char*)"cGoHome"            //回原點
#define CMD_Get_Remainning_time (const char*)"cReqRemainningTime" //請求剩餘列印時間
#define CMD_Get_Progress        (const char*)"cReqProgress"       //請求進度(百分比)
#define CMD_Get_Nozzle_Temp     (const char*)"cReqNozzleTemp"     //請求噴頭溫賭
#define CMD_Get_Bed_Temp        (const char*)"cReqBedTemp"        //請求熱床溫度
#define CMD_Set_Nozzle_Temp     (const char*)"cSetNozzleTemp"     //設置噴嘴溫度
#define CMD_Set_Bed_Temp        (const char*)"cSetBedTemp"        //設置熱床溫度
#define CMD_Enable_Camera       (const char*)"cEnableCamera"      //啟用錄影
#define CMD_Disable_Camera      (const char*)"cDisableCamera"     //進用錄影
#define CMD_GetFilament_Weight  (const char*)"cReqFilamentWeight" //請求耗材重量
#define CMD_Emergency_Stop      (const char*)"cEmergencyStop"     //緊急停止


/*            錯誤碼            */
#define ERROR_PARAM_REQ         (const char*)"eParamReq"          //參數查詢錯誤
#define ERROR_FILE_BROKEN       (const char*)"eFileBroken"        //檔案錯誤

#endif /* INC_COMMAND_LIST_H_ */
