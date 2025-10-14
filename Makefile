# Compilador a ser usado
CC = gcc

# Nome do executável final
TARGET = app

# Diretórios
SRCDIR = src
INCDIR = include

# Encontra todos os arquivos .c no diretório src
SRCS = $(wildcard $(SRCDIR)/*.c)
# Gera uma lista de arquivos objeto (.o) correspondentes
OBJS = $(SRCS:.c=.o)

# --- CONFIGURAÇÃO PARA RAYLIB ---
# Altere este caminho para o local onde você extraiu o Raylib
RAYLIB_PATH = C:/raylib

# Flags do compilador:
# -I$(INCDIR) -> Adiciona a nossa pasta 'include' aos caminhos de busca por headers
# -I$(RAYLIB_PATH)/include -> Adiciona a pasta 'include' do Raylib
# -Wall -> Ativa todos os avisos (warnings), é uma boa prática
CFLAGS = -I$(INCDIR) -I$(RAYLIB_PATH)/include -Wall

# Flags do linker:
# -L$(RAYLIB_PATH)/lib -> Informa ao linker onde encontrar as bibliotecas do Raylib
# -lraylib -> Vincula a biblioteca Raylib
# -lopengl32 -lgdi32 -lwinmm -> Bibliotecas do Windows necessárias para o Raylib
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib -lopengl32 -lgdi32 -lwinmm

# A regra 'all' é a regra padrão, que será executada se você apenas digitar 'make'
all: $(TARGET)

# Regra para vincular (link) todos os arquivos objeto e criar o executável final
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Regra genérica para compilar um arquivo .c em um arquivo .o
# $< é o nome do primeiro pré-requisito (o arquivo .c)
# $@ é o nome do alvo (o arquivo .o)
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para limpar os arquivos gerados (objetos e executável)
# Útil para forçar uma reconstrução completa
clean:
	del $(subst /,\,$(SRCS:.c=.o)) $(TARGET).exe

# 'PHONY' informa ao make que 'all' e 'clean' não são nomes de arquivos reais
.PHONY: all clean

