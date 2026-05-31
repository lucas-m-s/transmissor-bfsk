// --------- ESP32 CODE ----------
// Pinos conectados aos pinos de DADOS dos módulos TX
#define TX_PIN_1 14 // Módulo de 433MHz (Transmite o bit 1)
#define TX_PIN_0 4 // Módulo de 315MHz (Transmite o bit 0)

// Configuração de tempo (1000 microssegundos = 1 milissegundo por bit)
#define TEMPO_BIT_US 1000 

void setup() {
  pinMode(TX_PIN_1, OUTPUT);
  pinMode(TX_PIN_0, OUTPUT);
  
  // Garante que ambos comecem desligados
  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);

  Serial.begin(115200);
  Serial.println("Transmissor BFSK Pronto!");
  Serial.println("Digite um numero entre 0 e 255 e pressione Enter:");
}

// Função para enviar um único bit usando a lógica BFSK
void enviarBit(bool valorBit) {
  if (valorBit == true) { // Enviar Bit 1
    digitalWrite(TX_PIN_1, HIGH);
    digitalWrite(TX_PIN_0, LOW);
  } else {                // Enviar Bit 0
    digitalWrite(TX_PIN_1, LOW);
    digitalWrite(TX_PIN_0, HIGH);
  }
  
  // Mantém o sinal no ar pelo tempo definido
  delayMicroseconds(TEMPO_BIT_US);
  
  // Desliga ambos para criar uma "pausa" (Guard Time)
  // Isso impede que o receptor trave (problema de acoplamento AC em módulos OOK)
  digitalWrite(TX_PIN_1, LOW);
  digitalWrite(TX_PIN_0, LOW);
  delayMicroseconds(TEMPO_BIT_US); 
}

void loop() {
  // Envia uma sequência de teste: 1 0 1 1
  // enviarBit(1);
  // enviarBit(0);
  // enviarBit(1);
  // enviarBit(1);
  
  // Pausa longa antes de repetir o pacote para facilitar a leitura no Serial Monitor do RX
  // delay(2000); 

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
    }
    
    Serial.println(" -> [Enviado!]");
  }

}