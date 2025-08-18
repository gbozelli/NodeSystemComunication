#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

void SendMessage(int fromId, int toId)
{
  int path[50];
  int pathLength = BuildPath(fromId, toId, path, 50);

  if (pathLength == -1)
  {
    printf("Caminho não encontrado!\n");
    return;
  }

  for (int i = 0; i < pathLength - 1; i++)
  {
    Vector2 start = {nodes[path[i]].x, nodes[path[i]].y};
    Vector2 end = {nodes[path[i + 1]].x, nodes[path[i + 1]].y};

    for (float t = 0; t <= 1; t += 0.02f)
    {
      BeginDrawing();
      ClearBackground(RAYWHITE);

      DrawNetwork();

      // desenha a mensagem
      Vector2 pos = {
          start.x + (end.x - start.x) * t,
          start.y + (end.y - start.y) * t};
      DrawCircleV(pos, 8, RED);

      EndDrawing();
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

int main(void)
{
  InitWindow(800, 700, "Simulador de Rede - Raylib");
  SetTargetFPS(60);
  int mode = 0;
  int connectMode = 0;
  int fromNode = -1, toNode = -1;

  while (!WindowShouldClose())
  {
    // Clique esquerdo para adicionar nó
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mode == 0)
    {
      Vector2 mouse = GetMousePosition();
      AddNode(mouse.x, mouse.y);
      PushAction(ACTION_ADD_NODE, nodeCount - 1, -1); // registra ação
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
                SendMessage(fromNode, toNode);
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
      SendMessage(fromNode, toNode);
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
      DrawText("Pressione M: Entrar no modo MENSAGEM", 10, 100, 20, DARKGRAY);
    }
    else
    {
      DrawText("Modo MENSAGEM", 10, 10, 20, DARKGRAY);
      DrawText("Clique DIREITO: Selecionar origem e destino para enviar mensagem", 10, 40, 20, DARKGRAY);
      DrawText("Pressione M: Voltar ao modo NORMAL", 10, 70, 20, DARKGRAY);

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
    EndDrawing();
  }

  CloseWindow();
  return 0;
}