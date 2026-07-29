// --------- ARDUINO MEGA CODE (TIMER-BASED) ----------

#define RX_PIN_1 2 // 433MHz (Sincronismo e Bit 1)
#define RX_PIN_0 3 // 315MHz (Bit 0)

#define LED_BTN1 4
#define LED_BTN2 5
#define LED_BTN3 6
#define LED_BTN4 7

#define TAM_PACOTE_DADOS 7

// Constantes do Timer 1 (Prescaler 8 -> 0.5us por tick)
#define TICKS_1000_US 1999 // (1000us / 0.5us) - 1
#define TICKS_1500_US 2999 // (1500us / 0.5us) - 1

volatile unsigned long tempoSync = 0;
volatile uint8_t bitAtual = 0;
volatile uint32_t pacoteRecebido = 0;
volatile bool pacotePronto = false;

void setup() {
  pinMode(RX_PIN_1, INPUT);
  pinMode(RX_PIN_0, INPUT);
  
  pinMode(LED_BTN1, OUTPUT);
  pinMode(LED_BTN2, OUTPUT);
  pinMode(LED_BTN3, OUTPUT);
  pinMode(LED_BTN4, OUTPUT);

  // Zera configurações do Timer 1
  noInterrupts();
  TCCR1A = 0; 
  TCCR1B = 0;
  TIMSK1 = 0; // Interrupções do timer desligadas inicialmente
  interrupts();

  // Inicia escutando o pulso de Sincronismo de Início
  attachInterrupt(digitalPinToInterrupt(RX_PIN_1), isrTrataSync, CHANGE);
}

void loop() {
  if (pacotePronto) {
    // Validação do Checksum
    uint8_t parte1 = (pacoteRecebido >> 3) & 0b11;
    uint8_t parte2 = (pacoteRecebido >> 5) & 0b11;
    
    uint8_t soma = parte1 + parte2;
    uint8_t checksumEsperado = (~soma + 1) & 0b111;
    uint8_t checksumRecebido = pacoteRecebido & 0b111;

    if (checksumEsperado == checksumRecebido) {
      digitalWrite(LED_BTN1, bitRead(pacoteRecebido, 3));
      digitalWrite(LED_BTN2, bitRead(pacoteRecebido, 4));
      digitalWrite(LED_BTN3, bitRead(pacoteRecebido, 5));
      digitalWrite(LED_BTN4, bitRead(pacoteRecebido, 6));
    }

    // Prepara o sistema para o próximo pacote
    pacotePronto = false;
    
    // Limpa pendências do pino e reativa a interrupção externa
    EIFR = bit(INTF4); // Limpa flag da INT0 (Pino 2)
    attachInterrupt(digitalPinToInterrupt(RX_PIN_1), isrTrataSync, CHANGE);
  }
}

// 1. Interrupção Externa: Detecta apenas o Sincronismo de Início
void isrTrataSync() {
  if (digitalRead(RX_PIN_1) == HIGH) {
    tempoSync = micros(); // Marca a borda de subida
  } else {
    unsigned long duracao = micros() - tempoSync; // Borda de descida
    
    // Confirma se é o pulso de Sync de Início (~333us)
    if (duracao > 200 && duracao < 450) {
      
      // Desativa imediatamente a interrupção externa (Ignora todo o ruído seguinte)
      detachInterrupt(digitalPinToInterrupt(RX_PIN_1));
      
      // Prepara variáveis para a montagem do pacote
      bitAtual = 0;
      pacoteRecebido = 0;

      // Configura e Dispara o Timer 1
      TCNT1 = 0;                 // Zera o contador do timer
      OCR1A = TICKS_1000_US;     // Define o alvo inicial (1000us)
      
      TCCR1B = (1 << WGM12) | (1 << CS11); // Liga Modo CTC (WGM12) e Prescaler 8 (CS11)
      TIMSK1 = (1 << OCIE1A);              // Habilita a interrupção por comparação A
    }
  }
}

// 2. Interrupção de Hardware: Ocorre no centro exato de cada bit
ISR(TIMER1_COMPA_vect) {
  // Ajusta o intervalo para os próximos bits logo na primeira execução
  if (bitAtual == 0) {
    OCR1A = TICKS_1500_US; 
  }

  // Lê fisicamente o estado dos módulos
  uint8_t leitura1 = digitalRead(RX_PIN_1);
  uint8_t leitura0 = digitalRead(RX_PIN_0);
  
  int bitPos = (TAM_PACOTE_DADOS - 1) - bitAtual; // Posição 6 descendo até 0

  // Se houver sinal no 433MHz, registra 1. Se houver no 315MHz, registra 0.
  // Se ambos estiverem em LOW (ruído/falha), o bitClear garante que permaneça 0.
  if (leitura1 == HIGH) {
    bitSet(pacoteRecebido, bitPos);
  } else {
    bitClear(pacoteRecebido, bitPos);
  }

  bitAtual++;

  // Verifica se todos os 7 bits foram lidos
  if (bitAtual >= TAM_PACOTE_DADOS) {
    // Desliga o Timer 1
    TCCR1B = 0; 
    TIMSK1 = 0; 
    
    // Sinaliza o loop principal para validação
    pacotePronto = true; 
  }
}