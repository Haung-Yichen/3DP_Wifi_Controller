#ifndef UI_UPDATER_H
#define UI_UPDATER_H

#include <stdint.h>

void UI_Update_NozzleTemp(int temp);
void UI_Update_BedTemp(const char* temp);
void UI_Update_Progress(int progress);
void UI_Update_RemainingTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
void UI_Update_FilamentWeight(int weight);

#endif //UI_UPDATER_H
