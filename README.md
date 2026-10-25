# Sorteador para Resolume

Plugin FFGL para o **Resolume Arena/Avenue 7** que faz sorteios de **nomes** ou **números** com animação na tela. Você controla tudo pelo painel do Resolume ou por uma **página web**, aberta no celular, tablet ou em outro computador da mesma rede.

- Sorteio de uma lista de nomes (um por linha) ou de uma faixa de números (com opção de zeros à esquerda, ex.: `007`)
- Opção de não repetir quem já saiu, com histórico que continua salvo depois de fechar o Resolume
- Três animações: **rolagem vertical**, **rolagem horizontal** (letreiro) e **troca no lugar**
- Ajuste de duração, velocidade inicial, curva de desaceleração e quantidade de itens visíveis ao redor
- Escolha de fonte, tamanho, posição, cores (texto, sorteado, contorno e fundo), título e texto de espera
- Até 4 **canais** independentes, ou seja, sorteios diferentes ao mesmo tempo em camadas diferentes
- API HTTP simples para botões de Stream Deck / Bitfocus Companion

## Instalação (Windows)

1. Baixe o `Sorteador.dll` na página de [Releases](https://github.com/osamuelvieira/sorteador-resolume/releases). Se ainda não houver uma release, baixe o artefato **Sorteador-Windows** da última execução em [Actions](https://github.com/osamuelvieira/sorteador-resolume/actions).
2. Copie o arquivo para `Documentos\Resolume Arena\Extra Effects` (ou para a pasta de plugins configurada em *Preferences → Video*).
3. Reinicie o Resolume.
4. Na aba **Sources**, procure por **Sorteador** e arraste para um clip.
5. Na primeira vez, o Windows pode perguntar se o Resolume pode usar a rede. **Permita em redes privadas**, senão o painel web não abre em outros aparelhos.

> No macOS, use o `Sorteador.bundle` (artefato **Sorteador-macOS**) e copie para `~/Documents/Resolume Arena/Extra Effects`.

## Como usar

### Pelo painel web

Com o clip parado na tela de espera, o endereço do painel aparece no rodapé da saída, algo como `Controle web: http://192.168.0.10:8090`. Abra esse endereço em qualquer navegador na mesma rede.

No painel você:

- escolhe o canal (abas no topo)
- cola a lista de nomes ou define a faixa de números
- ajusta animação, visual e textos (as mudanças aparecem na hora)
- clica em **Iniciar sorteio** (ou aperta a barra de espaço) e acompanha o resultado ao vivo
- consulta, copia ou zera o histórico, e remove um sorteado específico, por exemplo se a pessoa não estiver presente

Para desligar o endereço na tela, desmarque **Mostrar endereço deste painel na tela de espera**.

### Pelo painel do Resolume

Os mesmos ajustes aparecem como parâmetros do clip, em três grupos (*Sorteio*, *Animação* e *Visual*), além dos botões:

| Parâmetro | O que faz |
|---|---|
| **Iniciar sorteio** | Começa a animação e sorteia |
| **Limpar tela** | Volta para o texto de espera |
| **Zerar histórico** | Todos voltam a poder ser sorteados |
| **Canal** | Qual sorteio este clip mostra |
| **Lista (separe com ;)** | Edição rápida da lista em uma linha: `Ana; Bruno; Carla` |

Os botões podem ser mapeados para MIDI, OSC ou teclado como qualquer parâmetro do Resolume.

Painel web e Resolume mostram sempre os mesmos valores: o que você muda em um aparece no outro.

### Canais

Cada canal tem sua lista, configuração e histórico. Clips no **mesmo canal** mostram exatamente o mesmo sorteio, em sincronia (útil para exibir em mais de uma camada ou saída). Para dois sorteios diferentes, coloque os clips em canais diferentes.

### Stream Deck / Companion

Qualquer aparelho que faça uma requisição HTTP pode disparar ações:

```
http://IP-DO-COMPUTADOR:8090/api/start?canal=1          iniciar
http://IP-DO-COMPUTADOR:8090/api/clear?canal=1          limpar tela
http://IP-DO-COMPUTADOR:8090/api/reset-history?canal=1  zerar histórico
http://IP-DO-COMPUTADOR:8090/api/state?canal=1          estado atual (JSON)
```

Mudar configurações: `POST /api/config?canal=1` com um JSON parcial, por exemplo `{"duration": 10, "animation": 1}`.

## Onde ficam os dados

As configurações e o histórico de todos os canais ficam em `%APPDATA%\SorteadorResolume\estado.json` (Windows) ou `~/Library/Application Support/SorteadorResolume/estado.json` (macOS). Nesse arquivo também dá para trocar a porta do painel web (`"port": 8090`). Se a porta estiver ocupada, o plugin tenta as próximas automaticamente (8091, 8092…).

## Dicas

- **Fundo transparente:** com *Opacidade do fundo* em 0, o sorteio fica sobreposto às outras camadas.
- **Desaceleração:** valores altos fazem os nomes passarem rápido e demorarem a parar, o que dá mais suspense.
- **Fontes:** o painel web lista as fontes instaladas no computador do Resolume. Acentos e cedilha são suportados.
- **Nomes longos** são reduzidos automaticamente para caber na tela.

## Compilando

Pré-requisitos: CMake 3.20+, e Visual Studio 2022 (Windows) ou Xcode Command Line Tools (macOS).

```bash
git clone --recursive https://github.com/osamuelvieira/sorteador-resolume.git
cd sorteador-resolume
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release      # testes da lógica do sorteio
```

O resultado fica em `build/Release/Sorteador.dll` (Windows) ou `build/Sorteador.bundle` (macOS). Cada push no GitHub compila as duas versões automaticamente. Criar uma tag `v1.0.0` publica uma release com os arquivos.

No macOS existe ainda o `sorteador_test_host`, um host mínimo que carrega o plugin sem o Resolume e salva quadros da animação como imagens:

```bash
./build/sorteador_test_host build/Sorteador.bundle /tmp/quadros        # tela de espera + um sorteio completo
./build/sorteador_test_host build/Sorteador.bundle /tmp/quadros 60     # fica 60 s no ar para testar o painel web
```

### Estrutura

```
src/
  SorteadorPlugin.*   plugin FFGL: parâmetros do Resolume e desenho de cada quadro
  Channel.*           lógica do sorteio: lista, números, não repetir, roleta e animação
  ChannelManager.*    estado compartilhado entre instâncias, salvamento em disco
  WebServer.*         servidor HTTP embutido e API JSON
  TextRenderer.*      texto em OpenGL com fonte SDF (nítido em qualquer tamanho, com contorno)
  Config.*            configurações e conversão para JSON
  Platform.*          caminhos, fontes do sistema, arquivos
web/index.html        painel web (embutido no plugin durante a compilação)
tests/                testes da lógica do sorteio
tools/TestHost.cpp    host de teste para macOS
external/ffgl         SDK oficial FFGL da Resolume (submódulo)
third_party/          cpp-httplib, nlohmann/json, stb_truetype
```

## Créditos

- [FFGL SDK](https://github.com/resolume/ffgl): Resolume / FreeFrame (licença BSD)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib): Yuji Hirose (MIT)
- [nlohmann/json](https://github.com/nlohmann/json): Niels Lohmann (MIT)
- [stb_truetype](https://github.com/nothings/stb): Sean Barrett (domínio público / MIT)
