/*Trabalho de conclusão de curso - Engenharia Química PUC-Rio - Isabela Bittencourt

Titulo do trabalho: Implementacao de monitoramento e controle em unidades experimentais
por meio da comunicação remota através do Arduino e Node-RED

Orientadora: Prof Dra Amanda Lemette

Codigo desenvolvido para monitoramento e controle de uma unidade experimental composta por:
medidor de vazão, sensor de temperatura, bombas, válvula solenoide, sensores de nível

Observacao: o codigo considera que a comunicacao serial deve ser feita considerando:
"F...*" (controle automatico)
"T...*" (controle manual)
*/

#include <Thermistor.h>
#include <Ultrasonic.h>

//define os pinos para trigger e echo, para os tanques inferior e superior
#define pino_triggerInf 9 //7
#define pino_echoInf 8 //3
#define pino_triggerSup 5
#define pino_echoSup 4 //6
#define TQMAX 20

//inicializa os sensores de nivel
Ultrasonic ultrasonicInf(pino_triggerInf,pino_echoInf);
Ultrasonic ultrasonicSup(pino_triggerSup,pino_echoSup);

//pinos digitais
int rele_bombaSup = 3;
int rele_bombaInf = 7;
int rele_valvula = 6;
int pino_medidor = 2;

//pino de temperatura - analogico A0
Thermistor temp(0);

//variaveis nivel
long microsecSup, microsecInf;
float dist_TSup, dist_TInf;
int SPauto = 12; //SP para controle automatico
int novoSP = SPauto; //inicializo o controle manual com o SP automatico
int nivelControleMin = 18; //distancia do sensor para garantir que a bomba nao vai cavitar
int nivelControleMax = 4; //menor distancia do sensor para nao transbordar os tanques/danificar sensores

//variaveis globais
String stringNode, mensagem, status_tanque;
String status_modo = "Auto. SP: " + String(novoSP); //inicializa com modo automatico
float vazao; //armazena vazao em L/min
int contaPulso; //quantidade de pulsos
bool valvula, bombaInf, bombaSup = false;
float temperatura;

//funcoes
void incrementa_pulso(){ 
  contaPulso++; //Incrementa a variavel de contagem dos pulsos
}

void mede_vazao(){
  contaPulso = 0;   //Zera a variável para contar os giros por segundo
  sei();      //Habilita interrupção, mesmo que a interrupt()
  delay (1000); //Aguarda 1 segundo
  cli();      //Desabilita interrupção, mesmo que a noInterrupt()
  
  vazao = contaPulso / 7.5; //Converte para L/min
}

//principal
void setup(){
  Serial.begin(9600);
  pinMode(rele_bombaInf, OUTPUT);
  pinMode(rele_bombaSup, OUTPUT);
  pinMode(rele_valvula, OUTPUT);
  pinMode(pino_medidor, INPUT);

  //inicializa com tudo desligado/fechado
  digitalWrite(rele_bombaInf, HIGH);
  digitalWrite(rele_bombaSup, HIGH);
  digitalWrite(rele_valvula, HIGH);

  attachInterrupt(digitalPinToInterrupt(pino_medidor),incrementa_pulso,RISING);//contagem de pulsos do medidor de vazao
}

void loop (){
 //avaliar se houve mudanca de controle
  if(Serial.available()){//vai verificar se chegou alguma mensagem na Serial -> ele conta pelo numero de caracteres
    mensagem = Serial.readStringUntil('*');
    if(mensagem[0] == 'F'){
      novoSP = SPauto;
      status_modo = "Auto. SP: " + String(novoSP);
    }
    else if(mensagem[0] == 'T'){
      mensagem.remove(0,1);
      novoSP = mensagem.toFloat();
      status_modo = "Manual. SP: ";
      status_modo += (novoSP);
    }
    mensagem = "";
  }

  microsecSup = ultrasonicSup.timing();
  microsecInf = ultrasonicInf.timing();
  dist_TSup = ultrasonicSup.convert(microsecSup, Ultrasonic::CM);
  dist_TInf = ultrasonicInf.convert(microsecInf, Ultrasonic::CM);

//Codigo de controle de tanques escrito considerando o tanque superior como um tanque reservatorio
  if(dist_TInf > (TQMAX - novoSP) && dist_TSup <= nivelControleMin){ //Tanque inferior abaixo do SP: ligar a bomba superior e abrir a valvula, desliga a inferior
    digitalWrite(rele_valvula,LOW);
    digitalWrite(rele_bombaSup,LOW);
    digitalWrite(rele_bombaInf,HIGH);
    valvula = true;
    bombaSup = true;
    bombaInf = false;
    status_tanque = "Abaixo do SP";
  }
  
  else if(dist_TInf < (TQMAX - novoSP)){//Tanque inferior acima do SP estabelecido, envio o excesso pro tanque superior
    digitalWrite(rele_bombaSup,HIGH);
    digitalWrite(rele_valvula,HIGH); 
    digitalWrite(rele_bombaInf,LOW);
    valvula = false;
    bombaSup = false;
    bombaInf = true;
    status_tanque = "Acima do SP";
  }

  else if(dist_TInf == (TQMAX - novoSP)){//Tanques inferior no target estabelecido
    digitalWrite(rele_bombaSup,HIGH);
    digitalWrite(rele_valvula,HIGH); 
    digitalWrite(rele_bombaInf,HIGH);
    valvula = false;
    bombaSup = false;
    bombaInf = false;
    status_tanque = "No target";
  }

  else if(dist_TSup > nivelControleMin || dist_TSup <= nivelControleMax || dist_TInf <= nivelControleMax){//Tanque reservatorio (superior) abaixo do limite minimo OU tanques com risco de transbordar
    digitalWrite(rele_bombaSup,HIGH);
    digitalWrite(rele_valvula,HIGH); 
    digitalWrite(rele_bombaInf,HIGH);
    valvula = false;
    bombaSup = false;
    bombaInf = false;
    status_tanque = "Verificar localmente";
  }
  
  mede_vazao();
  temperatura = temp.getTemp();
  
  stringNode = "I" + String(temperatura) + "," + String(vazao) + "," + String(valvula) + "," + String(bombaSup) + "," + String(bombaInf) + "," + String(dist_TSup) + "," + String(dist_TInf) + "," + status_tanque + "," + status_modo + "F";
  Serial.println(stringNode);
}
