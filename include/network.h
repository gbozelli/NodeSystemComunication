#ifndef NETWORK_H
#define NETWORK_H

#include "definitions.h"

// Protótipos das funções de gerenciamento da rede
void PushAction(ActionType type, int a, int b);
void UndoAction();
void AddNode(float x, float y);
void ConnectNodes(int a, int b);
int BuildPath(int start, int goal, int *path, int maxLen);
void CreateDefaultNetwork();

#endif // NETWORK_H
