# NodeSystemComunication

Sistema de comunicação **peer-to-peer** entre nós, implementado em **C**, que utiliza uma **árvore binária de busca (BST)** para roteamento de pacotes.  
O projeto busca simular e analisar o comportamento de uma rede distribuída, com controle de **vazão**, **latência** e gerenciamento de **filas**.

## 🧠 Visão Geral

O objetivo do projeto é criar um ambiente modular para experimentos com comunicação entre nós em topologias dinâmicas, permitindo o estudo de métricas de desempenho e algoritmos de roteamento.  
Cada nó é capaz de enviar e receber pacotes, armazenar mensagens em filas locais e encaminhá-las de forma otimizada segundo sua posição na BST.

### Funcionalidades principais
- Comunicação peer-to-peer entre múltiplos nós.
- Estrutura de **BST** para roteamento de pacotes.
- Gerenciamento de **filas de pacotes** por nó.
- Cálculo de **vazão** e **latência média**.
- Visualização de logs e métricas no terminal (interface gráfica em desenvolvimento com raylib/raygui).

## 🧩 Estrutura do Projeto
```
NodeSystemComunication/
│
├── include/               # Arquivos de cabeçalho (.h)
├── src/                   # Implementações (.c)
├── main.c                 # Ponto de entrada do programa
├── README.md              # Este arquivo
└── (futuro) Makefile      # Build system planejado
```

## ⚙️ Execução

Como o projeto ainda não possui Makefile, a compilação é feita diretamente via terminal:

```bash
gcc main.c -o node_system -lm
./node_system
```

> Certifique-se de ter o GCC instalado e configurado no PATH.

## 🔄 Funcionamento

Cada nó é representado por uma estrutura que contém informações como:
- Identificador único (ID);
- Ponteiros para filhos esquerdo e direito (BST);
- Fila de pacotes de entrada/saída;
- Métricas locais (tempo médio de envio/recebimento, latência, vazão).

Durante a execução:
1. A BST é construída para representar o caminho de roteamento entre os nós.
2. Pacotes são gerados e inseridos nas filas dos nós origem.
3. A função de roteamento utiliza a BST para determinar o caminho ótimo.
4. São medidas métricas de desempenho (ex: tempo médio de entrega).

## 📈 Métricas
- **Vazão (Throughput):** número de pacotes entregues por unidade de tempo.  
- **Latência:** tempo médio de entrega de pacotes entre dois nós.  
- **Taxa de entrega:** percentual de pacotes que chegaram ao destino.  

Essas métricas podem ser exibidas ao final da simulação ou armazenadas para análise posterior.

## 🚀 Próximos Passos
- Implementar o **Makefile**.
- Adicionar interface gráfica com **raylib/raygui**.
- Criar suporte a múltiplas threads (para simular tráfego paralelo).
- Exportar métricas em formato CSV/JSON para análise.

## 🤝 Contribuindo
1. Faça um fork do projeto.
2. Crie uma branch para sua modificação: `git checkout -b feature/nome-da-feature`
3. Commit suas mudanças: `git commit -m "Descrição do commit"`
4. Envie o pull request.


Autor: [Gabriel Bozelli](https://github.com/gbozelli)  
Engenharia de Telecomunicações — UNESP  
Bolsista FAPESP | Pesquisador em Redes e Sistemas de Comunicação
