// ------- ARDUINO MEGA CODE --------
// Pinos de interrupção do Arduino Mega
#define RX_PIN_1 3 // Módulo de 433MHz (Recebe o bit 1) - Interrupt 0
#define RX_PIN_0 2 // Módulo de 315MHz (Recebe o bit 0) - Interrupt 1

// Tempo do bit esperado
#define TEMPO_BIT_US 1000 
// Tolerância para aceitar o pulso (ex: entre 800us e 1200us)
#define MARGEM_ERRO 200 

// Variáveis 'volatile' são obrigatórias quando alteradas dentro de uma ISR
volatile bool novoDadoDisponivel = false;
volatile char bitRecebido = ' ';

void setup() {
  Serial.begin(9600);
  
  pinMode(RX_PIN_1, INPUT);
  pinMode(RX_PIN_0, INPUT);
  
  // Atrela as funções aos pinos, no modo CHANGE (qualquer mudança de estado)
  attachInterrupt(digitalPinToInterrupt(RX_PIN_1), isrBit1_433MHz, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RX_PIN_0), isrBit0_315MHz, CHANGE);
  
  Serial.println("Receptor Mega com Interrupcoes Iniciado...");
}

void loop() {
  // O loop principal não fica preso lendo pinos!
  // Ele apenas verifica a "bandeira" (flag) levantada pela ISR
  
  if (novoDadoDisponivel) {
    // Imprime o bit validado
    Serial.print(bitRecebido);
    
    // Abaixa a bandeira para aguardar o próximo bit
    novoDadoDisponivel = false; 
  }
  
  // Aqui você pode colocar outros códigos pesados (ex: atualizar display, ler sensores)
  // que o rádio continuará sendo recebido em segundo plano!
}

// --- ROTINA DE INTERRUPÇÃO PARA O MÓDULO 433MHz (BIT 1) ---
void isrBit1_433MHz() {
  static unsigned long tempoSubida = 0;
  
  if (digitalRead(RX_PIN_1) == HIGH) {
    // O sinal acabou de subir. Anotamos o momento exato.
    tempoSubida = micros();
  } else {
    // O sinal desceu. Vamos calcular quanto tempo ele ficou em ALTO.
    unsigned long duracaoPulso = micros() - tempoSubida;
    
    // Filtro anti-ruído: O pulso tem o tamanho do nosso bit?
    if (duracaoPulso > (TEMPO_BIT_US - MARGEM_ERRO) && duracaoPulso < (TEMPO_BIT_US + MARGEM_ERRO)) {
      bitRecebido = '1';
      novoDadoDisponivel = true;
    }
  }
}

// --- ROTINA DE INTERRUPÇÃO PARA O MÓDULO 315MHz (BIT 0) ---
void isrBit0_315MHz() {
  static unsigned long tempoSubida = 0;
  
  if (digitalRead(RX_PIN_0) == HIGH) {
    // O sinal acabou de subir. Anotamos o momento exato.
    tempoSubida = micros();
  } else {
    // O sinal desceu. Vamos calcular a duração.
    unsigned long duracaoPulso = micros() - tempoSubida;
    
    // Filtro anti-ruído: O pulso tem o tamanho do nosso bit?
    if (duracaoPulso > (TEMPO_BIT_US - MARGEM_ERRO) && duracaoPulso < (TEMPO_BIT_US + MARGEM_ERRO)) {
      bitRecebido = '0';
      novoDadoDisponivel = true;
    }
  }
}