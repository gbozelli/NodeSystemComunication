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
  int parentId; // -1 se não tiver pai
} Node;

Node nodes[MAX_NODES];
int nodeCount = 0;

// Adiciona nó na posição x, y
void AddNode(float x, float y)
{
  if (nodeCount >= MAX_NODES)
    return;
  nodes[nodeCount].id = nodeCount;
  nodes[nodeCount].x = x;
  nodes[nodeCount].y = y;
  nodes[nodeCount].parentId = -1;
  nodeCount++;
}


// Desenha os nós e conexões
void DrawNetwork()
{
  // Conexões
  for (int i = 0; i < nodeCount; i++)
  {
    if (nodes[i].parentId != -1)
    {
      DrawLine(nodes[i].x, nodes[i].y,
               nodes[nodes[i].parentId].x, nodes[nodes[i].parentId].y, GRAY);
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

  nodes[a].connections[nodes[a].connectionCount++] = b;
  nodes[b].connections[nodes[b].connectionCount++] = a;
  
  if (a >= 0 && a < nodeCount && b >= 0 && b < nodeCount)
  {
    nodes[b].parentId = a;
    }
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

int main(void)
{
  InitWindow(800, 600, "Simulador de Rede - Raylib");
  SetTargetFPS(60);
  int mode = 0;
  int connectMode = 0;
  int fromNode = -1, toNode = -1;

  while (!WindowShouldClose())
  {
    // Clique esquerdo para adicionar nó
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      Vector2 mouse = GetMousePosition();
      AddNode(mouse.x, mouse.y);
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
      fromNode = -1, toNode = -1; connectMode = 0;
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
