#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PROB_SUCCESS 0.5f
#define MAX_NODES 50
#define NODE_RADIUS 20
#define MAX_CONNECTIONS 10
#define MAX_MESSAGES 100
#define MESSAGE_SPEED 1.5f    // Velocidade da animação da mensagem
#define MESSAGE_INTERVAL 0.2f // Intervalo de 0.2s entre o envio de cada mensagem em lote

// ... (As structs Node, AsyncMessage, ActionType, Action continuam as mesmas) ...
typedef struct Node
{
  float x, y;
  int id;
  int connections[MAX_CONNECTIONS];
  int connectionCount;
} Node;

Node nodes[MAX_NODES];
int nodeCount = 0;

typedef enum MsgState
{
  SENDING,
  ACK_RECEIVING,
  DONE
} MsgState;

typedef struct AsyncMessage
{
  int from;
  int to;
  MsgState state;
  float progress; // 0 a 1 para animar o trecho atual

  int ackSegment;      // índice do segmento do ACK
  int path[MAX_NODES]; // caminho completo
  int pathLength;      // tamanho do caminho
  int currentSegment;  // segmento atual do caminho
} AsyncMessage;

typedef struct Network{
  int path[MAX_NODES][MAX_NODES];
} Network;

AsyncMessage messages[MAX_MESSAGES];
int messageCount = 0;

typedef enum ActionType
{
  ACTION_ADD_NODE,
  ACTION_CONNECT_NODES
} ActionType;

typedef struct Action
{
  ActionType type;
  int nodeA;
  int nodeB;
} Action;

Action actionStack[100];
int actionTop = -1;
// ... (As funções PushAction, UndoAction, AddNode, DrawNetwork, ConnectNodes, BuildPath, CreateDefaultNetwork continuam as mesmas) ...

void PushAction(ActionType type, int a, int b)
{
  if (actionTop < 99)
  {
    actionTop++;
    actionStack[actionTop].type = type;
    actionStack[actionTop].nodeA = a;
    actionStack[actionTop].nodeB = b;
  }
}

void UndoAction()
{
  if (actionTop < 0)
    return;

  Action act = actionStack[actionTop--];

  if (act.type == ACTION_ADD_NODE)
  {
    if (nodeCount > 0)
      nodeCount--;
  }
  else if (act.type == ACTION_CONNECT_NODES)
  {
    int a = act.nodeA;
    int b = act.nodeB;
    for (int i = 0; i < nodes[a].connectionCount; i++)
    {
      if (nodes[a].connections[i] == b)
      {
        for (int j = i; j < nodes[a].connectionCount - 1; j++)
          nodes[a].connections[j] = nodes[a].connections[j + 1];
        nodes[a].connectionCount--;
        break;
      }
    }
    for (int i = 0; i < nodes[b].connectionCount; i++)
    {
      if (nodes[b].connections[i] == a)
      {
        for (int j = i; j < nodes[b].connectionCount - 1; j++)
          nodes[b].connections[j] = nodes[b].connections[j + 1];
        nodes[b].connectionCount--;
        break;
      }
    }
  }
}

void AddNode(float x, float y)
{
  if (nodeCount >= MAX_NODES)
    return;
  nodes[nodeCount].id = nodeCount;
  nodes[nodeCount].x = x;
  nodes[nodeCount].y = y;
  nodes[nodeCount].connectionCount = 0;
  nodeCount++;
}

void DrawNetwork()
{
  for (int i = 0; i < nodeCount; i++)
  {
    for (int j = 0; j < nodes[i].connectionCount; j++)
    {
      int b = nodes[i].connections[j];
      if (b > i)
        DrawLine(nodes[i].x, nodes[i].y, nodes[b].x, nodes[b].y, GRAY);
    }
  }

  for (int i = 0; i < nodeCount; i++)
  {
    DrawCircle(nodes[i].x, nodes[i].y, NODE_RADIUS, BLUE);
    char label[8];
    sprintf(label, "%d", nodes[i].id);
    DrawText(label, nodes[i].x - 5, nodes[i].y - 10, 20, WHITE);
  }
}

void ConnectNodes(int a, int b)
{
  if (a < 0 || b < 0 || a >= nodeCount || b >= nodeCount || a == b)
    return;
  if (nodes[a].connectionCount >= MAX_CONNECTIONS || nodes[b].connectionCount >= MAX_CONNECTIONS)
    return;

  int exists = 0;
  for (int i = 0; i < nodes[a].connectionCount; i++)
    if (nodes[a].connections[i] == b)
      exists = 1;
  if (!exists)
    nodes[a].connections[nodes[a].connectionCount++] = b;

  exists = 0;
  for (int i = 0; i < nodes[b].connectionCount; i++)
    if (nodes[b].connections[i] == a)
      exists = 1;
  if (!exists)
    nodes[b].connections[nodes[b].connectionCount++] = a;
}

int BuildPath(int start, int goal, int *path, int maxLen)
{
  int visited[MAX_NODES] = {0};
  int parent[MAX_NODES];
  for (int i = 0; i < MAX_NODES; i++)
    parent[i] = -1;

  int queue[MAX_NODES];
  int front = 0, rear = 0;
  visited[start] = 1;
  queue[rear++] = start;

  int found = 0;
  while (front < rear)
  {
    int current = queue[front++];
    if (current == goal)
    {
      found = 1;
      break;
    }

    for (int i = 0; i < nodes[current].connectionCount; i++)
    {
      int next = nodes[current].connections[i];
      if (!visited[next])
      {
        visited[next] = 1;
        parent[next] = current;
        queue[rear++] = next;
      }
    }
  }

  if (!found)
    return -1;

  int temp[MAX_NODES];
  int len = 0;
  int cur = goal;
  while (cur != -1)
  {
    temp[len++] = cur;
    cur = parent[cur];
  }

  for (int i = 0; i < len; i++)
  {
    path[i] = temp[len - i - 1];
  }
  return len;
}

void CreateDefaultNetwork()
{
  nodeCount = 0;
  messageCount = 0;
  actionTop = -1;
  AddNode(150, 400);
  AddNode(300, 350);
  AddNode(300, 650);
  AddNode(450, 400);
  AddNode(600, 350);
  AddNode(600, 650);
  ConnectNodes(0, 1);
  ConnectNodes(0, 2);
  ConnectNodes(1, 3);
  ConnectNodes(2, 3);
  ConnectNodes(3, 4);
  ConnectNodes(3, 5);
}

// ================== MENSAGENS ASSÍNCRONAS (LÓGICA CENTRAL) ==================

void AddAsyncMessage(int from, int to)
{
  if (messageCount >= MAX_MESSAGES)
    return;

  AsyncMessage *m = &messages[messageCount];
  m->from = from;
  m->to = to;
  m->state = SENDING;
  m->progress = 0;
  m->currentSegment = 0;

  m->pathLength = BuildPath(from, to, m->path, MAX_NODES);
  if (m->pathLength <= 1)
  {
    printf("Caminho inválido de %d para %d!\n", from, to);
    return;
  }

  messageCount++;
}

void UpdateAsyncMessages(float dt)
{
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == DONE)
      continue;

    m->progress += dt * MESSAGE_SPEED;

    if (m->progress >= 1.0f)
    {
      m->progress = 0;

      if (m->state == SENDING)
      {
        m->currentSegment++;
        if (m->currentSegment >= m->pathLength - 1)
        {
          if ((float)rand() / RAND_MAX < PROB_SUCCESS)
          {
            m->state = ACK_RECEIVING;
            m->ackSegment = m->pathLength - 2;
          }
          else
          {
            m->currentSegment = 0;
          }
        }
      }
      else if (m->state == ACK_RECEIVING)
      {
        m->ackSegment--;
        if (m->ackSegment < 0)
        {
          if ((float)rand() / RAND_MAX < PROB_SUCCESS)
          {
            m->state = DONE;
          }
          else
          {
            m->state = SENDING;
            m->currentSegment = 0;
          }
        }
      }
    }
  }
}

void DrawAsyncMessages()
{
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == DONE)
      continue;

    Vector2 start, end, pos;
    Color color;

    if (m->state == SENDING)
    {
      int a = m->path[m->currentSegment];
      int b = m->path[m->currentSegment + 1];
      start = (Vector2){nodes[a].x, nodes[a].y};
      end = (Vector2){nodes[b].x, nodes[b].y};
      color = RED;
    }
    else // ACK_RECEIVING
    {
      int a = m->path[m->ackSegment + 1];
      int b = m->path[m->ackSegment];
      start = (Vector2){nodes[a].x, nodes[a].y};
      end = (Vector2){nodes[b].x, nodes[b].y};
      color = GREEN;
    }

    pos = (Vector2){
        start.x + (end.x - start.x) * m->progress,
        start.y + (end.y - start.y) * m->progress};
    DrawCircleV(pos, 8, color);
  }
}

// ================== INTERFACE E LOOP PRINCIPAL ==================

// A função agora recebe a área que ela deve ocupar
void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed)
{
  static char fromText[8] = "0";
  static char toText[8] = "1";
  static char msgText[8] = "1";
  static int activeBox = -1;

  // Desenha o fundo e a borda da UI
  DrawRectangleRec(uiArea, (Color){200, 200, 200, 180});
  DrawRectangleLinesEx(uiArea, 2, DARKGRAY);

  int startX = uiArea.x + 10;
  int startY = uiArea.y + 10;
  int boxW = 60, boxH = 30, spacing = 40;

  DrawText("Origem:", startX, startY, 20, DARKGRAY);
  DrawText("Destino:", startX, startY + spacing, 20, DARKGRAY);
  DrawText("Qtd Msgs:", startX, startY + 2 * spacing, 20, DARKGRAY);

  Rectangle boxFrom = {startX + 100, startY, boxW, boxH};
  Rectangle boxTo = {startX + 100, startY + spacing, boxW, boxH};
  Rectangle boxMsg = {startX + 100, startY + 2 * spacing, boxW, boxH};

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
  {
    if (CheckCollisionPointRec(GetMousePosition(), boxFrom))
      activeBox = 0;
    else if (CheckCollisionPointRec(GetMousePosition(), boxTo))
      activeBox = 1;
    else if (CheckCollisionPointRec(GetMousePosition(), boxMsg))
      activeBox = 2;
    else if (!CheckCollisionPointRec(GetMousePosition(), uiArea))
      activeBox = -1;
  }

  if (activeBox != -1)
  {
    char *target = NULL;
    if (activeBox == 0)
      target = fromText;
    if (activeBox == 1)
      target = toText;
    if (activeBox == 2)
      target = msgText;

    SetMouseCursor(MOUSE_CURSOR_IBEAM);
    int key = GetCharPressed();
    while (key > 0)
    {
      if (key >= '0' && key <= '9' && (strlen(target) < 7))
      {
        int len = strlen(target);
        target[len] = (char)key;
        target[len + 1] = '\0';
      }
      key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE))
    {
      int len = strlen(target);
      if (len > 0)
        target[len - 1] = '\0';
    }
  }
  else
  {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }

  DrawRectangleLinesEx(boxFrom, 2, (activeBox == 0) ? RED : DARKGRAY);
  DrawText(fromText, boxFrom.x + 5, boxFrom.y + 5, 20, BLACK);
  DrawRectangleLinesEx(boxTo, 2, (activeBox == 1) ? RED : DARKGRAY);
  DrawText(toText, boxTo.x + 5, boxTo.y + 5, 20, BLACK);
  DrawRectangleLinesEx(boxMsg, 2, (activeBox == 2) ? RED : DARKGRAY);
  DrawText(msgText, boxMsg.x + 5, boxMsg.y + 5, 20, BLACK);

  Rectangle btn = {startX, startY + 3 * spacing, boxW + 100, boxH};
  DrawRectangleRec(btn, LIGHTGRAY);
  DrawRectangleLinesEx(btn, 2, DARKGRAY);
  DrawText("Enviar Mensagens", btn.x + 10, btn.y + 5, 20, BLACK);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), btn))
  {
    *uiFromNode = atoi(fromText);
    *uiToNode = atoi(toText);
    *uiMsgCount = atoi(msgText);
    *sendPressed = true;
  }
}

int main(void)
{
  InitWindow(1280, 720, "Simulador de Rede Assíncrono");
  SetTargetFPS(60);
  srand(time(NULL));

  // Variáveis para a UI
  int uiFromNode = 0, uiToNode = 1, uiMsgCount = 1;
  bool sendPressed = false;

  // Variáveis para o sistema de envio escalonado
  int messagesToSend = 0;
  float messageSendTimer = 0.0f;
  int messageFromNode = -1;
  int messageToNode = -1;

  // Variável para conexão de nós
  int nodeToConnect = -1;

  while (!WindowShouldClose())
  {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();

    // FIX 1: Calcular a área da UI ANTES de processar o input.
    int screenW = GetScreenWidth();
    int boxW = 60, spacing = 40, boxH = 30;
    Rectangle uiArea = {screenW - boxW - 120 - 20, 10, boxW + 120 + 20, 3 * spacing + boxH + 20};

    // --- LÓGICA DE ATUALIZAÇÃO ---

    // FIX 2: Lógica para enviar mensagens em lote de forma escalonada
    if (sendPressed)
    {
      sendPressed = false; // Apenas rearma o gatilho
      // Inicia a fila de envio
      messagesToSend = uiMsgCount;
      messageFromNode = uiFromNode;
      messageToNode = uiToNode;
      messageSendTimer = 0.0f; // Zera o timer para enviar a primeira imediatamente
    }

    if (messagesToSend > 0)
    {
      messageSendTimer += dt;
      if (messageSendTimer >= MESSAGE_INTERVAL)
      {
        if (messageFromNode != messageToNode && messageFromNode < nodeCount && messageToNode < nodeCount)
        {
          AddAsyncMessage(messageFromNode, messageToNode);
        }
        messagesToSend--;
        messageSendTimer = 0.0f;
      }
    }

    UpdateAsyncMessages(dt);

    // --- LÓGICA DE ENTRADA (INPUT) ---

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      // Agora a verificação funciona, pois uiArea já foi calculada.
      if (!CheckCollisionPointRec(mouse, uiArea))
      {
        AddNode(mouse.x, mouse.y);
        PushAction(ACTION_ADD_NODE, nodeCount - 1, -1);
      }
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
      int clickedNode = -1;
      for (int i = 0; i < nodeCount; i++)
      {
        if (CheckCollisionPointCircle(mouse, (Vector2){nodes[i].x, nodes[i].y}, NODE_RADIUS))
        {
          clickedNode = i;
          break;
        }
      }

      if (clickedNode != -1)
      {
        if (nodeToConnect == -1)
        {
          nodeToConnect = clickedNode;
        }
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
        nodeToConnect = -1; // Clicar fora de um nó cancela a conexão
      }
    }

    if (IsKeyPressed(KEY_Q))
      CreateDefaultNetwork();
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
      UndoAction();
    if (IsKeyPressed(KEY_W))
      nodeToConnect = -1;

    // --- SEÇÃO DE DESENHO ---

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawNetwork();
    DrawAsyncMessages();

    // Passa a área da UI para a função de desenho
    DrawUI(uiArea, &uiFromNode, &uiToNode, &uiMsgCount, &sendPressed);

    DrawText("ESQ: Adicionar | DIR: Conectar", 10, 10, 20, DARKGRAY);
    DrawText("Q: Rede Padrão | W: Limpar seleção | CTRL+Z: Desfazer", 10, 40, 20, DARKGRAY);
    if (nodeToConnect != -1)
    {
      char buffer[64];
      sprintf(buffer, "Conectar nó %d com...", nodeToConnect);
      DrawText(buffer, 10, 70, 20, RED);
      DrawLine(nodes[nodeToConnect].x, nodes[nodeToConnect].y, mouse.x, mouse.y, DARKGRAY);
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}