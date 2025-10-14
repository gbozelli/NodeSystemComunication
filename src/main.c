#include "../include/definitions.h"
#include "../include/network.h"
#include "../include/messaging.h"
#include "../include/ui.h"

//====================================================================================
// DEFINIÇÃO DAS VARIÁVEIS GLOBAIS
//====================================================================================
// Neste modelo, o main.c é responsável por criar as variáveis globais.
// Elas são declaradas como 'extern' em definitions.h para serem usadas em outros arquivos.

// --- PARÂMETROS DA SIMULAÇÃO ---
int g_link_capacity = 20;
float g_timeout_seconds = 10.0f;
float g_prob_success = 1.0f;

// --- DADOS DA SIMULAÇÃO ---
Node nodes[MAX_NODES];
int nodeCount = 0;
AsyncMessage messages[MAX_MESSAGES];
int messageCount = 0;

Network pathfindingNetwork;
Network capacityNetwork;

Action actionStack[100];
int actionTop = -1;

// --- ESTATÍSTICAS ---
long long total_latency_ticks = 0;
int completed_messages_count = 0;
int total_retransmissions = 0;

//====================================================================================
// FUNÇÃO PRINCIPAL
//====================================================================================
int main(void)
{
  const int screenW = 1280, screenH = 720;
  InitWindow(screenW, screenH, "Simulador de Rede Avançado");
  SetTargetFPS(60);
  srand(time(NULL));

  // Variáveis de controle da UI e da lógica do loop principal
  int uiFromNode = 0, uiToNode = 13, uiMsgCount = 50;
  bool sendPressed = false, burstInProgress = false;
  int messagesToSend = 0, burstRoundsSent = 0;
  float messageSendTimer = 0.0f, burstTimer = 0.0f;
  int messageFromNode = -1, messageToNode = -1, nodeToConnect = -1;
  const int TOTAL_BURST_ROUNDS = 10;

  // Cria a rede padrão no início
  CreateDefaultNetwork();

  while (!WindowShouldClose())
  {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();
    Rectangle uiArea = {screenW - 270, 10, 260, 550}; // Ajustado para conter todos os paineis

    // Lógica de envio de mensagens da UI
    if (sendPressed)
    {
      sendPressed = false;
      messagesToSend = uiMsgCount;
      messageFromNode = uiFromNode;
      messageToNode = uiToNode;
      messageSendTimer = MESSAGE_INTERVAL;
    }
    if (messagesToSend > 0)
    {
      messageSendTimer += dt;
      if (messageSendTimer >= MESSAGE_INTERVAL)
      {
        if (messageFromNode != messageToNode)
          AddAsyncMessage(messageFromNode, messageToNode);
        messagesToSend--;
        messageSendTimer = 0.0f;
      }
    }

    // Atualiza o estado de todas as mensagens
    UpdateAsyncMessages(dt, 0.1f);

    // Lógica de interação com o mouse (Adicionar/Conectar nós)
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, uiArea))
    {
      AddNode(mouse.x, mouse.y);
      PushAction(ACTION_ADD_NODE, nodeCount - 1, -1);
    }
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
      int clickedNode = -1;
      for (int i = 0; i < nodeCount; i++)
        if (CheckCollisionPointCircle(mouse, (Vector2){nodes[i].x, nodes[i].y}, NODE_RADIUS))
        {
          clickedNode = i;
          break;
        }
      if (clickedNode != -1)
      {
        if (nodeToConnect == -1)
          nodeToConnect = clickedNode;
        else
        {
          if (nodeToConnect != clickedNode)
          {
            ConnectNodes(nodeToConnect, clickedNode);
            PushAction(ACTION_CONNECT_NODES, nodeToConnect, clickedNode);
          }
          nodeToConnect = -1;
        }
      }
      else
      {
        nodeToConnect = -1;
      }
    }

    // Lógica de interação com o teclado
    if (IsKeyPressed(KEY_Q))
      CreateDefaultNetwork();
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
      UndoAction();
    if (IsKeyPressed(KEY_W))
      nodeToConnect = -1;
    if (IsKeyPressed(KEY_P))
      PrintNonCompletedMessages();
    if (IsKeyPressed(KEY_B))
    {
      burstInProgress = true;
      burstRoundsSent = 0;
      burstTimer = 0.0f;
    }

    // Lógica de rajada de mensagens (burst)
    if (burstInProgress)
    {
      burstTimer += dt;
      if (burstTimer >= MESSAGE_INTERVAL)
      {
        SendOneBurstRound();
        burstRoundsSent++;
        burstTimer = 0.0f;
        if (burstRoundsSent >= TOTAL_BURST_ROUNDS)
          burstInProgress = false;
      }
    }

    // --- Seção de Desenho ---
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawNetwork();
    DrawTravelingMessages();
    DrawQueuedMessages();

    // Desenha as UIs
    DrawUI(uiArea, &uiFromNode, &uiToNode, &uiMsgCount, &sendPressed);
    DrawStatistics(screenW);
    DrawParametersUI(screenW);

    // Desenha textos de ajuda
    DrawText("ESQ: Adicionar | DIR: Conectar", 10, 10, 20, DARKGRAY);
    DrawText("Q: Rede Padrao | W: Limpar | P: Status | B: Rajada", 10, 40, 20, DARKGRAY);
    DrawText("CTRL+Z: Desfazer", 10, 70, 20, DARKGRAY);

    // Desenha linha de feedback para conexão de nós
    if (nodeToConnect != -1)
    {
      char buffer[64];
      sprintf(buffer, "Conectar nó %d com...", nodeToConnect);
      DrawText(buffer, 10, 100, 20, RED);
      DrawLine(nodes[nodeToConnect].x, nodes[nodeToConnect].y, mouse.x, mouse.y, DARKGRAY);
    }

    EndDrawing();
  }
  CloseWindow();
  return 0;
}
