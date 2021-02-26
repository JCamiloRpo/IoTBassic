//Librerias necesarios
#include <SPI.h> //Ethernet
#include <Ethernet.h> //Ethernet
#include <OneWire.h> //Sensor temperatura
#include <DallasTemperature.h> //Sensor temperatura
#include <Servo.h> //MicroServo
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

//Definiciones
#define TRI 14
#define ECH 15
#define REL 16
#define LDR 17
#define SER 6
//Variables
int Temp=0, T, D, luz = 0;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC del Arduino
unsigned long beginMicros, endMicros, byteCount = 0;
bool printWebData = true;
//Objetos
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
Servo motor;
OneWire ourWire(9); //Se establece el pin 2  como bus OneWire
DallasTemperature sensors(&ourWire); //Se declara una variable u objeto para nuestro sensor
IPAddress server(192,168,1,55); // Numero IP del WebServer
IPAddress ip(192, 168, 1, 56); // IP estatica por si falla el DHCP
IPAddress myDns(192, 168, 0, 1);
EthernetClient client;

//Inicializacion
void setup() {
  pinMode(TRI, OUTPUT);
  pinMode(ECH, INPUT);
  pinMode(REL, OUTPUT);
  motor.attach(SER);
  motor.write(180);
  Serial.begin(9600);
  sensors.begin();//Se inicia el sensor
  lcd.begin(16,2);//Medidas LCD
  lcd.home();
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");
}

//Lo que se tiene que hacer todo el tiempo
void loop() {
  
  ElectroValvula(!NivelAgua());
  delay(100);
  
  Temperatura();
  MostrarLCD();
  delay(100);

  Luz();
  delay(100);

  if(Serial.available()){
    char val = Serial.read();
    Alimentar(val);
  }

  Escuchar();

  Desconectado();
}

bool NivelAgua(){//Sensar nivel de agua
  digitalWrite(TRI, LOW);  
  delayMicroseconds(4);
  digitalWrite(TRI, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRI, LOW);

  T = (pulseIn(ECH, HIGH) / 2); //Obtener el tiempo que se demoró
  int tmp = (int)(T / 29); //Calcular la distancia
  if(tmp != D){
    D = tmp;
    //Serial.print("Distancia = ");
    //Serial.print(D);
    //Serial.println(" cm");
    if(D > 5){
      //Serial.println("Nivel de agua bajo, activar electrovalvula");
      Enviar("Agua", "01", D, "Nivel_de_agua_bajo");
      return true;
    }
    //Serial.println("Nivel de agua normal");
    Enviar("Agua", "01", D, "Nivel_de_agua_normal");
      
  }
  return false;
}

void ElectroValvula(bool Abrir){//Decidir sobre electrovalvula
  if (Abrir){
    digitalWrite(REL, HIGH);
  }
  else{
    digitalWrite(REL, LOW);
  }
}

void Temperatura(){//Sensar temperatura
  sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
  int tmp= (int)sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC
  if(tmp != Temp){
    Temp = tmp;
    //Serial.print("Temperatura = ");
    //Serial.print(Temp);
    //Serial.println(" °C");
    Enviar("Temperatura", "01", Temp, "Temperatura_normal");
  }
}

void Luz(){//Sensor de luz a traves del agua
  int tmp = (int)analogRead(LDR);
  if(tmp != luz){
    luz = tmp;
    //Serial.print("Cantidad de luz = ");
    //Serial.println(luz);
    if(luz > 500){
      //Serial.println("Agua muy sucia. Requiere recambio");
      Enviar("Luz", "01", luz, "Agua_sucia");
      return;
    }
    //Serial.println("Agua normal.");
    Enviar("Luz", "01", luz, "Agua_normal");
  }
}

void Alimentar(char Opcion){//Alimentar por medio del teclado
  if(Opcion == '0'){
      motor.write(90);
      delay(500);
      motor.write(180);
    }
}

void MostrarLCD(){
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Temperatura");
  lcd.setCursor(6,1);
  lcd.print(Temp);
  lcd.print(" C");
}

void Enviar(String t, String i, int d, String des){
  if (client.connect(server, 80)) {
    Serial.print("connected to ");
    Serial.println(client.remoteIP());
    // Make a HTTP request:
    client.println("GET /arduino.php?type="+t+"&id="+i+"&data="+d+"&des="+des+" HTTP/1.0");
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  beginMicros = micros();
}

void Escuchar(){
  int len = client.available();
  if (len > 0) {
    byte buffer[80];
    if (len > 80) len = 80;
    client.read(buffer, len);
    if (printWebData) {
      Serial.write(buffer, len); // show in the serial monitor (slows some boards)
    }
    byteCount = byteCount + len;
  }
}

void Desconectado(){
  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    endMicros = micros();
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    Serial.print("Received ");
    Serial.print(byteCount);
    Serial.print(" bytes in ");
    float seconds = (float)(endMicros - beginMicros) / 1000000.0;
    Serial.print(seconds, 4);
    float rate = (float)byteCount / seconds / 1000.0;
    Serial.print(", rate = ");
    Serial.print(rate);
    Serial.print(" kbytes/second");
    Serial.println();
  }
}
