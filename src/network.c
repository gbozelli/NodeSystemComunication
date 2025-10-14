#include "../include/network.h"
#include <string.h> // Para memset

// Funções de gerenciamento da pilha de ações (Undo/Redo)
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
  // Limpar mensagens ao desfazer pode ser drástico, mas previne estados inconsistentes.
  // Você pode refinar isso se precisar de um 'undo' mais inteligente.
  for (int i = 0; i < MAX_MESSAGES; i++)
  {
    messages[i] = (AsyncMessage){0};
  }
  messageCount = 0;

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
    int a = act.nodeA, b = act.nodeB;
    // Desconecta B de A
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
    // Desconecta A de B
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

// Funções de gerenciamento de nós e conexões
void AddNode(float x, float y)
{
  if (nodeCount >= MAX_NODES)
    return;
  nodes[nodeCount] = (Node){x, y, nodeCount, .enabled = true};
  nodes[nodeCount].connectionCount = 0; // Inicializa o contador de conexões
  nodeCount++;
}

void ConnectNodes(int a, int b)
{
  if (a < 0 || b < 0 || a >= nodeCount || b >= nodeCount || a == b)
    return;
  if (nodes[a].connectionCount >= MAX_CONNECTIONS || nodes[b].connectionCount >= MAX_CONNECTIONS)
    return;

  // Conecta A -> B
  int exists = 0;
  for (int i = 0; i < nodes[a].connectionCount; i++)
    if (nodes[a].connections[i] == b)
      exists = 1;
  if (!exists)
    nodes[a].connections[nodes[a].connectionCount++] = b;

  // Conecta B -> A
  exists = 0;
  for (int i = 0; i < nodes[b].connectionCount; i++)
    if (nodes[b].connections[i] == a)
      exists = 1;
  if (!exists)
    nodes[b].connections[nodes[b].connectionCount++] = a;
}

// Algoritmo de busca de caminho (BFS)
int BuildPath(int start, int goal, int *path, int maxLen)
{
  int visited[MAX_NODES] = {0};
  int parent[MAX_NODES];
  for (int i = 0; i < MAX_NODES; i++)
    parent[i] = -1;
  int queue[MAX_NODES], front = 0, rear = 0;

  if (start >= nodeCount || !nodes[start].enabled)
    return -1;

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
      if (!nodes[next].enabled)
        continue;

      // Checa a capacidade da PISTA OPOSTA na capacityNetwork.
      if (!visited[next] && capacityNetwork.graph[next][current] < g_link_capacity)
      {
        visited[next] = 1;
        parent[next] = current;
        queue[rear++] = next;
      }
    }
  }

  if (!found)
    return -1;

  // Reconstrói o caminho
  int temp[MAX_NODES], len = 0;
  int cur = goal;
  while (cur != -1)
  {
    temp[len++] = cur;
    cur = parent[cur];
  }
  // Inverte o caminho para a ordem correta (start -> goal)
  for (int i = 0; i < len; i++)
  {
    path[i] = temp[len - i - 1];
  }
  return len;
}

// Função para criar uma topologia de rede padrão
void CreateDefaultNetwork()
{
  // Reseta todo o estado da simulação
  nodeCount = 0;
  messageCount = 0;
  actionTop = -1;
  memset(&nodes, 0, sizeof(nodes));
  memset(&messages, 0, sizeof(messages));
  memset(&pathfindingNetwork, 0, sizeof(Network));
  memset(&capacityNetwork, 0, sizeof(Network));
  total_latency_ticks = 0;
  completed_messages_count = 0;
  total_retransmissions = 0;

  // Adiciona nós
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

  // Conecta os nós
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
