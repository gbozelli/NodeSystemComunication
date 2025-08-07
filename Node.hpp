// Node.h
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>

class Node : public std::enable_shared_from_this<Node>
{
public:
  std::string id;
  std::shared_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  // Tabela de prioridades (ID do nó → prioridade)
  std::map<std::string, int> priorityTable;

  Node(const std::string &id) : id(id) {}

  void addChild(std::shared_ptr<Node> child)
  {
    children.push_back(child);
    child->parent = shared_from_this();
  }

  // Simula envio de mensagem com base na tabela
  bool sendMessage(const std::string &destinationId, std::set<std::string> &visited, std::vector<std::string> &path)
  {
    visited.insert(id);
    path.push_back(id);

    if (id == destinationId)
      return true;

    // Prioridade mais alta = menor valor
    std::vector<std::pair<std::string, int>> sorted(priorityTable.begin(), priorityTable.end());
    std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b)
              { return a.second < b.second; });

    for (auto &[nextId, _] : sorted)
    {
      if (visited.count(nextId))
        continue;

      auto next = findNodeById(nextId);
      if (next && next->sendMessage(destinationId, visited, path))
        return true;
    }

    path.pop_back();
    return false;
  }

  std::shared_ptr<Node> findNodeById(const std::string &targetId)
  {
    if (id == targetId)
      return shared_from_this();

    for (auto &child : children)
    {
      auto res = child->findNodeById(targetId);
      if (res)
        return res;
    }

    if (parent)
    {
      return parent->findNodeById(targetId); // subir na árvore
    }

    return nullptr;
  }
};
