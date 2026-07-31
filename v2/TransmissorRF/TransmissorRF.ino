// --------- ESP32 CODE ----------

#define TX_PIN_1 33 // Módulo de 433MHz (Transmite o bit 1)
#define TX_PIN_0 32 // Módulo de 315MHz (Transmite o bit 0)
#define BTN1_PIN 25
#define BTN2_PIN 26
#define BTN3_PIN 27
#define BTN4_PIN 14

#define TAM_PACOTE_DADOS 7 // Tamanho do pacote de dados em bits

#define TEMPO_BIT_US 1000 // 1000 us por bit (1 ms)
#define TEMPO_MORTO_BIT_US 500   // 500 us de tempo morto (1/2 ms)
#define TEMPO_SILENCIO_MS 10 // 10 ms de silêncio
#define TEMPO_SYNCS_US 333 // 333 us de pulso de sincronização de início
#define TEMPO_SYNCE_US 2000 // 2 ms de pulso de sincronização de fim
#define TEMPO_MORTO_SYNC_US 500 // 500 us de tempo morto entre pulsos de sincronização

// --- VARIÁVEIS DE CONTROLE DE TEMPO E PACOTE ---
unsigned long tempoUltimoEnvio = 0;
uint32_t pacoteDadosPronto = 0; // Armazena o pacote continuamente atualizado

// --- VARIÁVEIS PARA O DEBOUNCE DOS BOTÕES ---
const int pinosBotoes[4] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN};
uint8_t estadoAtualBotoes[4] = {HIGH, HIGH, HIGH, HIGH}; // Inicia HIGH devido ao INPUT_PULLUP
uint8_t ultimoEstadoLido[4] = {HIGH, HIGH, HIGH, HIGH};
unsigned long ultimoTempoDebounce[4] = {0, 0, 0, 0};
const unsigned long tempoDebounce = 50; // 50 ms de delay para o debounce

// --- DECLARAÇÃO DE FUNÇÕES ---
void enviarBit(uint8_t valorBit);
void enviarStreamBits(uint32_t pacoteDados);
void montarPacoteDados();
void enviarSync(bool flag);

void setup() {
  // Reduz a frequência da CPU para 80MHz para economizar energia
  setCpuFrequencyMhz(80);
  
  pinMode(TX_PIN_1, OUTPUT);
  pinMode(TX_PIN_0, OUTPUT);
  
  for (int i = 0; i < 4; i++) {
    pinMode(pinosBotoes[i], INPUT_PULLUP);
  }
  
  // Garante que ambos comecem desligados
  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);
}

void loop() {
  montarPacoteDados();

  // Verifica se o período de silêncio já passou
  if (millis() - tempoUltimoEnvio >= TEMPO_SILENCIO_MS) {
    // Fim do silêncio: envia imediatamente o pacote mais recente e atualiza o cronômetro
    enviarSync(0);
    enviarStreamBits(pacoteDadosPronto);
    enviarSync(1);
    tempoUltimoEnvio = millis(); 
  }
}

void enviarSync(bool flag) {
  if (flag == 1)
    delayMicroseconds(TEMPO_MORTO_SYNC_US);

  digitalWrite(TX_PIN_1, HIGH);
  digitalWrite(TX_PIN_0, LOW);

  if (flag == 0)
    delayMicroseconds(TEMPO_SYNCS_US);
  else if (flag == 1)
    delayMicroseconds(TEMPO_SYNCE_US);
  
  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);

  if (flag == 0)
    delayMicroseconds(TEMPO_MORTO_SYNC_US);
}

void montarPacoteDados() {
  // 1. Lógica de Debounce para os 4 botões
  for (int i = 0; i < 4; i++) {
    int leitura = digitalRead(pinosBotoes[i]);

    // Se o estado do pino mudou (ruído ou clique real), reseta o timer
    if (leitura != ultimoEstadoLido[i]) {
      ultimoTempoDebounce[i] = millis();
    }

    // Se a leitura se manteve estável por mais tempo que o tempo de debounce
    if ((millis() - ultimoTempoDebounce[i]) > tempoDebounce) {
      // E se esse estado estável for diferente do estado atual validado
      if (leitura != estadoAtualBotoes[i]) {
        estadoAtualBotoes[i] = leitura; // Valida a nova leitura
      }
    }
    ultimoEstadoLido[i] = leitura; // Atualiza a leitura para a próxima iteração
  }

  // 2. Mapeia os estados validados (Invertendo a lógica por causa do PULLUP)
  uint32_t btn1Estado = !estadoAtualBotoes[0];
  uint32_t btn2Estado = !estadoAtualBotoes[1];
  uint32_t btn3Estado = !estadoAtualBotoes[2];
  uint32_t btn4Estado = !estadoAtualBotoes[3];

  // 3. Montagem do Pacote Base
  uint32_t pacoteDados = 0b0000'000; // Pacote "zerado"

  bitWrite(pacoteDados, 3, btn1Estado);
  bitWrite(pacoteDados, 4, btn2Estado); 
  bitWrite(pacoteDados, 5, btn3Estado); 
  bitWrite(pacoteDados, 6, btn4Estado);

  // 4. Cálculo do Checksum
  uint8_t parte1 = (pacoteDados >> 3) & 0b11;
  uint8_t parte2 = (pacoteDados >> 5) & 0b11;

  uint8_t soma = parte1 + parte2;
  uint8_t checksum = (~soma + 1) & 0b111; // Inverte os bits (~), soma 1 e restringe a 3 bits
  
  pacoteDados = pacoteDados | checksum;

  // Atualiza a variável global que será usada no momento do envio
  pacoteDadosPronto = pacoteDados;
}

void enviarBit(uint8_t valorBit) { 
  if (valorBit == 1) { 
    digitalWrite(TX_PIN_1, HIGH);
    digitalWrite(TX_PIN_0, LOW);
  } else {                
    digitalWrite(TX_PIN_1, LOW);
    digitalWrite(TX_PIN_0, HIGH);
  }
  
  delayMicroseconds(TEMPO_BIT_US);

  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);
}

void enviarStreamBits(uint32_t pacoteDados) {
  for (int i = TAM_PACOTE_DADOS-1; i >= 0; i--) {
    uint8_t bitAtual = bitRead(pacoteDados, i);
    enviarBit(bitAtual);
    if (i != 0) delayMicroseconds(TEMPO_MORTO_BIT_US);
  }  
}