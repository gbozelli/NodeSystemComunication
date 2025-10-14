#ifndef DEFINITIONS_H
#define DEFINITIONS_H

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
  bool enabled;
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
// DECLARAÇÃO DE VARIÁVEIS GLOBAIS (extern)
//====================================================================================
// 'extern' diz ao compilador que essas variáveis existem, mas são definidas em outro arquivo (.c).
// Isso evita erros de "definição múltipla" do linker.

// --- PARÂMETROS DA SIMULAÇÃO ---
extern int g_link_capacity;
extern float g_timeout_seconds;
extern float g_prob_success;

// --- DADOS DA SIMULAÇÃO ---
extern Node nodes[MAX_NODES];
extern int nodeCount;
extern AsyncMessage messages[MAX_MESSAGES];
extern int messageCount;

extern Network pathfindingNetwork;
extern Network capacityNetwork;

extern Action actionStack[100];
extern int actionTop;

// --- ESTATÍSTICAS ---
extern long long total_latency_ticks;
extern int completed_messages_count;
extern int total_retransmissions;

#endif // DEFINITIONS_H