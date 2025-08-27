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
#define MAX_MESSAGES 1000000
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
  // Reseta o estado da rede
  nodeCount = 0;
  messageCount = 0;
  actionTop = -1;
  memset(&busyNetwork, 0, sizeof(Network));

  // --- Adiciona 14 Nós em Posições Estratégicas ---

  // Hub Central e Anel Interno
  AddNode(450, 360); // Nó 0 (Hub Central)
  AddNode(300, 200); // Nó 1
  AddNode(300, 520); // Nó 2
  AddNode(600, 200); // Nó 3
  AddNode(600, 520); // Nó 4

  // Cluster da Esquerda
  AddNode(120, 120); // Nó 5
  AddNode(120, 360); // Nó 6
  AddNode(120, 600); // Nó 7

  // Cluster da Direita
  AddNode(780, 120); // Nó 8
  AddNode(780, 360); // Nó 9
  AddNode(780, 600); // Nó 10

  // Nós Externos (Topo, Baixo, Extrema Direita)
  AddNode(450, 50);  // Nó 11 (Topo)
  AddNode(450, 670); // Nó 12 (Baixo)
  AddNode(950, 360); // Nó 13 (Extrema Direita)

  // --- Cria as Conexões para Formar a Topologia ---

  // Conexões do Hub Central (Nó 0)
  ConnectNodes(0, 1);
  ConnectNodes(0, 2);
  ConnectNodes(0, 3);
  ConnectNodes(0, 4);

  // Conexões do Anel Interno
  ConnectNodes(1, 2);
  ConnectNodes(2, 4);
  ConnectNodes(4, 3);
  ConnectNodes(3, 1);

  // Conectando o Cluster da Esquerda ao Anel
  ConnectNodes(1, 5);
  ConnectNodes(1, 6);
  ConnectNodes(2, 6);
  ConnectNodes(2, 7);
  ConnectNodes(5, 6); // Conexão local do cluster
  ConnectNodes(6, 7); // Conexão local do cluster

  // Conectando o Cluster da Direita ao Anel
  ConnectNodes(3, 8);
  ConnectNodes(3, 9);
  ConnectNodes(4, 9);
  ConnectNodes(4, 10);
  ConnectNodes(8, 9);  // Conexão local do cluster
  ConnectNodes(9, 10); // Conexão local do cluster

  // Conectando os Nós Externos
  ConnectNodes(11, 1);
  ConnectNodes(11, 3);
  ConnectNodes(12, 2);
  ConnectNodes(12, 4);
  ConnectNodes(13, 9);
}

void AddAsyncMessage(int from, int to)
{
  if (messageCount >= MAX_MESSAGES)
    return;

  // --- LÓGICA PROATIVA ---
  // 1. Primeiro, tentamos encontrar o caminho de IDA.
  int pathLength = BuildPath(from, to, messages[messageCount].path, MAX_NODES, &busyNetwork);
  if (pathLength <= 1)
  {
    printf("FALHA PROATIVA (IDA): Nao ha caminho livre de %d para %d.\n", from, to);
    return;
  }

  // 2. Antes de confirmar, verificamos se já existe um caminho de VOLTA (para o futuro ACK).
  int tempAckPath[MAX_NODES]; // Caminho temporário apenas para verificação
  int ackPathLength = BuildPath(to, from, tempAckPath, MAX_NODES, &busyNetwork);
  if (ackPathLength <= 1)
  {
    printf("FALHA PROATIVA (VOLTA): Rota de %d->%d encontrada, mas nao ha rota de retorno de %d->%d para o ACK. Envio cancelado para evitar fila.\n", from, to, to, from);
    return; // **PONTO CRÍTICO**: Se não há como voltar, a mensagem não é nem enviada.
  }

  // 3. Se ambos os caminhos (ida e um potencial de volta) existem, confirmamos o envio.
  AsyncMessage *m = &messages[messageCount];
  m->pathLength = pathLength; // Já calculamos, agora só atribuímos.

  // Reserva o caminho de IDA na rede.
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
  m->ackPathLength = 0; // O caminho do ACK só será definido na chegada.
  m->queuedAtNodeId = -1;
  messageCount++;
  printf("Msg %d enviada de %d para %d. Rota de retorno para ACK verificada.\n", messageCount - 1, from, to);
}
// ALTERADO: Lógica de atualização completamente refeita para corrigir o bug de "vazamento de recursos".
// Agora os caminhos são liberados corretamente ao final de cada etapa.
// ALTERADO: Lógica de atualização completamente refeita para ser mais robusta e corrigir o bug da fila.
// ALTERADO: Lógica de atualização com mecanismo de "justiça" para evitar que uma
// mensagem bloqueie todas as outras na fila no mesmo quadro.
// NOVA FUNÇÃO para depuração: Imprime o estado de todas as mensagens que não foram concluídas.
void PrintNonCompletedMessages()
{

  
}
// NOVO: Variável estática para implementar o Round-Robin.
// Guarda o índice da PRÓXIMA mensagem que o nó deve tentar liberar.
// Variável estática para implementar o Round-Robin.
// Guarda o índice da PRÓXIMA mensagem que o nó deve tentar liberar.
// Variáveis estáticas para controle de fila e cooldown
static int nextMessageToTry[MAX_NODES] = {0};
static float nodeReleaseCooldown[MAX_NODES] = {0.0f};

void UpdateAsyncMessages(float dt, float releaseInterval)
{
  PrintNonCompletedMessages();

  // Diminui o cooldown de todos os nós a cada quadro.
  for (int i = 0; i < nodeCount; i++)
  {
    if (nodeReleaseCooldown[i] > 0)
    {
      nodeReleaseCooldown[i] -= dt;
    }
  }

  // --- LOOP DE ATUALIZAÇÃO UNIFICADO ---
  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];

    // O progresso de tempo é aplicado a mensagens em movimento
    if (m->state == SENDING || m->state == ACK_RECEIVING)
    {
      m->progress += dt * MESSAGE_SPEED;
    }

    // Máquina de estados para cada mensagem
    switch (m->state)
    {
    case SENDING:
      if (m->progress >= 1.0f)
      {
        m->progress -= 1.0f;
        m->currentSegment++;
        int currentNodeId = m->path[m->currentSegment];

        if (currentNodeId == m->to) // Chegou ao destino
        {
          for (int j = 0; j < m->pathLength - 1; j++)
          {
            if (busyNetwork.graph[m->path[j]][m->path[j + 1]] > 0)
              busyNetwork.graph[m->path[j]][m->path[j + 1]]--;
          }
          m->pathLength = 0;
          m->state = QUEUED;
          m->queuedAtNodeId = m->to;
          printf("Msg %d chegou ao destino %d e foi para a fila de ACK.\n", i, m->to);
        }
      }
      break;

    case ACK_RECEIVING:
      if (m->progress >= 1.0f)
      {
        m->progress -= 1.0f;
        m->currentAckSegment++;
        int currentNodeId = m->ackPath[m->currentAckSegment];

        if (currentNodeId == m->from) // ACK chegou à origem
        {
          for (int j = 0; j < m->ackPathLength - 1; j++)
          {
            if (busyNetwork.graph[m->ackPath[j]][m->ackPath[j + 1]] > 0)
              busyNetwork.graph[m->ackPath[j]][m->ackPath[j + 1]]--;
          }
          m->state = DONE;
          printf("Msg %d CONCLUIDA (ACK recebido).\n", i);
        }
      }
      break;

    case QUEUED:
    {
      int nodeId = m->queuedAtNodeId;
      if (nodeId != -1 && nodeReleaseCooldown[nodeId] <= 0)
      {
        // Tenta encontrar um caminho de volta para o ACK
        m->ackPathLength = BuildPath(m->to, m->from, m->ackPath, MAX_NODES, &busyNetwork);
        if (m->ackPathLength > 1)
        {
          // Sucesso! Configura a mensagem para o estado de ACK
          for (int j = 0; j < m->ackPathLength - 1; j++)
          {
            busyNetwork.graph[m->ackPath[j]][m->ackPath[j + 1]]++;
          }
          m->state = ACK_RECEIVING;
          m->progress = 0;
          m->currentAckSegment = 0;
          m->queuedAtNodeId = -1;

          // Ativa o cooldown para este nó
          nodeReleaseCooldown[nodeId] = releaseInterval;

          printf("Msg %d saiu da fila do no %d (ACK).\n", i, nodeId);
        }
      }
      break;
    }

    case DONE:
      // Nenhuma ação necessária
      break;
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

// ALTERADO: A função agora conta as mensagens na fila e desenha um único
// indicador com um contador, em vez de uma pilha de mensagens.
void DrawQueuedMessages()
{
  // 1. Primeiro, contamos quantas mensagens estão na fila de cada nó.
  int queueCounts[MAX_NODES] = {0};
  for (int i = 0; i < messageCount; i++)
  {
    if (messages[i].state == QUEUED && messages[i].queuedAtNodeId != -1)
    {
      queueCounts[messages[i].queuedAtNodeId]++;
    }
  }

  // 2. Agora, desenhamos um indicador para cada nó que tiver uma fila.
  for (int nodeId = 0; nodeId < nodeCount; nodeId++)
  {
    if (queueCounts[nodeId] > 0)
    {
      Node *node = &nodes[nodeId];

      // Posição do indicador visual da fila
      Vector2 pos = {node->x + QUEUE_OFFSET_X + 10, node->y - NODE_RADIUS};

      // CORRIGIDO: Uma mensagem enfileirada no destino está esperando para
      // se tornar um ACK, então a cor correta é VERDE.
      Color color = GREEN;

      // Desenha o círculo que representa a fila
      DrawCircleV(pos, 8, color);
      DrawCircleLines(pos.x, pos.y, 8, BLACK);

      // Prepara e desenha o texto com o número de mensagens na fila
      char countText[16];
      sprintf(countText, "%d", queueCounts[nodeId]);
      DrawText(countText, pos.x + 12, pos.y - 8, 20, BLACK);
    }
  }
}

// NOVO: Função para criar uma rajada de tráfego predefinida
// NOVO: Esta função envia apenas UMA rodada de mensagens da rajada.
// ALTERADO: A função agora envia uma rodada de fluxos com origens e destinos aleatórios.
void SendOneBurstRound()
{
  // Define quantos fluxos de mensagens queremos criar a cada rodada do burst.
  const int streamsPerRound = 5;

  // printf(" -> Gerando %d fluxos aleatórios para a rajada...\n", streamsPerRound);

  for (int i = 0; i < streamsPerRound; i++)
  {
    // Garante que haja nós na rede para evitar divisão por zero.
    if (nodeCount == 0)
      continue;

    // Escolhe um nó de origem aleatório.
    int from = rand() % nodeCount;

    // Escolhe um nó de destino aleatório.
    int to = rand() % nodeCount;

    // Garante que um nó não envie uma mensagem para si mesmo.
    while (from == to)
    {
      to = rand() % nodeCount;
    }

    // Adiciona a mensagem com o caminho aleatório ao sistema.
    AddAsyncMessage(from, to);
    // Opcional: Descomente a linha abaixo para ver no console os caminhos gerados.
    // printf("      - Fluxo aleatório criado: %d -> %d\n", from, to);
  }
}

void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed)
{
  static char fromText[8] = "0";
  static char toText[8] = "4";
  static char msgText[8] = "2";
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

    UpdateAsyncMessages(dt, dt*10);

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
    bool burstInProgress = false;
    int burstRoundsSent = 0;
    float burstTimer = 0.0f;
    const int TOTAL_BURST_ROUNDS = 10;
    if (IsKeyPressed(KEY_B)){ // NOVO: Gatilho para a rajada de tráfego
      burstTimer += dt;
      if (burstTimer >= dt) // Usa o mesmo intervalo da UI
      {
        SendOneBurstRound(); // Envia uma rodada de 5 fluxos
        burstRoundsSent++;
        burstTimer = 0.0f;

        // Se todas as rodadas foram enviadas, termina o burst
        if (burstRoundsSent >= TOTAL_BURST_ROUNDS)
        {
          burstInProgress = false;
          printf("---[ RAJADA DE TRÁFEGO CONCLUÍDA ]---\n");
        }
      }
  }
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
