// --- PINOS ESP32 ---
#define RX_PIN_1 21 // Módulo 433MHz (Recebe o Bit 1 e Sincronismos)
#define RX_PIN_0 22 // Módulo 315MHz (Recebe o Bit 0)

#define ENA_PIN 32
#define IN1_PIN 33
#define IN2_PIN 25
#define IN3_PIN 26
#define IN4_PIN 27
#define ENB_PIN 14

// Tolerâncias de tempo (em microssegundos)
#define TEMPO_SYNC_START_MIN 200
#define TEMPO_SYNC_START_MAX 450
#define TEMPO_SYNC_END_MIN   1500
#define TEMPO_SYNC_END_MAX   2500
#define TEMPO_BIT_MIN        700
#define TEMPO_BIT_MAX        1300

#define TAM_PACOTE_DADOS 7
#define MAX_BUFFER 20

#define TIMEOUT_ATUALIZACAO 1000 // Timeout de atualização em ms

class DCMotor {  
  int spd = 255;
  int pinEN, pin1, pin2, pwmChannel;
  
  // Propriedades do PWM baseadas no código de exemplo
  const int freq = 500;       // Frequência do PWM
  const int resolution = 8;   // Resolução de 8 bits (0-255)

  public:  
  
    // Pinout agora recebe o pino Enable (EN), os pinos de direção (IN1/IN2) e o canal PWM do ESP32
    void Pinout(int en, int in1, int in2, int channel){ 
      pinEN = en;
      pin1 = in1;
      pin2 = in2;
      pwmChannel = channel;

      // Define os pinos de direção como saída
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
      
      // Configura o PWM para o controle de velocidade usando a nova API do ESP32
      ledcAttachChannel(pinEN, freq, resolution, pwmChannel);
    }   

    void Speed(int speedValue){ // Método responsável por salvar a velocidade de atuação do motor
      spd = speedValue;
    }     

    void Forward(){ // Faz o motor girar para frente
      ledcWrite(pinEN, spd);    // Aplica a velocidade no pino de Enable via PWM
      digitalWrite(pin1, HIGH); // Configura direção para frente[cite: 1]
      digitalWrite(pin2, LOW);  // Configura direção para frente[cite: 1]
    }   

    void Backward(){ // Faz o motor girar para trás
      ledcWrite(pinEN, spd);    // Aplica a velocidade no pino de Enable via PWM[cite: 1]
      digitalWrite(pin1, LOW);  // Configura direção reversa[cite: 1]
      digitalWrite(pin2, HIGH); // Configura direção reversa[cite: 1]
    }

    void Stop(){ // Faz o motor ficar parado
      ledcWrite(pinEN, 0);      // Zera o ciclo de trabalho do PWM (desliga a força do motor)[cite: 1]
      digitalWrite(pin1, LOW);  // Desliga pino de direção[cite: 1]
      digitalWrite(pin2, LOW);  // Desliga pino de direção[cite: 1]
    }
};

DCMotor Motor1, Motor2;

// Estrutura para armazenar o evento de cada pulso
struct PulsoRF {
  uint8_t bitValor;
  unsigned long tempoSubida;
  unsigned long tempoDescida;
};

// Variáveis voláteis compartilhadas entre ISR e Loop
volatile PulsoRF bufferRF[MAX_BUFFER];
volatile uint8_t indexBuffer = 0;
volatile bool recebendo = false;
volatile bool pacotePronto = false;

volatile unsigned long bordaSubida1 = 0;
volatile unsigned long bordaSubida0 = 0;
volatile unsigned long tempoFimSyncStart = 0; // Marca o Início absoluto da janela de dados

unsigned long ultimaAtualizacao = 0; // Marca o tempo em que os pinos de saída foram atualizados pela última vez

void IRAM_ATTR isrTrataRX1();
void IRAM_ATTR isrTrataRX0();
void pararCarrinho();

void setup() {
  //Serial.begin(115200);
  // Reduz a frequência da CPU para 80MHz para economizar energia
  setCpuFrequencyMhz(80);
  
  pinMode(RX_PIN_1, INPUT);
  pinMode(RX_PIN_0, INPUT);
  
  // Seleção dos pinos que cada motor usará, como descrito na classe.
  Motor1.Pinout(ENA_PIN, IN1_PIN, IN2_PIN, 0);
  Motor2.Pinout(ENB_PIN, IN3_PIN, IN4_PIN, 1);
  pararCarrinho();

  // Inicia escutando APENAS o 433MHz em busca do Sincronismo
  attachInterrupt(digitalPinToInterrupt(RX_PIN_1), isrTrataRX1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RX_PIN_0), isrTrataRX0, CHANGE);
}

void loop() {
  if (pacotePronto) {
    // 1. Copia os dados de forma segura
    PulsoRF bufferLocal[MAX_BUFFER];
    uint8_t tamanhoLocal = 0;
    unsigned long tempoRefLocal = 0;
    
    noInterrupts();
    for (int i = 0; i < indexBuffer; i++) {
      bufferLocal[i].bitValor = bufferRF[i].bitValor;
      bufferLocal[i].tempoSubida = bufferRF[i].tempoSubida;
      bufferLocal[i].tempoDescida = bufferRF[i].tempoDescida;
    }
    tamanhoLocal = indexBuffer;
    tempoRefLocal = tempoFimSyncStart;
    pacotePronto = false;
    indexBuffer = 0;
    interrupts();

    // 2. Reconstrução Espacial (Tempo) do Pacote
    uint32_t pacoteRecebido = 0;
    uint8_t bitsPreenchidos = 0; // Usado como máscara para garantir que pegamos os 7 bits

    for (int i = 0; i < tamanhoLocal; i++) {
      unsigned long duracao = bufferLocal[i].tempoDescida - bufferLocal[i].tempoSubida;
      
      // Valida a largura do pulso
      if (duracao >= TEMPO_BIT_MIN && duracao <= TEMPO_BIT_MAX) {
        
        // Ponto 3: Calcula a distância do bit em relação ao fim do pulso de Sincronismo
        unsigned long deltaT = bufferLocal[i].tempoSubida - tempoRefLocal;
        
        // Matemática para descobrir qual é o bit:
        // O 1º bit chega em ~500us (Ordem 0). O 2º em ~2000us (Ordem 1). O 3º em ~3500us (Ordem 2).
        // Usamos uma compensação de +250 para arredondamento seguro na divisão inteira
        int ordem = (deltaT + 250) / 1500; 
        
        if (ordem >= 0 && ordem < TAM_PACOTE_DADOS) {
          int bitPos = (TAM_PACOTE_DADOS - 1) - ordem; // Transforma Ordem (0 a 6) em Posição (6 a 0)
          
          if (bufferLocal[i].bitValor == 1) {
            bitSet(pacoteRecebido, bitPos);
          } else {
            bitClear(pacoteRecebido, bitPos); 
          }
          
          // Marca que esta posição foi preenchida com sucesso
          bitSet(bitsPreenchidos, bitPos);
        }
      }
    }

    // 3. Valida se a máscara indica que todos os 7 bits foram capturados (0b01111111)
    if (bitsPreenchidos == 0x7F) {
      uint8_t parte1 = (pacoteRecebido >> 3) & 0b11;
      uint8_t parte2 = (pacoteRecebido >> 5) & 0b11;
      
      uint8_t soma = parte1 + parte2;
      uint8_t checksumEsperado = (~soma + 1) & 0b111;
      uint8_t checksumRecebido = pacoteRecebido & 0b111;

      if (checksumEsperado == checksumRecebido) {
        uint8_t btn1 = bitRead(pacoteRecebido, 3);
        uint8_t btn2 = bitRead(pacoteRecebido, 4);
        uint8_t btn3 = bitRead(pacoteRecebido, 5);
        uint8_t btn4 = bitRead(pacoteRecebido, 6);
        moverCarrinho(btn1, btn2, btn3, btn4);

        ultimaAtualizacao = millis();
      }
    }
  }
  // Serial.print("Recebendo = ");
  // Serial.print(recebendo);
  // Serial.print("\t");
  // Serial.print("IndexBuffer = ");
  // Serial.print(indexBuffer);
  // Serial.print("\t");
  // Serial.print("Pacote pronto = ");
  // Serial.println(pacotePronto);

  if ((millis() - ultimaAtualizacao) >= TIMEOUT_ATUALIZACAO)
    pararCarrinho();
}

// ISR para o módulo de 433MHz (RX_1 e Sincronização)
void IRAM_ATTR isrTrataRX1() {
  unsigned long agora = micros();

  // Ponto 2: Timeout de segurança (15ms). Se estourar, aborta o pacote.
  if (recebendo && (agora - tempoFimSyncStart > 15000)) {
    recebendo = false;
    //detachInterrupt(digitalPinToInterrupt(RX_PIN_0));
  }
  
  if (digitalRead(RX_PIN_1) == HIGH) {
    bordaSubida1 = agora;
  } else {
    unsigned long duracao = agora - bordaSubida1;
    
    // Ponto 1: Rate limiter (Ignora qualquer ruído de queda que durou menos de 80us)
    if (duracao < 80) return; 

    // Verifica Sincronização de Início
    if (duracao >= TEMPO_SYNC_START_MIN && duracao <= TEMPO_SYNC_START_MAX) {
      recebendo = true;
      pacotePronto = false;
      indexBuffer = 0;
      tempoFimSyncStart = agora; 
      
      // Habilita imediatamente a interrupção do Módulo 315MHz
      //attachInterrupt(digitalPinToInterrupt(RX_PIN_0), isrTrataRX0, CHANGE);
    } 
    // Verifica Sincronização de Fim
    else if (duracao >= TEMPO_SYNC_END_MIN && duracao <= TEMPO_SYNC_END_MAX) {
      if (recebendo) {
        recebendo = false;
        pacotePronto = true; 
        
        // Desabilita a interrupção do Módulo 315MHz (Reduz uso de CPU)
        //detachInterrupt(digitalPinToInterrupt(RX_PIN_0));
      }
    } 
    // Captura Bit 1
    else if (recebendo && indexBuffer < MAX_BUFFER) {
      bufferRF[indexBuffer].bitValor = 1;
      bufferRF[indexBuffer].tempoSubida = bordaSubida1;
      bufferRF[indexBuffer].tempoDescida = agora;
      indexBuffer++;
    }
  }
}

// ISR para o módulo de 315MHz (RX_0)
void IRAM_ATTR isrTrataRX0() {
  // Se não recebemos um Sync Start válido ainda, ignoramos as mudanças neste pino
  if (!recebendo) return;

  unsigned long agora = micros();
  
  if (digitalRead(RX_PIN_0) == HIGH) {
    bordaSubida0 = agora;
  } else {
    unsigned long duracao = agora - bordaSubida0;
    // Ponto 1: Rate limiter (Ignora qualquer ruído de queda que durou menos de 80us)
    if (duracao < 80) return; 
    // Como esta interrupção só é acionada durante um pacote válido, capturamos direto
    if (recebendo && indexBuffer < MAX_BUFFER) {
      bufferRF[indexBuffer].bitValor = 0;
      bufferRF[indexBuffer].tempoSubida = bordaSubida0;
      bufferRF[indexBuffer].tempoDescida = agora;
      indexBuffer++;
    }
  }
}

void moverCarrinho(uint8_t btn1, uint8_t btn2,
                   uint8_t btn3, uint8_t btn4) {

  // Evita comandos opostos/inválidos simultâneos (ex: frente+trás juntos)
  bool frenteTras = btn1 && btn2;
  bool esqDir = btn3 && btn4;

  Motor1.Speed(198); // A velocidade do motor pode variar de 0 a 255, onde 255 é a velocidade máxima.
  Motor2.Speed(198);

  if (!frenteTras && !esqDir) {
    if (btn1) {
      // Frente: s1->s2 (+12) e s3->s4 (+12)
      Motor1.Forward(); 
      Motor2.Forward();
    } else if (btn2) {
      // Trás: s1<-s2 (-12) e s3<-s4 (-12)
      Motor1.Backward();
      Motor2.Backward();
    } else if (btn3) {
      // Esquerda: motor A trás, motor B frente (gira no eixo)
      Motor1.Backward();
      Motor2.Forward();
    } else if (btn4) {
      // Direita: motor A frente, motor B trás (gira no eixo)
      Motor1.Forward();
      Motor2.Backward();
    } else {
      pararCarrinho();
    }
  } else {
    // Se houver comando conflitante, para por segurança
    pararCarrinho();
  }

}

void pararCarrinho() {
  Motor1.Stop();
  Motor2.Stop();
}