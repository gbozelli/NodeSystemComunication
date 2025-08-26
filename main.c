#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PROB_SUCCESS 0.99f // Chance de sucesso alta para testar a lógica de bloqueio
#define MAX_NODES 50
#define NODE_RADIUS 20
#define MAX_CONNECTIONS 10
#define MAX_MESSAGES 100000
#define MESSAGE_SPEED 1.5f
#define MESSAGE_INTERVAL 0.2f
#define QUEUE_OFFSET_X 25 // Distância X do nó para a fila
#define QUEUE_OFFSET_Y 15 // Distância Y entre mensagens na fila

// NOVO: A struct Network será usada para rastrear o tráfego direcional
typedef struct Network
{
  int graph[MAX_NODES][MAX_NODES];
} Network;

Network busyNetwork; // Instância global para o nosso placar de tráfego

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
  DONE,
  QUEUED,
} MsgState;

// ALTERADO: A struct da mensagem precisa de um caminho separado para o ACK
typedef struct AsyncMessage
{
  int from;
  int to;
  MsgState state;
  float progress;

  int currentSegment;
  int path[MAX_NODES];
  int pathLength;

  int currentAckSegment;
  int ackPath[MAX_NODES];
  int ackPathLength;
  int queuedAtNodeId;

} AsyncMessage;

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

// --- Lógica Principal da Rede ---

int BuildPath(int start, int goal, int *path, int maxLen, Network *currentTraffic)
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
      // Para ir de 'current' para 'next', a "pista" oposta ('next' para 'current') deve estar livre.
      if (!visited[next] && currentTraffic->graph[next][current] == 0)
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
  memset(&busyNetwork, 0, sizeof(Network));
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

void AddAsyncMessage(int from, int to)
{
  if (messageCount >= MAX_MESSAGES)
    return;
  AsyncMessage *m = &messages[messageCount];
  m->pathLength = BuildPath(from, to, m->path, MAX_NODES, &busyNetwork);
  if (m->pathLength <= 1)
  {
    printf("FALHA INICIAL: Nao ha caminho livre de %d para %d!\n", from, to);
    return;
  }
  for (int i = 0; i < m->pathLength - 1; i++)
  {
    int nodeA = m->path[i];
    int nodeB = m->path[i + 1];
    busyNetwork.graph[nodeA][nodeB]++;
  }
  m->from = from;
  m->to = to;
  m->state = SENDING;
  m->progress = 0;
  m->currentSegment = 0;
  m->ackPathLength = 0;
  m->queuedAtNodeId = -1;
  messageCount++;
}
// ALTERADO: Lógica de atualização completamente refeita para corrigir o bug de "vazamento de recursos".
// Agora os caminhos são liberados corretamente ao final de cada etapa.
void UpdateAsyncMessages(float dt)
{
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == DONE)
      continue;

    // 1. LÓGICA PARA MENSAGENS NA FILA (TENTATIVA DE SAÍDA)
    if (m->state == QUEUED)
    {
      int nextHopNode = -1;
      // Determina o próximo salto se for uma msg de ida ou um ACK na fila
      if (m->ackPathLength > 0)
      {                        // É um ACK que está na fila
        nextHopNode = m->from; // O destino final de um ACK é a origem da msg
      }
      else
      { // É uma mensagem de ida
        nextHopNode = m->path[m->currentSegment + 1];
      }

      int tempPath[MAX_NODES];
      int pathLen = BuildPath(m->queuedAtNodeId, nextHopNode, tempPath, MAX_NODES, &busyNetwork);

      if (pathLen > 1)
      {
        // Caminho encontrado! Sai da fila.
        printf("Mensagem %d saindo da fila no no %d.\n", i, m->queuedAtNodeId);
        m->state = (m->ackPathLength > 0) ? ACK_RECEIVING : SENDING;
        m->queuedAtNodeId = -1;
        m->progress = 0;

        // Reserva o primeiro segmento do novo caminho encontrado
        busyNetwork.graph[tempPath[0]][tempPath[1]]++;
      }
      continue; // Pula para a próxima mensagem
    }

    // 2. LÓGICA PARA MENSAGENS EM TRÂNSITO
    m->progress += dt * MESSAGE_SPEED;

    if (m->progress >= 1.0f)
    {
      m->progress -= 1.0f; // Usar -= para manter o progresso "que sobrou"

      if (m->state == SENDING)
      {
        m->currentSegment++;
        int currentNodeId = m->path[m->currentSegment];

        // Chegou ao destino final?
        if (currentNodeId == m->to)
        {
          // Libera o caminho de IDA que acabou de ser usado
          for (int j = 0; j < m->pathLength - 1; j++)
          {
            int nodeA = m->path[j];
            int nodeB = m->path[j + 1];
            if (busyNetwork.graph[nodeA][nodeB] > 0)
              busyNetwork.graph[nodeA][nodeB]--;
          }

          // Tenta encontrar um caminho de volta para o ACK
          m->ackPathLength = BuildPath(m->to, m->from, m->ackPath, MAX_NODES, &busyNetwork);
          if (m->ackPathLength > 1)
          {
            for (int j = 0; j < m->ackPathLength - 1; j++)
              busyNetwork.graph[m->ackPath[j]][m->ackPath[j + 1]]++; // Reserva caminho do ACK
            m->state = ACK_RECEIVING;
            m->currentAckSegment = 0;
          }
          else
          { // Não achou caminho, fica na fila no destino
            m->state = QUEUED;
            m->queuedAtNodeId = m->to;
            printf("Mensagem %d enfileirada no destino %d (sem rota de ACK).\n", i, m->to);
          }
        }
      }
      else if (m->state == ACK_RECEIVING)
      {
        m->currentAckSegment++;
        int currentNodeId = m->ackPath[m->currentAckSegment];

        // ACK chegou na origem?
        if (currentNodeId == m->from)
        {
          // Libera o caminho de VOLTA (ACK) que acabou de ser usado
          for (int j = 0; j < m->ackPathLength - 1; j++)
          {
            int nodeA = m->ackPath[j];
            int nodeB = m->ackPath[j + 1];
            if (busyNetwork.graph[nodeA][nodeB] > 0)
              busyNetwork.graph[nodeA][nodeB]--;
          }
          m->state = DONE; // Fim da transação
        }
      }
    }
  }
}

void DrawTravelingMessages()
{
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state != SENDING && m->state != ACK_RECEIVING)
      continue;

    Vector2 start, end, pos;
    Color color;
    if (m->state == SENDING)
    {
      start = (Vector2){nodes[m->path[m->currentSegment]].x, nodes[m->path[m->currentSegment]].y};
      end = (Vector2){nodes[m->path[m->currentSegment + 1]].x, nodes[m->path[m->currentSegment + 1]].y};
      color = RED;
    }
    else
    {
      start = (Vector2){nodes[m->ackPath[m->currentAckSegment]].x, nodes[m->ackPath[m->currentAckSegment]].y};
      end = (Vector2){nodes[m->ackPath[m->currentAckSegment + 1]].x, nodes[m->ackPath[m->currentAckSegment + 1]].y};
      color = GREEN;
    }
    pos = (Vector2){start.x + (end.x - start.x) * m->progress, start.y + (end.y - start.y) * m->progress};
    DrawCircleV(pos, 8, color);
    DrawCircleLines(pos.x, pos.y, 8, BLACK); // Borda preta
  }
}

// NOVA FUNÇÃO para desenhar as mensagens na fila
void DrawQueuedMessages()
{
  int queueCounts[MAX_NODES] = {0}; // Zera o contador de fila para cada nó
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == QUEUED)
    {
      int nodeId = m->queuedAtNodeId;
      if (nodeId != -1)
      {
        Node *node = &nodes[nodeId];
        Vector2 pos = {
            node->x + QUEUE_OFFSET_X,
            node->y - (NODE_RADIUS) + (queueCounts[nodeId] * QUEUE_OFFSET_Y)};

        // Determina a cor (vermelho se indo, verde se era um ACK)
        Color color = (m->ackPathLength > 0) ? GREEN : RED;

        DrawCircleV(pos, 7, color);
        DrawCircleLines(pos.x, pos.y, 7, BLACK); // Borda preta

        queueCounts[nodeId]++; // Incrementa a posição na pilha para este nó
      }
    }
  }
}

void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed)
{
  static char fromText[8] = "0";
  static char toText[8] = "3";
  static char msgText[8] = "20";
  static int activeBox = -1;
  DrawRectangleRec(uiArea, (Color){200, 200, 200, 180});
  DrawRectangleLinesEx(uiArea, 2, DARKGRAY);
  int startX = uiArea.x + 10;
  int startY = uiArea.y + 10;
  int boxW = 60, boxH = 30, spacing = 40;
  DrawText("Origem:", startX, startY, 20, DARKGRAY);
  DrawText("Destino:", startX, startY + spacing, 20, DARKGRAY);
  DrawText("Qtd:", startX, startY + 2 * spacing, 20, DARKGRAY);
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
  DrawText("Enviar", btn.x + 10, btn.y + 5, 20, BLACK);
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
  InitWindow(1280, 720, "Simulador de Rede com Fila nos Nos");
  SetTargetFPS(60);
  srand(time(NULL));

  memset(&busyNetwork, 0, sizeof(Network));
  int uiFromNode = 0, uiToNode = 3, uiMsgCount = 5;
  bool sendPressed = false;
  int messagesToSend = 0;
  float messageSendTimer = 0.0f;
  int messageFromNode = -1, messageToNode = -1;
  int nodeToConnect = -1;

  while (!WindowShouldClose())
  {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();
    int screenW = GetScreenWidth();
    int boxW = 60, spacing = 40, boxH = 30;
    Rectangle uiArea = {screenW - boxW - 120 - 20, 10, boxW + 120 + 20, 3 * spacing + boxH + 20};

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
        if (messageFromNode != messageToNode && messageFromNode < nodeCount && messageToNode < nodeCount)
          AddAsyncMessage(messageFromNode, messageToNode);
        messagesToSend--;
        messageSendTimer = 0.0f;
      }
    }

    UpdateAsyncMessages(dt);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
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
        if (CheckCollisionPointCircle(mouse, (Vector2){nodes[i].x, nodes[i].y}, NODE_RADIUS))
        {
          clickedNode = i;
          break;
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
        nodeToConnect = -1;
      }
    }

    if (IsKeyPressed(KEY_Q))
      CreateDefaultNetwork();
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
      UndoAction();
    if (IsKeyPressed(KEY_W))
      nodeToConnect = -1;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawNetwork();
    DrawTravelingMessages();
    DrawQueuedMessages(); // Desenha as mensagens na fila

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
