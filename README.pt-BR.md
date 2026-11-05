# ocarina-3ds

*[Read in English](README.md)*

Um plugin do Luma3DS para **The Legend of Zelda: Ocarina of Time 3D** que deixa você tocar as músicas da ocarina com
a sua voz, um assobio ou um instrumento de verdade. Tire a ocarina no jogo, assobie ou cantarole uma música no
microfone do 3DS, e o Link toca a música.

- Frase clara: o plugin reconhece uma das 12 músicas e a toca apertando os botões dela por você (os apertos são
  somados à entrada do próprio jogo, então o jogo reage como se você tivesse tocado com as mãos).
- Frase pouco clara, parecida com uma ou mais músicas: o plugin pergunta na **caixa Sim/Não do próprio jogo**,
  `<música>?`. Yes toca essa música; No pergunta pela próxima candidata (até 3), uma de cada vez.
- O plugin não desenha nada na tela, e os botões continuam funcionando como sempre.

Projeto de fã, sem ligação com a Nintendo e sem aprovação dela. Nenhum código, recurso ou dado do jogo está
incluído. Veja [Aspectos legais](#aspectos-legais).

A explicação técnica completa (como funciona por dentro) está no [README em inglês](README.md#how-it-works).

## Requisitos

- **Console:** desenvolvido e testado de ponta a ponta num Nintendo 3DS original ("Old 3DS"): cada mecanismo foi
  conferido antes com uma versão de teste específica (veja [docs/HISTORY.md](docs/HISTORY.md)) e o plugin completo
  foi depois confirmado funcionando nesse console. O 2DS é da mesma família de hardware, mas não foi testado; o
  New 3DS / New 2DS **não foram testados**. Relatos de outros consoles são bem-vindos.
- **Firmware customizado:** [Luma3DS](https://github.com/LumaTeam/Luma3DS) com o **carregador de plugins** ligado
  (menu Rosalina: `L + baixo no direcional + SELECT` → *Plugin Loader* → *Enabled*; continua ligado depois de
  reiniciar).
- **Jogo:** Ocarina of Time 3D, versão **americana (USA)**, ID de título `0004000000033500`, de uma cópia sua obtida
  legalmente. Foi testado com uma cópia cujo código bate com as assinaturas embutidas; a versão/atualização exata
  dessa cópia não foi anotada. Outras regiões (EUR, JPN, ...) e versões não foram testadas; a lista de alvos do
  plugin tem só o ID americano, então o Luma nem o carrega em outras regiões. Se o código não bater, o plugin **não
  altera nada** e escreve o motivo no log; o jogo roda normalmente.

## Instalação

1. Desligue o console e coloque o cartão SD no computador.
2. Crie a pasta `/luma/plugins/0004000000033500/` no cartão (se ainda não existir).
3. Copie `ocarina.3gx` (da página de [releases](../../releases) ou da sua compilação) para essa pasta. Deixe **só
   um** arquivo `.3gx` nela: com vários, o Luma pode carregar o errado.
4. Coloque o cartão de volta, ligue o console, confira que o carregador de plugins está ligado e abra o jogo. Nada
   aparece na tela; é normal.

```
/luma/plugins/0004000000033500/ocarina.3gx
/ocarina/log.txt                    (criado pelo plugin toda vez que o jogo abre)
```

Para desinstalar, apague `ocarina.3gx`. Para conferir o download: `sha256sum -c SHA256SUMS.txt`.

## Como usar

1. Carregue um save que já tenha a ocarina e vá para um lugar onde o Link possa tocar (um campo aberto serve).
2. **Tire a ocarina** e solte o botão.
3. Espere cerca de 1 segundo e **assobie, cantarole ou toque** a música perto do microfone (fica embaixo da tela de
   baixo). Faça notas firmes de uns 0,3 a 0,5 s, com uma pausa pequena entre elas. Depois da última nota, **fique em
   silêncio**.
4. O que pode acontecer:
   - **Frase clara:** cerca de 1,5 s depois o Link toca a música sozinho e o jogo reage.
   - **O plugin está em dúvida:** a caixa do jogo pergunta `<nome da música>?` com **Yes / No**. Escolha com o
     direcional e confirme com **A**. Yes toca a música (uns 1,5 s depois); No pergunta pela próxima candidata, se
     houver (no máximo 3 perguntas). Recusando todas, nada acontece.
   - **Não pareceu nenhuma música:** nada acontece; é só tentar de novo.
5. Você continua podendo tocar com os botões. Enquanto você segura um botão da ocarina, o microfone é ignorado e só
   volta a ouvir 1 segundo depois de soltar. Ele também é ignorado com a caixa aberta, enquanto o Link toca e por 1
   segundo depois.

Dicas: **assobiar funciona melhor**; **notas repetidas precisam de uma pausa clara** de uns 0,3 s (Serenade of Water
e Nocturne of Shadow têm duas notas iguais seguidas); **qualquer tom serve** (só contam os intervalos entre as
notas); **ambiente silencioso** (TV, conversa ou música perto do console podem virar notas falsas; no pior caso
aparece uma pergunta à toa, é só responder No).

## As 12 músicas

Notas como a ocarina do jogo toca (o plugin só compara intervalos, então qualquer transposição vale). Botões:
Ré4 (D4) = L, Fá4 (F4) = R, Lá4 (A4) = Y, Si4 (B4) = X, Ré5 (D5) = A.

| Música | Notas | Botões |
|---|---|---|
| Zelda's Lullaby | Si4 Ré5 Lá4 Si4 Ré5 Lá4 | X A Y X A Y |
| Epona's Song | Ré5 Si4 Lá4 Ré5 Si4 Lá4 | A X Y A X Y |
| Saria's Song | Fá4 Lá4 Si4 Fá4 Lá4 Si4 | R Y X R Y X |
| Sun's Song | Lá4 Fá4 Ré5 Lá4 Fá4 Ré5 | Y R A Y R A |
| Song of Time | Lá4 Ré4 Fá4 Lá4 Ré4 Fá4 | Y L R Y L R |
| Song of Storms | Ré4 Fá4 Ré5 Ré4 Fá4 Ré5 | L R A L R A |
| Minuet of Forest | Ré4 Ré5 Si4 Lá4 Si4 Lá4 | L A X Y X Y |
| Bolero of Fire | Fá4 Ré4 Fá4 Ré4 Lá4 Fá4 Lá4 Fá4 | R L R L Y R Y R |
| Serenade of Water | Ré4 Fá4 Lá4 Lá4 Si4 | L R Y Y X |
| Nocturne of Shadow | Si4 Lá4 Lá4 Ré4 Si4 Lá4 Fá4 | X Y Y L X Y R |
| Requiem of Spirit | Ré4 Fá4 Ré4 Lá4 Fá4 Ré4 | L R L Y R L |
| Prelude of Light | Ré5 Lá4 Ré5 Lá4 Si4 Ré5 | A Y A Y X A |

## O log (`/ocarina/log.txt`)

O arquivo é recriado (apagado) toda vez que o jogo abre; copie antes de abrir o jogo de novo. Com a ocarina aberta
o log fica na memória; ele só é gravado no cartão **depois que você guarda a ocarina**, porque cada gravação trava
o jogo por uns 25 ms. Ele contém:

- **Cabeçalho:** versão, endereços achados pelas assinaturas, estado dos ganchos, a linha do gancho do controle
  (`pad=... padscan=... padhook=...`), o microfone (`mic=... stage=0` = funcionando) e a linha `plan: ... -> ACTIVE`,
  ou `-> REFUSED (nothing patched)` com os códigos do motivo nas linhas acima;
- `sess#N open` / `sess#N close ...`: cada abertura/fechamento da ocarina, com estatísticas (notas, frases,
  músicas, perguntas, respostas, tempo de análise `ana_max=...us`, ruído `floor=`, `peak=`);
- `phrase ... c/ms@ms= 7726/500@0 ... -> SONG|AMBIG|TIMEOUT|CLOSED`: as notas ouvidas em cada frase (altura em
  cents / duração em ms @ início em ms) e a decisão;
- `ask` / `dlg ...` / `answer ... res=YES|NO`: cada pergunta e a sua resposta;
- `play ...`, `replay start` / `replay end`: a música tocada pelo plugin;
- `overflow`, `anomaly ...` / `PANIC ...`: algo inesperado.

O log para de crescer em 2 MiB (o plugin continua funcionando) e não contém áudio.

## Limitações conhecidas

- **As 12 músicas estão sempre liberadas**, mesmo as que o Link ainda não aprendeu (aí o Link toca as notas e o
  jogo não faz nada).
- **A música e os efeitos do próprio jogo podem vazar para o microfone** e virar notas falsas, principalmente com o
  alto-falante alto. Fones ou volume mais baixo ajudam.
- **Cantarolar/cantar não foi testado no console**: o detector foi ajustado com gravações de assobio de uma pessoa
  só e testado de ponta a ponta com sons sintéticos.
- **Notas muito agudas** (assobio fino acima de ~1400 Hz) podem sair uma oitava abaixo.
- **Sem a Scarecrow's Song** (ela é definida pelo jogador; só as 12 músicas fixas são reconhecidas).
- Notas repetidas precisam de uma pausa de uns 0,3 s.
- A caixa mostra sempre **"Yes" / "No" em inglês**, e os nomes das músicas em inglês.
- Só a versão americana é suportada.

## Se der problema

- **O console congela ao apertar HOME:** provavelmente é outro plugin (antigo ou de teste); esta versão liga a
  flag *EventsSelfManaged*. Deixe só este `ocarina.3gx` na pasta. (A correção segue o código do carregador do Luma;
  uma saída limpa pelo HOME com esta versão exata ainda não foi confirmada no console. Se congelar, avise.)
- **Nada acontece e não existe `/ocarina/log.txt`:** o plugin não carregou. Confira o carregador de plugins no
  Rosalina, o nome exato da pasta `/luma/plugins/0004000000033500/`, que só há um `.3gx` nela e que o jogo é o USA.
- **Há log, mas `plan:` termina em `REFUSED`:** o código do seu jogo não bate com as assinaturas (outra versão ou
  região) ou uma checagem falhou; nada foi alterado. Abra uma issue com o log.
- **`ACTIVE`, mas nenhuma linha `phrase`:** o microfone não ouviu notas firmes o bastante. Assobie mais perto e mais
  alto, segure mais cada nota e veja `floor=`/`peak=` nas linhas `sess#`.
- **Toca a música errada:** mande o log e diga qual música você quis tocar.
- **A caixa ficou presa:** espere uns 20 segundos; um cão de guarda fecha a caixa e restaura o estado do jogo.
- **`PANIC` no log:** depois de 3 anomalias ou 2 falhas da caixa numa sessão, o plugin se desliga até o jogo
  reiniciar. O jogo continua normal, só sem o microfone. Depois de uma falha da caixa, nenhuma pergunta é feita até
  você guardar a ocarina e tirar de novo.
- **O jogo travou:** fotografe as duas telas (com o texto de erro, se houver), anote o que estava acontecendo,
  reinicie **sem** abrir o jogo e mande o `/ocarina/log.txt`.

## Compilação e testes

Precisa do devkitPro (devkitARM + libctru) e do **3gxtool** (MIT, <https://gitlab.com/thepixellizeross/3gxtool>;
na máquina de desenvolvimento veio pelo pacote pacman `3gxtool` 1.3 do repositório `[thepixellizeross-win]` de
ThePixellizerOSS, instalado em `$DEVKITPRO/tools/bin`). O CTRPluginFramework **não** é necessário.

```sh
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
make                                   # -> ocarina.3gx
cmake -S . -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
python3 -I tools/test_make_signatures.py
```

No Windows com o Visual Studio 2022 Build Tools: `run-tests.cmd`. Dados opcionais (nunca faça commit deles):
`OOT3D_CANONICAL_CODE` (imagem de código extraída da **sua** cópia do jogo; liga as checagens de assinatura e é
necessária para gerar `src/sigs.h`) e `OCARINA_RECORDINGS_DIR` (pasta com gravações `rec1.raw`/`rec2.raw`). Sem eles
essas checagens mostram `SKIP` e os testes passam. Detalhes no [README em inglês](README.md#building).

## Contribuir

Issues e pull requests são bem-vindos. Nunca faça commit de dados do jogo (imagens de código, ROMs, CIAs, dumps) nem
de gravações de áudio. Mantenha o plugin sem libc, sem float, sem `.bss`. Logs de sessões reais (com uma frase sobre
o que você tentou) são os relatos mais úteis, principalmente de outras regiões/versões, do New 3DS e de quem
cantarola.

## Aspectos legais

Projeto de fã não oficial, sem ligação com a Nintendo e sem aprovação ou patrocínio dela. "The Legend of Zelda",
"Ocarina of Time" e "Nintendo 3DS" são marcas da Nintendo.

Nenhum código, recurso ou dado do jogo é distribuído. `src/sigs.h` contém padrões curtos de instruções ARM do
código do jogo, com partes mascaradas, usados só para *achar* funções no seu próprio jogo enquanto ele roda; o
plugin e as ferramentas só funcionam com uma cópia do jogo que você mesmo fornece. Use um jogo obtido legalmente.

Use por sua conta e risco. Firmware customizado e plugins mudam como o console roda os programas; isso pode afetar
a garantia e os serviços online. O software é fornecido "no estado em que se encontra", sem garantia (veja
[LICENSE](LICENSE)). O código deste repositório usa a licença MIT ("OcarinaMod contributors"); as ferramentas de
terceiros usadas para compilar ou rodar não estão incluídas (veja a seção NOTICE do arquivo de licença).
