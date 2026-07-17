// ------- ARDUINO MEGA CODE --------

// Pinos de interrupção do Arduino Mega
#define RX_PIN_1 3 // Módulo de 433MHz (Recebe o bit 1) - Interrupt 1
#define RX_PIN_0 2 // Módulo de 315MHz (Recebe o bit 0) - Interrupt 0

// Definição dos pinos dos LEDs correspondentes aos botões
#define LED1_PIN 4
#define LED2_PIN 5
#define LED3_PIN 6
#define LED4_PIN 7

#define TEMPO_BIT_US 1000 // 1000 us  
#define MARGEM_ERRO 200 // 200 us     
#define TAM_PACOTE_DADOS 18 // Tamanho do pacote de dados em bits

#define TIMEOUT_SYNC_US 6000 // 6000 us de sincronia (6 ms). Acima disso considera-se o fim do pacote anterior

#define TIMEOUT_RESET_MS 1000 // Reseta pinos dos LED se nenhum pacote válido chegar dentro do timeout

volatile uint32_t bufferRF = 0; 
volatile uint8_t ibuf = 0;
volatile unsigned long ultimoTempoBit = 0;
volatile bool pacotePronto = false;
unsigned long ultimoPacotePronto = 0;

void setup() {
  Serial.begin(115200); 
  
  // Configuração dos pinos de RF
  pinMode(RX_PIN_1, INPUT);
  pinMode(RX_PIN_0, INPUT);
  
  // Configuração dos pinos dos LEDs como saída
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(RX_PIN_1), isrBit1_433MHz, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RX_PIN_0), isrBit0_315MHz, CHANGE);
  
  // Serial.println("Receptor Mega Iniciado. Aguardando comandos para os LEDs...");
}

void loop() {
  if (pacotePronto) {
    // Pausa interrupções rapidamente para copiar o dado de 32 bits em segurança
    noInterrupts();
    uint32_t pacoteParaProcessar = bufferRF;
    bufferRF = 0; 
    ibuf = 0;
    pacotePronto = false; 
    interrupts();

    // Envia direto para validação e extração
    bool ret = processarDadosRecebidos(pacoteParaProcessar);
    if (ret) ultimoPacotePronto = millis();
    else {
      Serial.println(pacoteParaProcessar, BIN);
    }
  }

  unsigned long tempoSemPacoteValido = millis() - ultimoPacotePronto;
  if (tempoSemPacoteValido > TIMEOUT_RESET_MS)
    resetPinos();
}

// ROTINA DE INTERRUPÇÃO PARA O MÓDULO 433MHz (BIT 1)
void isrBit1_433MHz() {
  static unsigned long tempoSubida = 0;
  
  if (digitalRead(RX_PIN_1) == HIGH) {
    tempoSubida = micros();
  } else {
    unsigned long tempoAtual = micros();
    unsigned long duracaoPulso = tempoAtual - tempoSubida;

    if (duracaoPulso > (TEMPO_BIT_US - MARGEM_ERRO) && duracaoPulso < (TEMPO_BIT_US + MARGEM_ERRO)) {
      
      if (tempoAtual - ultimoTempoBit > TIMEOUT_SYNC_US) {
        ibuf = 0; 
        bufferRF = 0; 
      }
      ultimoTempoBit = tempoAtual;

      if (ibuf < TAM_PACOTE_DADOS && !pacotePronto) {
        bufferRF = (bufferRF << 1) | 1; 
        ibuf++;
        
        if (ibuf >= TAM_PACOTE_DADOS) {
          pacotePronto = true;
        }
      }
    }
  }
}

// ROTINA DE INTERRUPÇÃO PARA O MÓDULO 315MHz (BIT 0)
void isrBit0_315MHz() {
  static unsigned long tempoSubida = 0;
  
  if (digitalRead(RX_PIN_0) == HIGH) {
    tempoSubida = micros();
  } else {
    unsigned long tempoAtual = micros();
    unsigned long duracaoPulso = tempoAtual - tempoSubida;

    if (duracaoPulso > (TEMPO_BIT_US - MARGEM_ERRO) && duracaoPulso < (TEMPO_BIT_US + MARGEM_ERRO)) {
      
      if (tempoAtual - ultimoTempoBit > TIMEOUT_SYNC_US) {
        ibuf = 0; 
        bufferRF = 0;
      }
      ultimoTempoBit = tempoAtual;

      if (ibuf < TAM_PACOTE_DADOS && !pacotePronto) {
        bufferRF = (bufferRF << 1); 
        ibuf++;
        
        if (ibuf >= TAM_PACOTE_DADOS) {
          pacotePronto = true;
        }
      }
    }
  }
}

// Função que valida a integridade do pacote baseado no Checksum Customizado
bool validarPacote(uint32_t pacoteRecebido) {
  uint8_t parte1 = (pacoteRecebido >> 14) & 0b1111;
  uint8_t parte2 = (pacoteRecebido >> 10) & 0b1111;
  uint8_t parte3 = (pacoteRecebido >> 6)  & 0b1111;

  uint8_t checksumRecebido = pacoteRecebido & 0b111111;
  uint8_t somaTotal = parte1 + parte2 + parte3 + checksumRecebido;
  uint8_t validacao = somaTotal & 0b111111;

  return (validacao == 0); 
}

// Extrai as informações e aciona os LEDs se o pacote for validado com sucesso
// Extrai as informações dos botões e aciona a Ponte H para controlar o carrinho
bool processarDadosRecebidos(uint32_t pacoteRecebido) {
  if (validarPacote(pacoteRecebido)) {

    // Extrai o estado (0 ou 1) de cada botão
    uint8_t btn1 = bitRead(pacoteRecebido, 7);  // Frente
    uint8_t btn2 = bitRead(pacoteRecebido, 10); // Trás
    uint8_t btn3 = bitRead(pacoteRecebido, 13); // Esquerda
    uint8_t btn4 = bitRead(pacoteRecebido, 16); // Direita

    // Variáveis que vão para as entradas da Ponte H (in1, in2, in3, in4)
    uint8_t in1 = 0, in2 = 0, in3 = 0, in4 = 0;

    // Evita comandos opostos/inválidos simultâneos (ex: frente+trás juntos)
    bool frenteTras = btn1 && btn2;
    bool esqDir = btn3 && btn4;

    if (!frenteTras && !esqDir) {
      if (btn1) {
        // Frente: s1->s2 (+12) e s3->s4 (+12)
        in1 = 1; in2 = 0;
        in3 = 1; in4 = 0;
      } else if (btn2) {
        // Trás: s1<-s2 (-12) e s3<-s4 (-12)
        in1 = 0; in2 = 1;
        in3 = 0; in4 = 1;
      } else if (btn3) {
        // Esquerda: motor A trás, motor B frente (gira no eixo)
        in1 = 0; in2 = 1;
        in3 = 1; in4 = 0;
      } else if (btn4) {
        // Direita: motor A frente, motor B trás (gira no eixo)
        in1 = 1; in2 = 0;
        in3 = 0; in4 = 1;
      }
      // Se nenhum botão pressionado: in1..in4 permanecem 0 -> motores parados
    }
    // Se houver comando conflitante, in1..in4 permanecem 0 (para por segurança)

    digitalWrite(LED1_PIN, in1); // IN1 da Ponte H
    digitalWrite(LED2_PIN, in2); // IN2 da Ponte H
    digitalWrite(LED3_PIN, in3); // IN3 da Ponte H
    digitalWrite(LED4_PIN, in4); // IN4 da Ponte H

    return true; // Processou dados corretamente
  }

  return false; // Não processou dados com sucesso
}

void resetPinos() {
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
}