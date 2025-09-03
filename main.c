#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//====================================================================================
// DEFINIÇÕES E CONSTANTES GLOBAIS
//====================================================================================
#define MAX_NODES 50
#define NODE_RADIUS 20
#define MAX_CONNECTIONS 10
#define MAX_MESSAGES 1000000
#define MESSAGE_SPEED 1.5f
#define MESSAGE_INTERVAL 0.2f
#define QUEUE_OFFSET_X 25

#define MAX_CAPACITY_PER_LINK 20
#define TIMEOUT_SECONDS 10.0f

//====================================================================================
// ESTRUTURAS DE DADOS
//====================================================================================

typedef struct Network
{
  int graph[MAX_NODES][MAX_NODES];
} Network;
typedef struct Node
{
  float x, y;
  int id;
  int connections[MAX_CONNECTIONS];
  int connectionCount;
} Node;
typedef enum MsgState
{
  SENDING,
  ACK_RECEIVING,
  DONE,
  QUEUED
} MsgState;

typedef struct AsyncMessage
{
  int from, to;
  MsgState state;
  float progress;
  int currentSegment, path[MAX_NODES], pathLength;
  int currentAckSegment, ackPath[MAX_NODES], ackPathLength;
  int queuedAtNodeId;
  clock_t creation_time, last_sent_time, completion_time;
  int retransmission_count;
} AsyncMessage;

typedef enum ActionType
{
  ACTION_ADD_NODE,
  ACTION_CONNECT_NODES
} ActionType;
typedef struct Action
{
  ActionType type;
  int nodeA, nodeB;
} Action;

//====================================================================================
// VARIÁVEIS GLOBAIS
//====================================================================================
Node nodes[MAX_NODES];
int nodeCount = 0;
AsyncMessage messages[MAX_MESSAGES];
int messageCount = 0;

// --- DUAS REDES SEPARADAS PARA CONTROLE ---
Network pathfindingNetwork; // Para BuildPath usar a regra da pista oposta == 0
Network capacityNetwork;    // Para contar mensagens e checar capacidade

Action actionStack[100];
int actionTop = -1;

long long total_latency_ticks = 0;
int completed_messages_count = 0;
int total_retransmissions = 0;

//====================================================================================
// FUNÇÕES DE GERENCIAMENTO DA REDE
//====================================================================================

void PushAction(ActionType type, int a, int b)
{
  if (actionTop < 99)
  {
    actionTop++;
    actionStack[actionTop] = (Action){type, a, b};
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

// Usa a 'pathfindingNetwork' com a regra estrita da pista oposta.
int BuildPath(int start, int goal, int *path, int maxLen)
{
  int visited[MAX_NODES] = {0};
  int parent[MAX_NODES];
  for (int i = 0; i < MAX_NODES; i++)
    parent[i] = -1;
  int queue[MAX_NODES], front = 0, rear = 0;
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
      if (!visited[next] && pathfindingNetwork.graph[next][current] == 0)
      {
        visited[next] = 1;
        parent[next] = current;
        queue[rear++] = next;
      }
    }
  }
  if (!found)
    return -1;
  int temp[MAX_NODES], len = 0;
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
  memset(&pathfindingNetwork, 0, sizeof(Network));
  memset(&capacityNetwork, 0, sizeof(Network));
  total_latency_ticks = 0;
  completed_messages_count = 0;
  total_retransmissions = 0;

  AddNode(450, 360);
  AddNode(300, 200);
  AddNode(300, 520);
  AddNode(600, 200);
  AddNode(600, 520);
  AddNode(120, 120);
  AddNode(120, 360);
  AddNode(120, 600);
  AddNode(780, 120);
  AddNode(780, 360);
  AddNode(780, 600);
  AddNode(450, 50);
  AddNode(450, 670);
  AddNode(950, 360);

  ConnectNodes(0, 1);
  ConnectNodes(0, 2);
  ConnectNodes(0, 3);
  ConnectNodes(0, 4);
  ConnectNodes(1, 2);
  ConnectNodes(2, 4);
  ConnectNodes(4, 3);
  ConnectNodes(3, 1);
  ConnectNodes(1, 5);
  ConnectNodes(1, 6);
  ConnectNodes(2, 6);
  ConnectNodes(2, 7);
  ConnectNodes(5, 6);
  ConnectNodes(6, 7);
  ConnectNodes(3, 8);
  ConnectNodes(3, 9);
  ConnectNodes(4, 9);
  ConnectNodes(4, 10);
  ConnectNodes(8, 9);
  ConnectNodes(9, 10);
  ConnectNodes(11, 1);
  ConnectNodes(11, 3);
  ConnectNodes(12, 2);
  ConnectNodes(12, 4);
}

//====================================================================================
// LÓGICA PRINCIPAL DAS MENSAGENS
//====================================================================================

void AddAsyncMessage(int from, int to)
{
  if (messageCount >= MAX_MESSAGES)
    return;
  AsyncMessage *m = &messages[messageCount];
  *m = (AsyncMessage){.from = from, .to = to, .retransmission_count = 0, .creation_time = clock()};

  m->pathLength = BuildPath(from, to, m->path, MAX_NODES);

  if (m->pathLength > 1)
  {
    int first_hop_node = m->path[1];
    if (capacityNetwork.graph[from][first_hop_node] < MAX_CAPACITY_PER_LINK)
    {
      m->state = SENDING;
      m->queuedAtNodeId = -1;
      pathfindingNetwork.graph[from][first_hop_node]++;
      capacityNetwork.graph[from][first_hop_node]++;
      m->last_sent_time = clock();
    }
    else
    {
      m->state = QUEUED;
      m->queuedAtNodeId = from;
    }
  }
  else
  {
    m->state = QUEUED;
    m->queuedAtNodeId = from;
  }
  messageCount++;
}

void UpdateAsyncMessages(float dt, float releaseInterval)
{
  static float nodeReleaseCooldown[MAX_NODES] = {0.0f};
  for (int i = 0; i < nodeCount; i++)
    if (nodeReleaseCooldown[i] > 0)
      nodeReleaseCooldown[i] -= dt;
  clock_t now = clock();

  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == DONE)
      continue;

    if ((m->state != QUEUED || m->queuedAtNodeId != m->from) && m->last_sent_time > 0)
    {
      if (((double)(now - m->last_sent_time) / CLOCKS_PER_SEC) > TIMEOUT_SECONDS)
      {
        printf("!!! TIMEOUT da Mensagem %d (%d->%d) !!!\n", i, m->from, m->to);
        total_retransmissions++;

        if (m->state == SENDING)
        {
          int prevNode = m->path[m->currentSegment];
          int nextNode = m->path[m->currentSegment + 1];
          if (pathfindingNetwork.graph[prevNode][nextNode] > 0)
            pathfindingNetwork.graph[prevNode][nextNode]--;
          if (capacityNetwork.graph[prevNode][nextNode] > 0)
            capacityNetwork.graph[prevNode][nextNode]--;
        }
        else if (m->state == ACK_RECEIVING)
        {
          int prevNode = m->ackPath[m->currentAckSegment];
          int nextNode = m->ackPath[m->currentAckSegment + 1];
          if (pathfindingNetwork.graph[prevNode][nextNode] > 0)
            pathfindingNetwork.graph[prevNode][nextNode]--;
          if (capacityNetwork.graph[prevNode][nextNode] > 0)
            capacityNetwork.graph[prevNode][nextNode]--;
        }

        m->state = QUEUED;
        m->queuedAtNodeId = m->from;
        m->retransmission_count++;
        m->pathLength = 0;
        m->ackPathLength = 0;
        m->progress = 0;
        m->currentSegment = 0;
        continue;
      }
    }

    if (m->state == SENDING || m->state == ACK_RECEIVING)
      m->progress += dt * MESSAGE_SPEED;

    switch (m->state)
    {
    case SENDING:
      if (m->progress >= 1.0f)
      {
        m->progress -= 1.0f;
        int prevNodeId = m->path[m->currentSegment];
        m->currentSegment++;
        int currentNodeId = m->path[m->currentSegment];

        pathfindingNetwork.graph[prevNodeId][currentNodeId]--;
        capacityNetwork.graph[prevNodeId][currentNodeId]--;

        if (currentNodeId == m->to)
        {
          m->state = QUEUED;
          m->queuedAtNodeId = m->to;
        }
        else
        {
          int nextNodeId = m->path[m->currentSegment + 1];
          if (capacityNetwork.graph[currentNodeId][nextNodeId] < MAX_CAPACITY_PER_LINK)
          {
            pathfindingNetwork.graph[currentNodeId][nextNodeId]++;
            capacityNetwork.graph[currentNodeId][nextNodeId]++;
          }
          else
          {
            m->state = QUEUED;
            m->queuedAtNodeId = currentNodeId;
          }
        }
      }
      break;
    case ACK_RECEIVING:
      if (m->progress >= 1.0f)
      {
        m->progress -= 1.0f;
        int prevNodeId = m->ackPath[m->currentAckSegment];
        m->currentAckSegment++;
        int currentNodeId = m->ackPath[m->currentAckSegment];

        pathfindingNetwork.graph[prevNodeId][currentNodeId]--;
        capacityNetwork.graph[prevNodeId][currentNodeId]--;

        if (currentNodeId == m->from)
        {
          m->state = DONE;
          m->completion_time = clock();
          total_latency_ticks += (m->completion_time - m->creation_time);
          completed_messages_count++;
        }
        else
        {
          int nextNodeId = m->ackPath[m->currentAckSegment + 1];
          if (capacityNetwork.graph[currentNodeId][nextNodeId] < MAX_CAPACITY_PER_LINK)
          {
            pathfindingNetwork.graph[currentNodeId][nextNodeId]++;
            capacityNetwork.graph[currentNodeId][nextNodeId]++;
          }
          else
          {
            m->state = QUEUED;
            m->queuedAtNodeId = currentNodeId;
          }
        }
      }
      break;
    case QUEUED:
    {
      int nodeId = m->queuedAtNodeId;
      if (nodeId != -1 && nodeReleaseCooldown[nodeId] <= 0)
      {
        if (nodeId == m->from)
        {
          m->pathLength = BuildPath(m->from, m->to, m->path, MAX_NODES);
          if (m->pathLength > 1)
          {
            int nextNodeId = m->path[1];
            if (capacityNetwork.graph[nodeId][nextNodeId] < MAX_CAPACITY_PER_LINK)
            {
              m->state = SENDING;
              m->queuedAtNodeId = -1;
              m->last_sent_time = clock();
              m->progress = 0;
              pathfindingNetwork.graph[nodeId][nextNodeId]++;
              capacityNetwork.graph[nodeId][nextNodeId]++;
              nodeReleaseCooldown[nodeId] = releaseInterval;
            }
          }
        }
        else if (nodeId == m->to)
        {
          m->ackPathLength = BuildPath(m->to, m->from, m->ackPath, MAX_NODES);
          if (m->ackPathLength > 1)
          {
            int nextNodeId = m->ackPath[1];
            if (capacityNetwork.graph[nodeId][nextNodeId] < MAX_CAPACITY_PER_LINK)
            {
              m->state = ACK_RECEIVING;
              m->currentAckSegment = 0;
              m->queuedAtNodeId = -1;
              m->progress = 0;
              pathfindingNetwork.graph[nodeId][nextNodeId]++;
              capacityNetwork.graph[nodeId][nextNodeId]++;
              nodeReleaseCooldown[nodeId] = releaseInterval;
            }
          }
        }
        else
        {
          int nextNodeId = (m->pathLength > 0) ? m->path[m->currentSegment + 1] : m->ackPath[m->currentAckSegment + 1];
          if (capacityNetwork.graph[nodeId][nextNodeId] < MAX_CAPACITY_PER_LINK)
          {
            m->state = (m->pathLength > 0) ? SENDING : ACK_RECEIVING;
            m->queuedAtNodeId = -1;
            m->progress = 0;
            pathfindingNetwork.graph[nodeId][nextNodeId]++;
            capacityNetwork.graph[nodeId][nextNodeId]++;
            nodeReleaseCooldown[nodeId] = releaseInterval;
          }
        }
      }
      break;
    }
    case DONE:
      break;
    }
  }
}

void SendOneBurstRound()
{
  const int streamsPerRound = 10;
  if (nodeCount == 0)
    return;
  for (int i = 0; i < streamsPerRound; i++)
  {
    int from = rand() % nodeCount;
    int to = rand() % nodeCount;
    while (from == to)
    {
      to = rand() % nodeCount;
    }
    AddAsyncMessage(from, to);
  }
}

//====================================================================================
// FUNÇÕES DE VISUALIZAÇÃO E MAIN
//====================================================================================

void PrintNonCompletedMessages()
{
  printf("\n---[ Status das Mensagens Nao Concluidas ]---\n");
  int notDoneCount = 0;
  for (int i = 0; i < messageCount; i++)
  {
    if (messages[i].state != DONE)
    {
      notDoneCount++;
      const char *stateStr;
      switch (messages[i].state)
      {
      case SENDING:
        stateStr = "ENVIANDO";
        break;
      case ACK_RECEIVING:
        stateStr = "RECEBENDO ACK";
        break;
      case QUEUED:
        stateStr = "ENFILEIRADA";
        break;
      default:
        stateStr = "DESCONHECIDO";
        break;
      }
      printf("Msg[%d]: De %d->%d | Estado: %-15s", i, messages[i].from, messages[i].to, stateStr);
      if (messages[i].state == QUEUED)
      {
        printf("| Local: No %d\n", messages[i].queuedAtNodeId);
      }
      else if (messages[i].state == SENDING)
      {
        printf("| Progresso: %.2f | Segmento: %d de %d\n", messages[i].progress, messages[i].currentSegment, messages[i].pathLength - 1);
      }
      else if (messages[i].state == ACK_RECEIVING)
      {
        printf("| Progresso: %.2f | Segmento ACK: %d de %d\n", messages[i].progress, messages[i].currentAckSegment, messages[i].ackPathLength - 1);
      }
    }
  }
  if (notDoneCount == 0)
    printf("Todas as mensagens foram concluidas com sucesso.\n");
  else
    printf("Total de mensagens nao concluidas: %d\n", notDoneCount);
  printf("---[ Fim do Relatorio ]---\n\n");
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
    DrawCircleLines(pos.x, pos.y, 8, BLACK);
  }
}

void DrawQueuedMessages()
{
  int originQueueCounts[MAX_NODES] = {0};
  int ackQueueCounts[MAX_NODES] = {0};
  int intermediateQueueCounts[MAX_NODES] = {0};
  for (int i = 0; i < messageCount; i++)
  {
    if (messages[i].state == QUEUED && messages[i].queuedAtNodeId != -1)
    {
      int nodeId = messages[i].queuedAtNodeId;
      if (nodeId == messages[i].from)
        originQueueCounts[nodeId]++;
      else if (nodeId == messages[i].to)
        ackQueueCounts[nodeId]++;
      else
        intermediateQueueCounts[nodeId]++;
    }
  }
  for (int nodeId = 0; nodeId < nodeCount; nodeId++)
  {
    Node *node = &nodes[nodeId];
    bool hasOriginQueue = originQueueCounts[nodeId] > 0 || intermediateQueueCounts[nodeId] > 0;
    bool hasAckQueue = ackQueueCounts[nodeId] > 0;
    if (hasOriginQueue)
    {
      Vector2 pos = {node->x + QUEUE_OFFSET_X + 10, node->y - (NODE_RADIUS / 2.0f)};
      DrawCircleV(pos, 8, RED);
      DrawCircleLines(pos.x, pos.y, 8, BLACK);
      char countText[16];
      sprintf(countText, "%d", originQueueCounts[nodeId] + intermediateQueueCounts[nodeId]);
      DrawText(countText, pos.x + 12, pos.y - 8, 20, BLACK);
    }
    if (hasAckQueue)
    {
      Vector2 pos = {node->x + QUEUE_OFFSET_X + 10, node->y + (NODE_RADIUS / 2.0f)};
      DrawCircleV(pos, 8, GREEN);
      DrawCircleLines(pos.x, pos.y, 8, BLACK);
      char countText[16];
      sprintf(countText, "%d", ackQueueCounts[nodeId]);
      DrawText(countText, pos.x + 12, pos.y - 8, 20, BLACK);
    }
  }
}

void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed)
{
  static char fromText[8] = "0";
  static char toText[8] = "13";
  static char msgText[8] = "50";
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

// ALTERADO: Função de estatísticas agora inclui o cálculo e a exibição da VAZÃO (Throughput).
void DrawStatistics(int screenW)
{
  // Aumenta a altura da área para caber a nova estatística
  Rectangle statsArea = {screenW - 270, 200, 260, 180};
  DrawRectangleRec(statsArea, (Color){220, 220, 220, 190});
  DrawRectangleLinesEx(statsArea, 2, DARKGRAY);

  DrawText("--- Estatisticas ---", statsArea.x + 10, statsArea.y + 10, 20, BLACK);

  // Calcula a latência média (código existente)
  float avg_latency_ms = 0;
  if (completed_messages_count > 0)
  {
    avg_latency_ms = ((float)total_latency_ticks / completed_messages_count / CLOCKS_PER_SEC) * 1000.0f;
  }

  // --- NOVO: CÁLCULO DA VAZÃO ---
  float elapsed_time = GetTime(); // Pega o tempo total desde o início da simulação
  float throughput = 0;
  // Evita divisão por zero no início e calcula a vazão
  if (elapsed_time > 0.1f) // Usamos um pequeno limiar para estabilizar no início
  {
    throughput = (float)completed_messages_count / elapsed_time;
  }

  // Exibe as estatísticas
  DrawText(TextFormat("Msgs Concluidas: %d", completed_messages_count), statsArea.x + 10, statsArea.y + 40, 20, DARKGRAY);
  DrawText(TextFormat("Latencia: %.2f ms", avg_latency_ms), statsArea.x + 10, statsArea.y + 70, 20, DARKGRAY);
  DrawText(TextFormat("Timeouts: %d", total_retransmissions), statsArea.x + 10, statsArea.y + 100, 20, DARKGRAY);

  // --- NOVO: EXIBIÇÃO DA VAZÃO ---
  DrawText(TextFormat("Vazao: %.2f msg/s", throughput), statsArea.x + 10, statsArea.y + 130, 20, DARKGRAY);
}

//====================================================================================
// FUNÇÃO PRINCIPAL
//====================================================================================
int main(void)
{
  const int screenW = 1280, screenH = 720;
  InitWindow(screenW, screenH, "Simulador de Rede Avançado");
  SetTargetFPS(60);
  srand(time(NULL));

  int uiFromNode = 0, uiToNode = 13, uiMsgCount = 50;
  bool sendPressed = false, burstInProgress = false;
  int messagesToSend = 0, burstRoundsSent = 0;
  float messageSendTimer = 0.0f, burstTimer = 0.0f;
  int messageFromNode = -1, messageToNode = -1, nodeToConnect = -1;
  const int TOTAL_BURST_ROUNDS = 10;

  while (!WindowShouldClose())
  {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();
    Rectangle uiArea = {screenW - 220, 10, 210, 190};

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

    UpdateAsyncMessages(dt, 0.1f);

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

    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawNetwork();
    DrawTravelingMessages();
    DrawQueuedMessages();
    DrawUI(uiArea, &uiFromNode, &uiToNode, &uiMsgCount, &sendPressed);
    DrawStatistics(screenW);
    DrawText("ESQ: Adicionar | DIR: Conectar", 10, 10, 20, DARKGRAY);
    DrawText("Q: Rede Padrao | W: Limpar | P: Status | B: Rajada", 10, 40, 20, DARKGRAY);
    DrawText("CTRL+Z: Desfazer", 10, 70, 20, DARKGRAY);
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