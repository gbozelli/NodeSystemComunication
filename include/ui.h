#ifndef UI_H
#define UI_H

#include "definitions.h"

// Protótipos das funções de visualização e UI
void PrintNonCompletedMessages();
void DrawNetwork();
void DrawTravelingMessages();
void DrawQueuedMessages();
void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed);
void DrawStatistics(int screenW);
void DrawParametersUI(int screenW);

#endif // UI_H
