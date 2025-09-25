# Projeto 1 – Computação Visual  
Universidade Presbiteriana Mackenzie – Ciência da Computação

---

## 👥 Contribuições do grupo

**Cláudio Dias Alves – RA: 10403569**  
- Refatoração da inicialização do SDL3, logs e tratamento de erros.
- Implementação da janela principal e da janela secundária.
- Renderização do histograma e implementação do botão interativo.
- Organização do README do projeto.

**Daniel Rubio Camargo – RA: 10408823**  
- Verificação se a imagem está em escala de cinza.
- Conversão da imagem colorida para escala de cinza.
- Estruturas e funções para armazenar intensidades e aplicar equalização.
- Preparação inicial do ambiente do projeto.

**João Pedro Mascaro Baccelli – RA: 10224004**  
- Ajustes no histograma.
- Implementação e correção da exibição de textos com fonte (SDL_ttf).
- Funcionalidade de salvar a imagem processada.
- Integração geral e melhorias na usabilidade.
- Criação do README

---

## 📌 Descrição
Este projeto implementa um software de **processamento de imagens** em linguagem C, utilizando a biblioteca **SDL3** e as extensões **SDL_image** e **SDL_ttf**.  

O programa permite:  
- Carregar imagens nos formatos PNG, JPG e BMP.  
- Converter automaticamente para escala de cinza (quando necessário).  
- Calcular e exibir o histograma da imagem.  
- Mostrar estatísticas de intensidade e contraste.  
- Equalizar o histograma de forma interativa.  
- Salvar a imagem resultante em disco.  

O trabalho foi desenvolvido em grupo como parte da disciplina **Computação Visual** (Prof. André Kishimoto).  

---

## 🎯 Funcionalidades
1. **Carregamento de imagem**  
   - Leitura de imagens em formatos comuns.  
   - Tratamento de erros para arquivos inexistentes ou inválidos.  

2. **Conversão para escala de cinza**  
   - Verifica se a imagem já está em tons de cinza.  
   - Se não, converte usando a fórmula:  
     \[
     Y = 0.2125R + 0.7154G + 0.0721B
     \]  

3. **Interface gráfica (GUI)**  
   - **Janela principal**: exibe a imagem em processamento, ajustada ao seu tamanho.  
   - **Janela secundária**: exibe o histograma e contém um botão interativo.  

4. **Histograma e estatísticas**  
   - Exibição gráfica do histograma (0–255 intensidades).  
   - Cálculo e exibição da **média** de intensidade (escura, média, clara).  
   - Cálculo e exibição do **desvio padrão** (contraste baixo, médio, alto).  

5. **Equalização do histograma**  
   - Botão interativo que alterna entre a imagem original (em tons de cinza) e a equalizada.  
   - O histograma e as estatísticas são recalculados e atualizados conforme a imagem exibida.  
   - O texto e a cor do botão mudam conforme o estado (original/equalizado).  

6. **Salvar imagem**  
   - Pressionar a tecla `S` salva a imagem exibida na janela principal em `output_image.png`.  
   - Caso já exista, o arquivo é sobrescrito.  

---

## 🧩 Verificação das bibliotecas
`pkg-config --modversion sdl3 sdl3-image sdl3-ttf` 
`pkg-config --cflags sdl3 sdl3-image sdl3-ttf`  

---

## 💻 Compilação
`gcc -std=c99 -O2 -Wall -Wextra -o main.exe main.c $(pkg-config --cflags --libs sdl3 sdl3-image sdl3-ttf) -lm`

`./main.exe caminho/para/imagem.png` 

---

## 📂 Estrutura do projeto
```
processamento-de-imagens/  
│── DejaVuSans.ttf   # Fonte usada para renderizar textos nas imagens ou gerar histogramas    
│── main.c           # Código-fonte principal 
└── README.md        # Documentação do projeto    
```

---

## 📑 Referências
Documentação oficial da SDL3  
Slides da disciplina Computação Visual – Prof. André Kishimoto  
