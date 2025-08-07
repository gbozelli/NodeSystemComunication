// main.cpp
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include "Network.hpp"

int main()
{
  sf::RenderWindow window(sf::VideoMode(1280, 720), "Rede de Nós");
  ImGui::SFML::Init(window);

  Network net;

  std::string newNodeId;
  std::string parentId, childId;
  std::string fromNode, toNode;
  std::vector<std::string> lastPath;

  sf::Clock deltaClock;
  while (window.isOpen())
  {
    sf::Event event;
    while (window.pollEvent(event))
    {
      ImGui::SFML::ProcessEvent(event);
      if (event.type == sf::Event::Closed)
        window.close();
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    ImGui::Begin("Controle da Rede");

    ImGui::InputText("Novo Nó", &newNodeId);
    if (ImGui::Button("Adicionar Nó"))
    {
      net.addNode(newNodeId);
    }

    ImGui::InputText("Pai", &parentId);
    ImGui::InputText("Filho", &childId);
    if (ImGui::Button("Conectar"))
    {
      net.connectParentChild(parentId, childId);
    }

    ImGui::Separator();

    ImGui::InputText("Origem", &fromNode);
    ImGui::InputText("Destino", &toNode);
    if (ImGui::Button("Enviar Mensagem"))
    {
      lastPath = net.sendMessage(fromNode, toNode);
    }

    if (!lastPath.empty())
    {
      ImGui::Text("Caminho:");
      for (auto &step : lastPath)
      {
        ImGui::BulletText("%s", step.c_str());
      }
    }

    ImGui::End();

    window.clear();
    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
  return 0;
}
