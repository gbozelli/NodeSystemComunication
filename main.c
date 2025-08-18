#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdlib.h> // para rand()
#include <time.h>   // para srand

#define PROB_SUCCESS 0.1f // 80% de chance de mensagem chegar
#define MAX_NODES 50
#define NODE_RADIUS 20
#define MAX_CONNECTIONS 10

typedef struct Node
{
  float x, y;
  int id;
  int connections[MAX_CONNECTIONS];
  int connectionCount;
} Node;

Node nodes[MAX_NODES];
int nodeCount = 0;

typedef struct Message
{
  int from;
  int to;
  int id;
  bool ackReceived;
  float timer;   // tempo decorrido desde envio
  float timeout; // limite para reenviar
} Message;

typedef enum ActionType
{
  ACTION_ADD_NODE,
  ACTION_CONNECT_NODES
} ActionType;

typedef struct Action
{
  ActionType type;
  int nodeA;
  int nodeB; // Para conexão, ou -1 para adição de nó
} Action;

Action actionStack[100];
int actionTop = -1;

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
    return; // Nada para desfazer

  Action act = actionStack[actionTop--];

  if (act.type == ACTION_ADD_NODE)
  {
    // Remove o último nó adicionado
    if (nodeCount > 0)
      nodeCount--;
  }
  else if (act.type == ACTION_CONNECT_NODES)
  {
    // Remove a conexão
    int a = act.nodeA;
    int b = act.nodeB;
    // Remover de A
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
    // Remover de B
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

// Adiciona nó na posição x, y
void AddNode(float x, float y)
{
  if (nodeCount >= MAX_NODES)
    return;
  nodes[nodeCount].id = nodeCount;
  nodes[nodeCount].x = x;
  nodes[nodeCount].y = y;
  nodeCount++;
}

// Desenha os nós e conexões
void DrawNetwork()
{
  // Conexões
  for (int i = 0; i < nodeCount; i++)
  {
    for (int j = 0; j < nodes[i].connectionCount; j++)
    {
      int b = nodes[i].connections[j];
      if (b > i) // evita duplicar a linha (desenha só uma vez)
        DrawLine(nodes[i].x, nodes[i].y,
                 nodes[b].x, nodes[b].y, GRAY);
    }
  }

  // Nós
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

  // Adiciona conexão em A → B se não existir
  int exists = 0;
  for (int i = 0; i < nodes[a].connectionCount; i++)
    if (nodes[a].connections[i] == b)
      exists = 1;
  if (!exists)
    nodes[a].connections[nodes[a].connectionCount++] = b;

  // Adiciona conexão em B → A se não existir
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

  // reconstruir caminho
  int temp[MAX_NODES];
  int len = 0;
  int cur = goal;
  while (cur != -1)
  {
    temp[len++] = cur;
    cur = parent[cur];
  }

  // inverter
  for (int i = 0; i < len; i++)
  {
    path[i] = temp[len - i - 1];
  }
  return len;
}

void SendMessage(int fromId, int toId, int repetitions)
{
  int path[50];
  int pathLength = BuildPath(fromId, toId, path, 50);
  if (pathLength == -1)
  {
    printf("Caminho não encontrado!\n");
    return;
  }

  for (int r = 0; r < repetitions; r++)
  {
    for (int i = 0; i < pathLength - 1; i++)
    {
      Vector2 start = {nodes[path[i]].x, nodes[path[i]].y};
      Vector2 end = {nodes[path[i + 1]].x, nodes[path[i + 1]].y};

      for (float t = 0; t <= 1; t += 0.02f)
      {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawNetwork();

        Vector2 pos = {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t};
        DrawCircleV(pos, 8, RED);

        EndDrawing();
      }
    }
  }
}

// === Função para criar rede pré-pronta ===
void CreateDefaultNetwork()
{
  // Limpa rede
  nodeCount = 0;

  // Adiciona alguns nós em posições fixas
  AddNode(150, 400); // nó 0
  AddNode(300, 350); // nó 1
  AddNode(300, 650); // nó 2
  AddNode(450, 400); // nó 3
  AddNode(600, 350); // nó 4
  AddNode(600, 650); // nó 5

  // Conexões
  ConnectNodes(0, 1);
  ConnectNodes(0, 2);
  ConnectNodes(1, 3);
  ConnectNodes(2, 3);
  ConnectNodes(3, 4);
  ConnectNodes(3, 5);
}

void DrawUI(int *fromNode, int *toNode, int *msgCount, bool *sendPressed, int *mode)
{
  static char fromText[8] = "0";
  static char toText[8] = "1";
  static char msgText[8] = "1";

  static int activeBox = -1; // -1 = nenhum, 0 = origem, 1 = destino, 2 = qtd msgs

  Vector2 mouse = GetMousePosition();

  // === Labels ===
  // === Pega largura e altura da tela ===
  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();

  // === Margens ===
  int marginX = 20;
  int marginY = 20;

  // === Dimensões da caixa ===
  int boxW = 60;
  int boxH = 30;

  // === Altura entre cada campo ===
  int spacing = 40;

  // === Posição inicial (primeira caixa) ===
  int startX = screenW - boxW - 100 - marginX; // 100 ~ largura estimada do texto
  int startY = screenH - (3 * boxH + 2 * spacing) - marginY;

  // === Textos ===
  DrawText("Origem:", startX, startY, 20, DARKGRAY);
  DrawText("Destino:", startX, startY + spacing, 20, DARKGRAY);
  DrawText("Qtd Msgs:", startX, startY + 2 * spacing, 20, DARKGRAY);

  // === Input boxes ===
  Rectangle boxFrom = {startX + 100, startY, boxW, boxH};
  Rectangle boxTo = {startX + 100, startY + spacing, boxW, boxH};
  Rectangle boxMsg = {startX + 100, startY + 2 * spacing, boxW, boxH};

  // Clique para ativar caixa
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
  {
    if (CheckCollisionPointRec(mouse, boxFrom))
      activeBox = 0;
    else if (CheckCollisionPointRec(mouse, boxTo))
      activeBox = 1;
    else if (CheckCollisionPointRec(mouse, boxMsg))
      activeBox = 2;
    else
      activeBox = -1;
  }

  // Captura entrada de texto
  int key = GetCharPressed();
  while (key > 0)
  {
    char *target = NULL;
    if (activeBox == 0)
      target = fromText;
    if (activeBox == 1)
      target = toText;
    if (activeBox == 2)
      target = msgText;

    if (target)
    {
      int len = strlen(target);
      if (key >= 48 && key <= 57 && len < 7) // só números
      {
        target[len] = (char)key;
        target[len + 1] = '\0';
      }
    }
    key = GetCharPressed();
  }

  // Backspace
  if (IsKeyPressed(KEY_BACKSPACE) && activeBox != -1)
  {
    char *target = NULL;
    if (activeBox == 0)
      target = fromText;
    if (activeBox == 1)
      target = toText;
    if (activeBox == 2)
      target = msgText;

    int len = strlen(target);
    if (len > 0)
      target[len - 1] = '\0';
  }



  // === Desenha caixas com borda ===
  DrawRectangleLinesEx(boxFrom, 2, (activeBox == 0) ? RED : DARKGRAY);
  DrawRectangleLinesEx(boxTo, 2, (activeBox == 1) ? RED : DARKGRAY);
  DrawRectangleLinesEx(boxMsg, 2, (activeBox == 2) ? RED : DARKGRAY);

  // === Desenha textos dentro das caixas ===
  DrawText(fromText, boxFrom.x + 5, boxFrom.y + 5, 20, BLACK);
  DrawText(toText, boxTo.x + 5, boxTo.y + 5, 20, BLACK);
  DrawText(msgText, boxMsg.x + 5, boxMsg.y + 5, 20, BLACK);

  // === Labels ===
  DrawText("Origem:", startX, startY, 20, DARKGRAY);
  DrawText("Destino:", startX, startY + spacing, 20, DARKGRAY);
  DrawText("Qtd Msgs:", startX, startY + 2 * spacing, 20, DARKGRAY);

  // === Botão Enviar ===
  Rectangle btn = {startX, startY + 3 * spacing, boxW + 100, boxH};
  DrawRectangleRec(btn, LIGHTGRAY);
  DrawRectangleLinesEx(btn, 2, DARKGRAY);
  DrawText("Enviar Mensagens", btn.x + 10, btn.y + 5, 20, BLACK);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, btn))
  {
    *fromNode = atoi(fromText);
    *toNode = atoi(toText);
    *msgCount = atoi(msgText);
    *sendPressed = true;
  }
  // Área do painel da UI (cobre labels, caixas e botão)
  Rectangle uiArea;
  uiArea.x = startX;
  uiArea.y = startY;
  uiArea.width = boxW + 120;               // largura total aproximada do painel
  uiArea.height = 3 * spacing + boxH + 50; // altura total incluindo botão


  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mode == 0)
  {
    if (!CheckCollisionPointRec(mouse, uiArea)) // só cria nó fora da UI
    {
      AddNode(mouse.x, mouse.y);
      PushAction(ACTION_ADD_NODE, nodeCount - 1, -1);
    }
  }
}

void SendMessageWithAckAnim(int fromId, int toId)
{
  int steps = 50; // número de frames da animação
  Vector2 start = {nodes[fromId].x, nodes[fromId].y};
  Vector2 end = {nodes[toId].x, nodes[toId].y};

  // --- Mensagem indo (vermelha) ---
  for (int i = 0; i <= steps; i++)
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawNetwork();

    float t = (float)i / steps;
    Vector2 pos = {start.x + (end.x - start.x) * t,
                   start.y + (end.y - start.y) * t};
    DrawCircleV(pos, 8, RED); // mensagem
    EndDrawing();
  }

  // --- ACK voltando (verde) ---
  for (int i = 0; i <= steps; i++)
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawNetwork();

    float t = (float)i / steps;
    Vector2 pos = {end.x + (start.x - end.x) * t,
                   end.y + (start.y - end.y) * t};
    DrawCircleV(pos, 6, GREEN); // ACK menor
    EndDrawing();
  }
}

void SendMessagesWithAck(int fromNode, int toNode, int msgCount)
{
  Message messages[msgCount];
  for (int i = 0; i < msgCount; i++)
  {
    messages[i].from = fromNode;
    messages[i].to = toNode;
    messages[i].id = i;
    messages[i].ackReceived = false;
    messages[i].timer = 0;
    messages[i].timeout = 1.0f; // 1 segundo para ACK
  }

  int currentMsg = 0;
  float dt = 0.016f; // ~60FPS
  while (currentMsg < msgCount)
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawNetwork();

    Message *msg = &messages[currentMsg];

    // === Só envia se ainda não recebeu ACK ===
    if (!msg->ackReceived)
    {
      msg->timer += dt;
      SendMessage(msg->from, msg->to,1);
      // Probabilidade de a mensagem chegar
      if ((float)rand() / RAND_MAX < PROB_SUCCESS)
      {
        // mensagem chegou: desenha animada
        SendMessageWithAckAnim(msg->from, msg->to);

        // ACK instantâneo
        msg->ackReceived = true;
        currentMsg++;
      }
      else if (msg->timer >= msg->timeout)
      {
        // Timeout: reenviar
        msg->timer = 0;
      }
    }

    EndDrawing();
    // Atualiza tempo real (simula dt)
    // Pode usar Sleep(dt*1000) se quiser desacelerar visual
  }
}

int main(void)
{
  InitWindow(800, 700, "Simulador de Rede - Raylib");
  SetTargetFPS(60);
  int mode = 0;
  int connectMode = 0;
  int fromNode = -1, toNode = -1;
  int msgCount = 0;
  bool sendPressed = false;

  while (!WindowShouldClose())
  {
    
    // --- Calcula área da UI (mesmo que em DrawUI) ---
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int boxW = 60, boxH = 30, spacing = 40;
    int startX = screenW - boxW - 100 - 20;
    int startY = screenH - (3 * boxH + 2 * spacing + 40) - 20;
    Rectangle uiArea = {startX, startY, boxW + 120, 3 * spacing + boxH + 50};

    // --- Desenha fundo cinza semi-transparente ---
    DrawRectangleRec(uiArea, (Color){200, 200, 200, 180}); // cinza claro com alpha 180

    // --- Desenha borda para destacar ---
    DrawRectangleLinesEx(uiArea, 2, DARKGRAY);

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      if (!CheckCollisionPointRec(mouse, uiArea)) // só cria nó fora da UI
      {
        AddNode(mouse.x, mouse.y);
        PushAction(ACTION_ADD_NODE, nodeCount - 1, -1);
      }
    }

    // --- Depois desenha a UI normalmente ---
    DrawUI(&fromNode, &toNode, &msgCount, &sendPressed, &mode);

    // Clique esquerdo para adicionar nó
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mode == 0)
    {
      Vector2 mouse = GetMousePosition();
      if (!CheckCollisionPointRec(mouse, uiArea)) // só cria nó fora da UI
      {
        AddNode(mouse.x, mouse.y);
        PushAction(ACTION_ADD_NODE, nodeCount - 1, -1);
      }
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
    {
      UndoAction();
    }

    if (IsKeyDown(KEY_Q))
    {
      CreateDefaultNetwork();
    }

      // Clique direito para selecionar nó
      if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
      {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < nodeCount; i++)
        {
          float dist = sqrtf((mouse.x - nodes[i].x) * (mouse.x - nodes[i].x) + (mouse.y - nodes[i].y) * (mouse.y - nodes[i].y));
          if (dist <= NODE_RADIUS)
          {
            if (mode == 0)
            {
              // modo normal conecta nós (sua lógica original)
              if (connectMode == 0)
              {
                fromNode = i;
                connectMode = 1;
              }
              else
              {
                toNode = i;
                ConnectNodes(fromNode, toNode);
                PushAction(ACTION_CONNECT_NODES, fromNode, toNode);
                fromNode = toNode;
                toNode = -1;
              }
            }
            else
            {
              // modo mensagem: seleciona origem e destino para enviar mensagem
              if (fromNode == -1)
              {
                fromNode = i;
              }
              else if (toNode == -1 && i != fromNode)
              {
                toNode = i;
                SendMessagesWithAck(fromNode, toNode, 1);
                fromNode = -1;
                toNode = -1;
              }
            }
            break;
          }
        }
      }

    // Tecla 'S' para enviar mensagem do último par selecionado
    if (IsKeyPressed(KEY_M))
    {
      mode = 1 - mode; // alterna entre 0 e 1
      fromNode = -1;
      toNode = -1;
      connectMode = 0;
    }
    if (mode == 1 && IsKeyPressed(KEY_S) && fromNode != -1 && toNode != -1)
    {
      SendMessagesWithAck(fromNode, toNode, 1);
    }
    if (IsKeyPressed(KEY_W))
    {
      fromNode = -1, toNode = -1;
      connectMode = 0;
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (mode == 0)
    {
      DrawText("Modo NORMAL", 10, 10, 20, DARKGRAY);
      DrawText("Clique ESQUERDO: Adicionar nó", 10, 40, 20, DARKGRAY);
      DrawText("Clique DIREITO: Conectar nós (clique nos nós que serão conectados)", 10, 70, 20, DARKGRAY);
    }
    else
    {

      char buff[64];
      if (fromNode != -1)
      {
        sprintf(buff, "Nó Origem selecionado: %d", fromNode);
        DrawText(buff, 10, 100, 20, RED);
      }
      else
      {
        DrawText("Nó Origem selecionado: nenhum", 10, 100, 20, RED);
      }
    }

    DrawText("Pressione W: Limpar seleção", 10, 130, 20, DARKGRAY);

    char fromText[64];
    if (fromNode != -1)
      sprintf(fromText, "Nó selecionado: %d", fromNode);
    else
      strcpy(fromText, "Nó selecionado: nenhum");
    DrawText(fromText, 10, 160, 20, RED);

    DrawNetwork();
    if (sendPressed)
    {
      sendPressed = false;
      if (fromNode >= 0 && toNode >= 0 && fromNode < nodeCount && toNode < nodeCount)
        SendMessagesWithAck(fromNode, toNode, msgCount);
    }
    EndDrawing();
  }

  CloseWindow();
  return 0;
}