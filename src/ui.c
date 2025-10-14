#include "../include/ui.h"
#include "../include/definitions.h" // Adicionado para ter visibilidade das estruturas

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
  // Desenha as conexões
  for (int i = 0; i < nodeCount; i++)
  {
    for (int j = 0; j < nodes[i].connectionCount; j++)
    {
      int b = nodes[i].connections[j];
      if (b > i) // Desenha cada linha apenas uma vez
        DrawLine(nodes[i].x, nodes[i].y, nodes[b].x, nodes[b].y, GRAY);
    }
  }
  // Desenha os nós
  for (int i = 0; i < nodeCount; i++)
  {
    DrawCircle(nodes[i].x, nodes[i].y, NODE_RADIUS, BLUE);
    DrawText(TextFormat("%d", nodes[i].id), nodes[i].x - 5, nodes[i].y - 10, 20, WHITE);
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
    else // ACK_RECEIVING
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

  // Conta mensagens enfileiradas em cada nó
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

  // Desenha os indicadores de fila
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
      DrawText(TextFormat("%d", originQueueCounts[nodeId] + intermediateQueueCounts[nodeId]), pos.x + 12, pos.y - 8, 20, BLACK);
    }
    if (hasAckQueue)
    {
      Vector2 pos = {node->x + QUEUE_OFFSET_X + 10, node->y + (NODE_RADIUS / 2.0f)};
      DrawCircleV(pos, 8, GREEN);
      DrawCircleLines(pos.x, pos.y, 8, BLACK);
      DrawText(TextFormat("%d", ackQueueCounts[nodeId]), pos.x + 12, pos.y - 8, 20, BLACK);
    }
  }
}

void DrawUI(Rectangle uiArea, int *uiFromNode, int *uiToNode, int *uiMsgCount, bool *sendPressed)
{
  static char fromText[8] = "0";
  static char toText[8] = "13";
  static char msgText[8] = "50";
  static int activeBox = -1; // -1: nenhum, 0: from, 1: to, 2: msg

  // A área da UI agora é só para o painel de envio
  Rectangle sendArea = {uiArea.x, uiArea.y, uiArea.width, 180};
  DrawRectangleRec(sendArea, (Color){200, 200, 200, 180});
  DrawRectangleLinesEx(sendArea, 2, DARKGRAY);

  int startX = sendArea.x + 10;
  int startY = sendArea.y + 10;
  int boxW = 80, boxH = 30, spacing = 40;

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
    // Não desativa o activeBox se clicar fora da área de envio, apenas fora da UI geral
    else if (!CheckCollisionPointRec(GetMousePosition(), uiArea))
      activeBox = -1;
  }

  char *targetText = NULL;
  if (activeBox == 0)
    targetText = fromText;
  else if (activeBox == 1)
    targetText = toText;
  else if (activeBox == 2)
    targetText = msgText;

  if (targetText)
  {
    SetMouseCursor(MOUSE_CURSOR_IBEAM);
    int key = GetCharPressed();
    while (key > 0)
    {
      if (key >= '0' && key <= '9' && (strlen(targetText) < 7))
      {
        int len = strlen(targetText);
        targetText[len] = (char)key;
        targetText[len + 1] = '\0';
      }
      key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE))
    {
      int len = strlen(targetText);
      if (len > 0)
        targetText[len - 1] = '\0';
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
  DrawText("Enviar", btn.x + 45, btn.y + 5, 20, BLACK);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), btn))
  {
    *uiFromNode = atoi(fromText);
    *uiToNode = atoi(toText);
    *uiMsgCount = atoi(msgText);
    *sendPressed = true;
  }
}

void DrawStatistics(int screenW)
{
  Rectangle statsArea = {screenW - 270, 200, 260, 180};
  DrawRectangleRec(statsArea, (Color){220, 220, 220, 190});
  DrawRectangleLinesEx(statsArea, 2, DARKGRAY);

  DrawText("--- Estatisticas ---", statsArea.x + 10, statsArea.y + 10, 20, BLACK);

  float avg_latency_ms = 0;
  if (completed_messages_count > 0)
  {
    avg_latency_ms = ((float)total_latency_ticks / completed_messages_count / CLOCKS_PER_SEC) * 1000.0f;
  }

  float elapsed_time = GetTime();
  float throughput = 0;
  if (elapsed_time > 0.1f)
  {
    throughput = (float)completed_messages_count / elapsed_time;
  }

  DrawText(TextFormat("Msgs Concluidas: %d", completed_messages_count), statsArea.x + 10, statsArea.y + 40, 20, DARKGRAY);
  DrawText(TextFormat("Latencia: %.2f ms", avg_latency_ms), statsArea.x + 10, statsArea.y + 70, 20, DARKGRAY);
  DrawText(TextFormat("Timeouts: %d", total_retransmissions), statsArea.x + 10, statsArea.y + 100, 20, DARKGRAY);
  DrawText(TextFormat("Vazao: %.2f msg/s", throughput), statsArea.x + 10, statsArea.y + 130, 20, DARKGRAY);
}

void DrawParametersUI(int screenW)
{
  static char capacityText[8] = "20";
  static char timeoutText[8] = "10.0";
  static char probText[8] = "1.0";
  static int activeBox = -1;

  Rectangle paramArea = {screenW - 270, 390, 260, 160};
  DrawRectangleRec(paramArea, (Color){220, 220, 220, 190});
  DrawRectangleLinesEx(paramArea, 2, DARKGRAY);

  DrawText("--- Parametros ---", paramArea.x + 10, paramArea.y + 10, 20, BLACK);

  int startX = paramArea.x + 10;
  int startY = paramArea.y + 40;
  int boxW = 80, boxH = 30, spacing = 40;
  Rectangle boxCap = {startX + 150, startY, boxW, boxH};
  Rectangle boxTime = {startX + 150, startY + spacing, boxW, boxH};
  Rectangle boxProb = {startX + 150, startY + 2 * spacing, boxW, boxH};

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
  {
    if (CheckCollisionPointRec(GetMousePosition(), boxCap))
      activeBox = 0;
    else if (CheckCollisionPointRec(GetMousePosition(), boxTime))
      activeBox = 1;
    else if (CheckCollisionPointRec(GetMousePosition(), boxProb))
      activeBox = 2;
    else
      activeBox = -1;
  }

  DrawText("Cap. enlace:", startX, startY + 5, 20, DARKGRAY);
  DrawRectangleLinesEx(boxCap, 2, (activeBox == 0) ? RED : DARKGRAY);
  DrawText(capacityText, boxCap.x + 5, boxCap.y + 5, 20, BLACK);

  DrawText("Timeout (s):", startX, startY + spacing + 5, 20, DARKGRAY);
  DrawRectangleLinesEx(boxTime, 2, (activeBox == 1) ? RED : DARKGRAY);
  DrawText(timeoutText, boxTime.x + 5, boxTime.y + 5, 20, BLACK);

  DrawText("Prob. suc. :", startX, startY + 2 * spacing + 5, 20, DARKGRAY);
  DrawRectangleLinesEx(boxProb, 2, (activeBox == 2) ? RED : DARKGRAY);
  DrawText(probText, boxProb.x + 5, boxProb.y + 5, 20, BLACK);

  char *targetText = NULL;
  if (activeBox == 0)
    targetText = capacityText;
  else if (activeBox == 1)
    targetText = timeoutText;
  else if (activeBox == 2)
    targetText = probText;

  if (targetText)
  {
    SetMouseCursor(MOUSE_CURSOR_IBEAM);
    int key = GetCharPressed();
    while (key > 0)
    {
      if ((key >= '0' && key <= '9') || (key == '.' && activeBox != 0))
      {
        if (strlen(targetText) < 7)
        {
          int len = strlen(targetText);
          targetText[len] = (char)key;
          targetText[len + 1] = '\0';
        }
      }
      key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE))
    {
      int len = strlen(targetText);
      if (len > 0)
        targetText[len - 1] = '\0';
    }

    // Atualiza as variáveis globais
    g_link_capacity = atoi(capacityText);
    g_timeout_seconds = atof(timeoutText);
    g_prob_success = atof(probText);

    // Garante limites razoáveis
    if (g_link_capacity < 1)
      g_link_capacity = 1;
    if (g_timeout_seconds < 0.1f)
      g_timeout_seconds = 0.1f;
    if (g_prob_success < 0.0f)
      g_prob_success = 0.0f;
    if (g_prob_success > 1.0f)
      g_prob_success = 1.0f;
  }
  else
  {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }
}
