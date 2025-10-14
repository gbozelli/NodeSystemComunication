#include "../include/messaging.h"
#include "../include/network.h" // Precisa de BuildPath

void AddAsyncMessage(int from, int to)
{
  // Simula falha no envio com base na probabilidade
  if (((float)rand() / RAND_MAX) > g_prob_success)
  {
    return;
  }
  if (messageCount >= MAX_MESSAGES)
    return;

  AsyncMessage *m = &messages[messageCount];
  *m = (AsyncMessage){.from = from, .to = to, .retransmission_count = 0, .creation_time = clock()};

  m->pathLength = BuildPath(from, to, m->path, MAX_NODES);
  if (m->pathLength > 1)
  {
    int first_hop_node = m->path[1];
    if (capacityNetwork.graph[from][first_hop_node] < g_link_capacity)
    {
      // O caminho está livre, envia a mensagem
      m->state = SENDING;
      m->queuedAtNodeId = -1;
      pathfindingNetwork.graph[from][first_hop_node]++;
      capacityNetwork.graph[from][first_hop_node]++;
      m->last_sent_time = clock(); // Inicia o timer do timeout
    }
    else
    {
      // O caminho está ocupado, enfileira na origem
      m->state = QUEUED;
      m->queuedAtNodeId = from;
      m->last_sent_time = 0; // Não inicia o timer
    }
  }
  else
  {
    // Não foi encontrado caminho, enfileira na origem
    m->state = QUEUED;
    m->queuedAtNodeId = from;
    m->last_sent_time = 0; // Não inicia o timer
  }
  messageCount++;
}

void UpdateAsyncMessages(float dt, float releaseInterval)
{
  static float nodeReleaseCooldown[MAX_NODES] = {0.0f};
  for (int i = 0; i < nodeCount; i++)
  {
    if (nodeReleaseCooldown[i] > 0)
      nodeReleaseCooldown[i] -= dt;
  }
  clock_t now = clock();

  for (int i = 0; i < messageCount; i++)
  {
    AsyncMessage *m = &messages[i];
    if (m->state == DONE)
      continue;

    // --- VERIFICAÇÃO DE TIMEOUT ---
    // Só verifica se a mensagem já foi enviada (timer iniciado)
    if (m->last_sent_time > 0)
    {
      if (((double)(now - m->last_sent_time) / CLOCKS_PER_SEC) > g_timeout_seconds)
      {
        total_retransmissions++;

        // Libera recursos que a mensagem estava usando
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

        // Reseta a mensagem para a fila da origem
        m->state = QUEUED;
        m->queuedAtNodeId = m->from;
        m->retransmission_count++;
        m->pathLength = 0;
        m->ackPathLength = 0;
        m->progress = 0;
        m->currentSegment = 0;
        m->last_sent_time = 0; // Pausa o timer

        continue; // Pula para a próxima mensagem
      }
    }

    // Atualiza o progresso da animação
    if (m->state == SENDING || m->state == ACK_RECEIVING)
      m->progress += dt * MESSAGE_SPEED;

    // Máquina de estados da mensagem
    switch (m->state)
    {
    case SENDING:
      if (m->progress >= 1.0f)
      {
        m->progress -= 1.0f;
        int prevNodeId = m->path[m->currentSegment];
        m->currentSegment++;
        int currentNodeId = m->path[m->currentSegment];

        // Libera o enlace anterior
        pathfindingNetwork.graph[prevNodeId][currentNodeId]--;
        capacityNetwork.graph[prevNodeId][currentNodeId]--;

        if (currentNodeId == m->to) // Chegou ao destino
        {
          m->state = QUEUED; // Enfileira para enviar o ACK
          m->queuedAtNodeId = m->to;
        }
        else // Nó intermediário
        {
          int nextNodeId = m->path[m->currentSegment + 1];
          if (capacityNetwork.graph[currentNodeId][nextNodeId] < g_link_capacity)
          {
            // Ocupa o próximo enlace
            pathfindingNetwork.graph[currentNodeId][nextNodeId]++;
            capacityNetwork.graph[currentNodeId][nextNodeId]++;
          }
          else
          {
            // Enfileira no nó atual se o próximo enlace estiver ocupado
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

        if (currentNodeId == m->from) // ACK chegou à origem
        {
          m->state = DONE;
          m->completion_time = clock();
          total_latency_ticks += (m->completion_time - m->creation_time);
          completed_messages_count++;
        }
        else // Nó intermediário do ACK
        {
          int nextNodeId = m->ackPath[m->currentAckSegment + 1];
          if (capacityNetwork.graph[currentNodeId][nextNodeId] < g_link_capacity)
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
        if (nodeId == m->from) // Na origem, tentando (re)enviar
        {
          m->pathLength = BuildPath(m->from, m->to, m->path, MAX_NODES);
          if (m->pathLength > 1)
          {
            int nextNodeId = m->path[1];
            if (capacityNetwork.graph[nodeId][nextNodeId] < g_link_capacity)
            {
              m->state = SENDING;
              m->queuedAtNodeId = -1;
              m->last_sent_time = clock();
              m->progress = 0;
              m->currentSegment = 0;
              pathfindingNetwork.graph[nodeId][nextNodeId]++;
              capacityNetwork.graph[nodeId][nextNodeId]++;
              nodeReleaseCooldown[nodeId] = releaseInterval;
            }
          }
        }
        else if (nodeId == m->to) // No destino, tentando enviar ACK
        {
          m->ackPathLength = BuildPath(m->to, m->from, m->ackPath, MAX_NODES);
          if (m->ackPathLength > 1)
          {
            int nextNodeId = m->ackPath[1];
            if (capacityNetwork.graph[nodeId][nextNodeId] < g_link_capacity)
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
        else // Em nó intermediário, tentando continuar o caminho
        {
          // Determina se é a mensagem original ou o ACK que está enfileirado
          bool isSendingMsg = (m->ackPathLength == 0);
          int nextNodeId = isSendingMsg ? m->path[m->currentSegment + 1] : m->ackPath[m->currentAckSegment + 1];

          if (capacityNetwork.graph[nodeId][nextNodeId] < g_link_capacity)
          {
            m->state = isSendingMsg ? SENDING : ACK_RECEIVING;
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
