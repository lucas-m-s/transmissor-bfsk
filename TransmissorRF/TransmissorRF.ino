// --------- ESP32 CODE ----------

#define TX_PIN_1 33 // Módulo de 433MHz (Transmite o bit 1)
#define TX_PIN_0 32 // Módulo de 315MHz (Transmite o bit 0)
#define BTN1_PIN 25
#define BTN2_PIN 26
#define BTN3_PIN 27
#define BTN4_PIN 14

#define TAM_PACOTE_DADOS 18 // Tamanho do pacote de dados em bits

#define TEMPO_BIT_US 1000 // 1000 us por bit (1 ms)
#define TEMPO_MORTO_US 500   // 500 us de tempo morto (0,5 ms)
#define TEMPO_SILENCIO_MS 100 // 100 ms de silêncio

void enviarBit(uint8_t valorBit); // Função para enviar um único bit usando a lógica BFSK
void enviarStreamBits(uint32_t pacoteDados);
uint32_t montarPacoteDados(); // Monta o pacote de dados a ser enviado
void enviarSerial(); // Enviar um número de 8 bits digitado no monitor serial

void setup() {
  pinMode(TX_PIN_1, OUTPUT);
  pinMode(TX_PIN_0, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  
  // Garante que ambos comecem desligados
  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);

  Serial.begin(115200);
  Serial.println("Transmissor BFSK Pronto!");
  Serial.println("Digite um numero entre 0 e 255 e pressione Enter:");
}

void loop() {
  uint32_t pacoteDados = montarPacoteDados();
  enviarStreamBits(pacoteDados);
  // Momento de silêncio
  delay(TEMPO_SILENCIO_MS);
}




void enviarBit(uint8_t valorBit) { 
  if (valorBit == 1) { // Enviar Bit 1
    digitalWrite(TX_PIN_1, HIGH);
    digitalWrite(TX_PIN_0, LOW);
  } else {                // Enviar Bit 0
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
      
    // Mostra no monitor do próprio transmissor o que ele está jogando no ar
    Serial.print(bitAtual);

    // Envia fisicamente pelos rádios
    enviarBit(bitAtual);
    if (i != 0) delayMicroseconds(TEMPO_MORTO_US);
  }  
  Serial.println(" -> [Enviado!]");
}

uint32_t montarPacoteDados() {
  uint32_t btn1Estado = !digitalRead(BTN1_PIN);
  uint32_t btn2Estado = !digitalRead(BTN2_PIN);
  uint32_t btn3Estado = !digitalRead(BTN3_PIN);
  uint32_t btn4Estado = !digitalRead(BTN4_PIN);

  uint32_t pacoteDados = 0b100100100100000000; // Pacote "zerado"

  bitWrite(pacoteDados, 7, btn1Estado);  // 0b1001001001x0000000
  bitWrite(pacoteDados, 10, btn2Estado); // 0b1001001x01x0000000
  bitWrite(pacoteDados, 13, btn3Estado); // 0b1001x01x01x0000000
  bitWrite(pacoteDados, 16, btn4Estado); // 0b1x01x01x01x0000000

  uint8_t parte1 = (pacoteDados >> 14) & 0b1111; // 0b|1x01|x01x01x0000000
  uint8_t parte2 = (pacoteDados >> 10) & 0b1111; // 0b1x01|x01x|01x0000000
  uint8_t parte3 = (pacoteDados >> 6)  & 0b1111; // 0b1x01x01x|01x0|000000

  uint8_t soma = parte1 + parte2 + parte3;
  
  // Inverte os bits (~), soma 1 e restringe a 6 bits
  uint8_t checksum = (~soma + 1) & 0b111111;

  pacoteDados = pacoteDados | checksum;

  return pacoteDados;
}

// Para fins de debug, quando eu precisar
void enviarSerial() {
  // Verifica se você digitou algo no Monitor Serial do Transmissor
  if (Serial.available() > 0) {
    
    // Lê o número digitado como um número inteiro
    int numeroDigitado = Serial.parseInt();
    
    // Limpa qualquer caractere restante no buffer (como o '\n' do Enter)
    while (Serial.available() > 0) {
      Serial.read();
    }
    
    // Filtro para garantir que cabe em 8 bits (0 a 255)
    if (numeroDigitado < 0 || numeroDigitado > 255) {
      Serial.println("Por favor, digite um numero valido entre 0 e 255.");
      return; 
    }

    Serial.print("Enviando o numero ");
    Serial.print(numeroDigitado);
    Serial.print(" em binario: ");

    // Loop para ler os 8 bits do número (do Bit Mais Significativo - MSB ao Menos Significativo - LSB)
    // i = 7 é o primeiro bit da esquerda, i = 0 é o último bit da direita
    for (int i = 7; i >= 0; i--) {
      // bitRead() é uma função nativa do Arduino que extrai o valor (0 ou 1) do bit na posição 'i'
      bool bitAtual = bitRead(numeroDigitado, i);
      
      // Mostra no monitor do próprio transmissor o que ele está jogando no ar
      Serial.print(bitAtual);

      // Envia fisicamente pelos rádios
      enviarBit(bitAtual);
      if (i != 0) delayMicroseconds(TEMPO_MORTO_US);
    }
    
    Serial.println(" -> [Enviado!]");
  }
}