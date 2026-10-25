# Sorteador para Resolume

Plugin FFGL para o **Resolume Arena/Avenue 7** que faz sorteios de **nomes** ou **números** com animação e efeitos na tela. Você controla tudo pelo painel do Resolume ou por uma **página web**, aberta no celular, tablet ou em outro computador da mesma rede.

![Tela de espera, nomes passando e resultado com efeitos](docs/img/saida-hero.png)

## Sumário

- [Recursos](#recursos)
- [Instalação](#instalação)
- [Primeiro sorteio em 5 minutos](#primeiro-sorteio-em-5-minutos)
- [O painel web](#o-painel-web)
- [O painel do Resolume](#o-painel-do-resolume)
- [Animações](#animações)
- [Efeitos no resultado](#efeitos-no-resultado)
- [Sorteio de números](#sorteio-de-números)
- [Sorteio de teste (prévia)](#sorteio-de-teste-prévia)
- [Canais](#canais)
- [Tutoriais](#tutoriais)
- [API HTTP (Stream Deck / Companion)](#api-http-stream-deck--companion)
- [Onde ficam os dados](#onde-ficam-os-dados)
- [Solução de problemas](#solução-de-problemas)
- [Compilando](#compilando)
- [Créditos](#créditos) · [Licença](#licença)

## Recursos

**O que sortear**

- Lista de nomes (um por linha) ou faixa de números, com opção de completar com zeros à esquerda (`007`)
- Opção de **não repetir** quem já saiu; o painel mostra quantos ainda estão disponíveis
- **Histórico** com data e horário de cada sorteado, salvo mesmo depois de fechar o Resolume. Dá para copiar a lista, zerar tudo ou remover um sorteado específico

**Como aparece na tela**

- Três animações: **rolagem vertical**, **rolagem horizontal** (letreiro) e **troca no lugar**
- Duração, velocidade inicial, curva de desaceleração e quantidade de itens visíveis ao redor
- Fonte (qualquer fonte instalada no computador), tamanho, posição vertical e contorno
- Cores do texto, do sorteado, do contorno e do fundo, com fundo transparente para sobrepor a outras camadas
- Título no topo, texto de espera e opção de deixar todos os textos em maiúsculas (com acentos)
- Texto nítido em qualquer resolução, e nomes longos reduzidos automaticamente para caber

**Efeitos quando o nome para**

- Impacto, flash, onda de choque, faíscas, confete e brilho, cada um liga e desliga
- Intensidade geral de 20% a 200%

**Controle**

- **Painel web** responsivo, com telinha **Ao vivo** que reproduz a saída com fidelidade (mesma fonte, animação e efeitos)
- Todos os ajustes também como **parâmetros do clip** no Resolume, mapeáveis para MIDI, OSC e teclado
- Painel web e Resolume sempre sincronizados
- **Sorteio de teste** para ensaiar sem ir para o ar e sem contar no histórico
- Até **4 canais** independentes, para sorteios diferentes ao mesmo tempo
- **API HTTP** para Stream Deck, Bitfocus Companion ou qualquer aparelho que faça uma requisição web

## Instalação

### Windows

1. Baixe o `Sorteador.dll` na página de [Releases](https://github.com/osamuelvieira/sorteador-resolume/releases). Se ainda não houver uma release, baixe o artefato **Sorteador-Windows** da última execução em [Actions](https://github.com/osamuelvieira/sorteador-resolume/actions).
2. Copie o arquivo para `Documentos\Resolume Arena\Extra Effects` (ou para a pasta de plugins configurada em *Preferences → Video*).
3. Reinicie o Resolume.
4. Na aba **Sources**, procure por **Sorteador** e arraste para um clip.
5. Na primeira vez, o Windows pode perguntar se o Resolume pode usar a rede. **Permita em redes privadas**, senão o painel web não abre em outros aparelhos.

### macOS

Baixe o `Sorteador-macOS.zip` (ou o artefato **Sorteador-macOS**), extraia e copie o `Sorteador.bundle` para `~/Documents/Resolume Arena/Extra Effects`. Depois siga os passos 3 e 4 acima.

## Primeiro sorteio em 5 minutos

1. **Coloque o plugin num clip.** Na aba *Sources* do Resolume, arraste **Sorteador** para um clip vazio e dispare o clip. A saída mostra o texto de espera, `SORTEIO`.
2. **Abra o painel web.** Nos parâmetros do clip, clique em **Abrir painel web**. O navegador abre direto no canal desse clip.
3. **Cole a lista.** Em *O que sortear*, deixe **Lista de nomes** selecionado e cole os nomes, um por linha (dá para copiar uma coluna do Excel ou do Google Planilhas).
4. **Dê um título.** Em *Textos*, escreva o título, por exemplo `Sorteio de brindes`. A telinha *Ao vivo* já mostra como fica.
5. **Sorteie.** Clique em **Iniciar sorteio** ou aperte a **barra de espaço**. O nome sorteado vai para o *Histórico* com data e horário.
6. Para voltar à tela de espera, clique em **Limpar tela**.

| Sorteando | Resultado |
|---|---|
| ![Painel web durante o sorteio](docs/img/painel-sorteando.png) | ![Painel web mostrando o nome sorteado com confete](docs/img/painel-resultado.png) |

## O painel web

Abra pelo botão **Abrir painel web** no clip, ou digite o endereço do computador do Resolume em qualquer navegador da mesma rede (por exemplo `http://192.168.0.10:8090`). Para saber o endereço, ligue **Mostrar endereço deste painel na tela de espera**: ele aparece no rodapé da saída.

<p align="center"><img src="docs/img/painel-web.png" alt="Painel web completo" width="640"></p>

| Seção | O que tem |
|---|---|
| **Abas de canal** (topo) | Troca entre os canais 1 a 4. O indicador à direita mostra se o painel está conectado ao plugin |
| **Ao vivo** | Reprodução fiel da saída, com barra de progresso e estado (*Aguardando*, *Sorteando…*, *Resultado*). Botões **Iniciar sorteio** e **Limpar tela** |
| **Sorteio de teste** | Telinha própria do teste e botões **Sorteio de teste** e **Limpar teste** ([veja abaixo](#sorteio-de-teste-prévia)) |
| **O que sortear** | Lista de nomes ou faixa de números, *Completar com zeros*, *Não repetir* e quantos ainda estão disponíveis |
| **Animação** | Tipo de animação, duração, velocidade inicial, desaceleração, itens visíveis ao redor e os efeitos |
| **Visual** | Fonte, tamanho, posição vertical, contorno, cores e opacidade do fundo |
| **Textos** | Título, texto de espera, maiúsculas e endereço do painel na tela de espera |
| **Histórico** | Ordem, nome, data e horário de cada sorteado. **×** remove um item, **Copiar lista** copia tudo com os horários, **Zerar histórico** libera todos de novo |

As mudanças aparecem na saída na hora, sem precisar salvar. O painel lembra o último canal usado no navegador, e o endereço `/?canal=2` abre direto no canal 2.

## O painel do Resolume

Os mesmos ajustes aparecem como parâmetros do clip. Painel web e Resolume mostram sempre os mesmos valores: o que você muda em um aparece no outro.

| Parâmetro | O que faz |
|---|---|
| **Iniciar sorteio** | Começa a animação e sorteia |
| **Limpar tela** | Volta para o texto de espera |
| **Zerar histórico** | Todos voltam a poder ser sorteados |
| **Abrir painel web** | Abre o painel no navegador do computador do Resolume, já no canal deste clip |
| **Canal** | Qual sorteio este clip mostra (1 a 4) |
| **Modo prévia (teste)** | Este clip passa a mostrar só o sorteio de teste |
| **Tipo** | Lista de nomes ou Números |
| **Lista (separe com ;)** | Edição rápida da lista em uma linha: `Ana; Bruno; Carla` |

E nos grupos:

- **Sorteio:** número inicial, número final, não repetir
- **Animação:** animação, duração (s), velocidade inicial, desaceleração, itens ao redor, os seis efeitos e a intensidade dos efeitos
- **Visual:** tamanho do texto, posição vertical, cor do texto, cor do sorteado, contorno e sua cor, opacidade e cor do fundo, título, texto de espera, letras maiúsculas, mostrar endereço web

Os botões e os demais parâmetros podem ser mapeados para **MIDI, OSC ou teclado** como qualquer parâmetro do Resolume (clique com o botão direito no parâmetro ou use o modo de mapeamento).

## Animações

![As três animações: rolagem vertical, rolagem horizontal e troca no lugar](docs/img/saida-animacoes.png)

| Animação | Como é |
|---|---|
| **Rolagem vertical** | Os nomes descem como numa roleta, com os vizinhos acima e abaixo em tom mais apagado |
| **Rolagem horizontal** | Os nomes passam da direita para a esquerda, como um letreiro |
| **Troca no lugar** | Um nome por vez no centro, trocando cada vez mais devagar |

Ajustes que valem para todas:

- **Duração:** de 1 a 60 segundos
- **Velocidade inicial:** quantos itens passam por segundo no começo (2 a 100)
- **Desaceleração:** de 1 a 8. Em 1 a velocidade cai de forma constante; em 8 os nomes passam rápido por mais tempo e demoram a parar, o que dá mais suspense
- **Itens visíveis ao redor:** de 0 a 4 vizinhos de cada lado (só nas rolagens)

O sorteado é escolhido no instante em que o sorteio começa, de forma aleatória e uniforme; a animação só mostra o caminho até ele.

## Efeitos no resultado

Para o público perceber na hora que o sorteio terminou, o plugin pode disparar efeitos quando o nome para:

![Efeitos: flash com onda de choque e faíscas, impacto com faíscas, confete com brilho](docs/img/saida-efeitos.png)

| Efeito | O que faz | Padrão |
|---|---|---|
| **Impacto** | O nome dá um "soco" de zoom e a tela treme por um instante | ligado |
| **Flash** | Clarão rápido na tela inteira | ligado |
| **Onda de choque** | Anéis que se expandem a partir do nome | desligado |
| **Faíscas** | Faíscas saem do nome | ligado |
| **Confete** | Chuva de confete colorido por alguns segundos | desligado |
| **Brilho** | Halo pulsando em volta do nome enquanto ele fica na tela | desligado |

**Intensidade** (20% a 200%) aumenta ou diminui todos de uma vez: quantidade de partículas, força do flash e do tremor. Os efeitos ficam no grupo *Animação* do clip e na seção *Animação* do painel web.

## Sorteio de números

Em *O que sortear*, escolha **Números** e defina o número inicial e o final (podem ser negativos). Ligue **Completar com zeros** para que todos tenham a mesma quantidade de dígitos: de 1 a 500, o 47 aparece como `047`.

![Sorteio de números de 001 a 500](docs/img/saida-numeros.png)

Com *Não repetir* ligado, cada número sai uma vez só até você zerar o histórico.

## Sorteio de teste (prévia)

Serve para ensaiar a animação e os efeitos antes do evento, ou conferir um ajuste durante ele, **sem ir para o ar**:

- o teste roda a animação e os efeitos completos, mas **não entra no histórico** e não tira ninguém da lista
- ele aparece **só nos clips com _Modo prévia (teste)_ ligado**. Os clips normais continuam mostrando o sorteio de verdade, então a saída não muda
- num clip em modo prévia, os botões *Iniciar sorteio* e *Limpar tela* passam a valer para o teste, e um aviso **PRÉVIA - TESTE** aparece no canto

No painel web, a seção **Sorteio de teste** tem uma telinha própria, ao lado da **Ao vivo**, que mostra o teste exatamente como o clip de prévia mostra. O passo a passo para montar está no [tutorial de ensaio](#ensaiar-sem-ir-para-o-ar).

## Canais

Cada canal tem sua própria lista, configuração e histórico.

- Clips no **mesmo canal** mostram exatamente o mesmo sorteio, em sincronia. Útil para exibir em mais de uma camada ou saída (por exemplo, telão e monitor de retorno)
- Para dois sorteios diferentes ao mesmo tempo, coloque os clips em **canais diferentes**

## Tutoriais

### Sorteio de brindes num evento

1. Monte a lista no Excel ou Google Planilhas, uma pessoa por linha, e cole em *O que sortear*.
2. Deixe **Não repetir quem já foi sorteado** ligado, para ninguém ganhar duas vezes.
3. Escolha a animação. Para suspense, **Rolagem vertical**, duração de 8 a 10 s e desaceleração 4 ou 5.
4. Ligue os efeitos que combinam com o evento, por exemplo **Confete** e **Brilho** para premiações.
5. Ensaie com o [sorteio de teste](#ensaiar-sem-ir-para-o-ar) antes de abrir as portas.
6. Na hora, use **Iniciar sorteio** para cada brinde e **Limpar tela** entre um e outro.
7. Se o sorteado não estiver presente, clique no **×** ao lado do nome no histórico: ele volta para a lista e você sorteia de novo.
8. No fim, **Copiar lista** copia todos os sorteados com data e horário, para conferência ou ata.

### Sobrepor o sorteio a um vídeo

Com **Opacidade do fundo** em 0 (o padrão), só o texto e os efeitos aparecem, e as camadas de baixo continuam visíveis. Coloque o clip do Sorteador numa camada acima do seu vídeo ou arte de fundo.

<p align="center"><img src="docs/img/saida-transparente.png" alt="Sorteio com fundo transparente; o xadrez representa a transparência" width="640"></p>

Para destacar o texto sobre fundos claros, aumente o **Contorno** ou ligue o **Brilho**. Se preferir um fundo sólido, escolha a **Cor do fundo** e suba a **Opacidade do fundo**.

### Controlar pelo celular

1. Ligue **Mostrar endereço deste painel na tela de espera**. O rodapé da saída passa a mostrar algo como `Controle web: http://192.168.0.10:8090 (canal 1)`.
2. Conecte o celular **na mesma rede Wi-Fi** do computador do Resolume.
3. Abra esse endereço no navegador do celular. Dica: adicione à tela de início para abrir como um app.
4. Desligue a opção do endereço de novo antes de ir para o ar.

Se não abrir, veja [Solução de problemas](#solução-de-problemas).

### Ensaiar sem ir para o ar

1. Duplique o clip do sorteio (no mesmo canal).
2. Na cópia, ligue **Modo prévia (teste)**.
3. Deixe a cópia fora da saída: por exemplo, veja pelo monitor de **Preview** do Resolume, ou ponha numa camada enviada só para o monitor do operador (*Advanced Output*).
4. Clique em **Sorteio de teste** no painel web (ou em *Iniciar sorteio* no clip de prévia).
5. Ajuste animação e efeitos até ficar bom. Os ajustes valem para o sorteio de verdade, mas o teste não conta no histórico.

### Dois sorteios ao mesmo tempo

Por exemplo, um sorteio de nomes no telão principal e um de números da cartela num telão lateral:

1. Crie um clip do Sorteador na camada do telão principal com **Canal** = *Canal 1*.
2. Crie outro clip na camada do telão lateral com **Canal** = *Canal 2*.
3. No painel web, use as abas **Canal 1** e **Canal 2** para configurar cada um. Listas, visuais e históricos são separados.

### Stream Deck e Bitfocus Companion

**Companion:** adicione uma conexão **Generic HTTP** e crie botões com a ação *GET* para os endereços da [API](#api-http-stream-deck--companion), por exemplo `http://192.168.0.10:8090/api/start?canal=1`.

**Stream Deck:** use um plugin de requisição web (como *API Ninja* ou *Web Requests*) com os mesmos endereços.

Sugestão de botões: *Sortear*, *Limpar tela*, *Teste* e *Limpar teste*.

## API HTTP (Stream Deck / Companion)

Todas as rotas aceitam `GET` ou `POST` e o parâmetro `canal` (1 a 4, padrão 1). As ações respondem com o estado atual do canal em JSON.

| Rota | O que faz |
|---|---|
| `/api/start?canal=1` | Inicia o sorteio |
| `/api/clear?canal=1` | Limpa a tela (volta para o texto de espera) |
| `/api/reset-history?canal=1` | Zera o histórico |
| `/api/history/remove?canal=1&index=0` | Remove um sorteado do histórico (`index` começa em 0) |
| `/api/test?canal=1` | Sorteio de teste (só nos clips em modo prévia) |
| `/api/test/clear?canal=1` | Limpa o teste |
| `/api/state?canal=1` | Estado atual: fase, sorteado, histórico, disponíveis e configuração |
| `/api/fonts` | Fontes instaladas no computador |

Para mudar configurações, envie `POST /api/config?canal=1` com um JSON parcial. Só as chaves enviadas mudam:

```bash
curl -X POST "http://192.168.0.10:8090/api/config?canal=1" \
  -H "Content-Type: application/json" \
  -d '{"title": "Sorteio de brindes", "duration": 10, "animation": 1, "fxConfetti": true}'
```

| Chave | Valores |
|---|---|
| `mode` | `0` lista de nomes, `1` números |
| `listText` | Nomes separados por quebra de linha (`\n`) |
| `numMin`, `numMax`, `zeroPad` | Faixa de números e completar com zeros |
| `noRepeat` | Não repetir quem já saiu |
| `animation` | `0` vertical, `1` horizontal, `2` troca no lugar |
| `duration`, `speed`, `decel`, `neighbors` | Duração (1 a 60 s), velocidade (2 a 100), desaceleração (1 a 8), itens ao redor (0 a 4) |
| `fontPath`, `textSize`, `posY`, `outline` | Fonte (caminho do arquivo, vazio = padrão), tamanho e posição (0 a 1), contorno (0 a 1) |
| `textColor`, `winnerColor`, `outlineColor`, `bgColor` | Cores como `[r, g, b]` de 0 a 1 |
| `bgOpacity` | Opacidade do fundo (0 a 1) |
| `title`, `idleText`, `uppercase`, `showAddress` | Textos e opções de texto |
| `fxImpact`, `fxFlash`, `fxShockwave`, `fxSparks`, `fxConfetti`, `fxGlow` | Liga/desliga de cada efeito |
| `fxIntensity` | Intensidade dos efeitos (0.2 a 2) |

## Onde ficam os dados

As configurações e o histórico de todos os canais ficam em:

- **Windows:** `%APPDATA%\SorteadorResolume\estado.json`
- **macOS:** `~/Library/Application Support/SorteadorResolume/estado.json`

Copie esse arquivo para fazer backup, ou para levar a configuração para outro computador. Nele também dá para trocar a porta do painel web (`"port": 8090`). Se a porta estiver ocupada, o plugin tenta as próximas automaticamente (8091, 8092…).

## Solução de problemas

**O painel web não abre em outro aparelho**
- Confira se o aparelho está na **mesma rede** do computador do Resolume (redes de convidados costumam isolar os aparelhos).
- No Windows, permita o Resolume no firewall em **redes privadas** e confirme que a rede está marcada como *Privada*.
- Use o endereço exato mostrado na tela de espera: se a porta 8090 estiver ocupada, o plugin usa outra (8091, 8092…).

**"A lista está vazia."** A lista do canal não tem nenhum nome. Confira se você está no canal certo.

**"Todos os itens já foram sorteados." / "Todos os números já foram sorteados."** Com *Não repetir* ligado, todos já saíram. Use **Zerar histórico** para recomeçar, ou remova um sorteado específico.

**O sorteio de teste não aparece** Ele só aparece em clips com **Modo prévia (teste)** ligado. Veja o [tutorial de ensaio](#ensaiar-sem-ir-para-o-ar).

**Um clip mostra outro sorteio** Confira o parâmetro **Canal** de cada clip. Clips no mesmo canal mostram o mesmo sorteio.

**Acentos ou a fonte não aparecem como esperado** Escolha outra fonte no painel web. A lista mostra as fontes instaladas no computador do Resolume.

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

No macOS existe ainda o `sorteador_test_host`, um host mínimo que carrega o plugin sem o Resolume e salva quadros da animação como imagens (as imagens de saída deste README foram feitas com ele):

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
docs/img/             imagens deste README
external/ffgl         SDK oficial FFGL da Resolume (submódulo)
third_party/          cpp-httplib, nlohmann/json, stb_truetype
```

## Créditos

Criado por **Samuel Vieira**: Instagram [@osamuelvieira.tecnica](https://www.instagram.com/osamuelvieira.tecnica/)

Bibliotecas utilizadas:

- [FFGL SDK](https://github.com/resolume/ffgl): Resolume / FreeFrame (licença BSD)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib): Yuji Hirose (MIT)
- [nlohmann/json](https://github.com/nlohmann/json): Niels Lohmann (MIT)
- [stb_truetype](https://github.com/nothings/stb): Sean Barrett (domínio público / MIT)

## Licença

[MIT](LICENSE): livre para usar, modificar e distribuir, mantendo o aviso de copyright.
