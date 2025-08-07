// Network.h
#pragma once
#include "Node.hpp"
#include <memory>
#include <unordered_map>

class Network
{
public:
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;

  void addNode(const std::string &id)
  {
    nodes[id] = std::make_shared<Node>(id);
  }

  void connectParentChild(const std::string &parentId, const std::string &childId)
  {
    auto parent = nodes[parentId];
    auto child = nodes[childId];
    if (parent && child)
      parent->addChild(child);
  }

  std::vector<std::string> sendMessage(const std::string &fromId, const std::string &toId)
  {
    std::vector<std::string> path;
    std::set<std::string> visited;
    if (nodes[fromId]->sendMessage(toId, visited, path))
    {
      return path;
    }
    return {}; // Falha no envio
  }

  void setPriorityTable(const std::string &nodeId, const std::map<std::string, int> &table)
  {
    if (nodes.count(nodeId))
    {
      nodes[nodeId]->priorityTable = table;
    }
  }
};
