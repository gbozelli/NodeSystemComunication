#ifndef MESSAGING_H
#define MESSAGING_H

#include "definitions.h"

// Protótipos das funções de lógica de mensagens
void AddAsyncMessage(int from, int to);
void UpdateAsyncMessages(float dt, float releaseInterval);
void SendOneBurstRound();

#endif // MESSAGING_H
