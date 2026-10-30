// Firmware del brazo articulado de 4 GDL más pinza.
//
// Reparto de trabajo (Capítulo 6): el anfitrión en Python resuelve la
// cinemática inversa y envía ángulos; este firmware solo traduce ángulos a
// pulsos, respeta los límites mecánicos e interpola el movimiento.
//
// Protocolo, una línea de texto terminada en '\n':
//   PING                  -> OK PONG
//   MOVE a0 a1 a2 a3      -> OK          (ángulos del modelo, en grados)
//   GRIP 0 | GRIP 1       -> OK          (0 = abierta, 1 = cerrada)
//   SPEED n               -> OK          (ms entre pasos de interpolación)
//   HOME                  -> OK
//   GET                   -> OK POS a0 a1 a2 a3
//   cualquier otra cosa   -> ERR ...
#include <Servo.h>

const int N_JUNTAS = 4;
const int LARGO_MAX = 48;

// Calibración de cada articulación. Se obtiene midiendo, no calculando:
// el cero del modelo cinemático casi nunca coincide con el centro del servo.
struct Articulacion {
  byte pin;
  int pulsoCero;      // us que corresponden a 0 grados del modelo
  float usPorGrado;   // el signo corrige el sentido de giro
  int pulsoMin;       // tope mecánico seguro
  int pulsoMax;
  float anguloHome;
};

Articulacion junta[N_JUNTAS] = {
  {  9, 1500,  11.11, 700, 2300,   0.0 },  // 0 base
  { 10, 1480, -11.11, 700, 2300,  60.0 },  // 1 hombro
  { 11, 2100,  11.11, 500, 2300, -80.0 },  // 2 codo
  {  6, 1500, -11.11, 700, 2300, -40.0 },  // 3 muñeca
};

const int PIN_PINZA = 5;
const int PINZA_ABIERTA = 1700;
const int PINZA_CERRADA = 1150;

Servo servo[N_JUNTAS];
Servo servoPinza;

int pulsoActual[N_JUNTAS];
float anguloActual[N_JUNTAS];

int msPorPaso = 8;              // velocidad: más alto, más lento
const int PASO_US = 5;          // resolución de la interpolación

char linea[LARGO_MAX];
int cuantos = 0;

// --- Conversión y límites -------------------------------------------------

int anguloAPulso(int i, float grados) {
  long pulso = junta[i].pulsoCero + (long)(grados * junta[i].usPorGrado);
  return constrain(pulso, junta[i].pulsoMin, junta[i].pulsoMax);
}

// Mueve todas las articulaciones a la vez, en pasos pequeños. Sin esto los
// servos salen disparados a máxima velocidad y el brazo pierde la carga.
void moverA(float destino[N_JUNTAS]) {
  int objetivo[N_JUNTAS];
  int mayorSalto = 0;

  for (int i = 0; i < N_JUNTAS; i++) {
    objetivo[i] = anguloAPulso(i, destino[i]);
    int salto = abs(objetivo[i] - pulsoActual[i]);
    if (salto > mayorSalto) mayorSalto = salto;
  }

  int pasos = mayorSalto / PASO_US;
  if (pasos < 1) pasos = 1;

  int inicio[N_JUNTAS];
  for (int i = 0; i < N_JUNTAS; i++) inicio[i] = pulsoActual[i];

  for (int p = 1; p <= pasos; p++) {
    for (int i = 0; i < N_JUNTAS; i++) {
      long avance = (long)(objetivo[i] - inicio[i]) * p / pasos;
      pulsoActual[i] = inicio[i] + (int)avance;
      servo[i].writeMicroseconds(pulsoActual[i]);
    }
    delay(msPorPaso);
  }

  for (int i = 0; i < N_JUNTAS; i++) {
    pulsoActual[i] = objetivo[i];
    // Se guarda el ángulo que realmente quedó, no el pedido: si el límite
    // mecánico recortó el movimiento, el anfitrión debe enterarse.
    anguloActual[i] = (objetivo[i] - junta[i].pulsoCero) / junta[i].usPorGrado;
  }
}

// --- Interpretación de comandos -------------------------------------------

void responderPosicion() {
  Serial.print(F("OK POS"));
  for (int i = 0; i < N_JUNTAS; i++) {
    Serial.print(' ');
    Serial.print(anguloActual[i], 2);
  }
  Serial.println();
}

void ejecutar(char *texto) {
  char *comando = strtok(texto, " ");
  if (comando == NULL) return;

  if (strcmp(comando, "PING") == 0) {
    Serial.println(F("OK PONG"));

  } else if (strcmp(comando, "MOVE") == 0) {
    float destino[N_JUNTAS];
    for (int i = 0; i < N_JUNTAS; i++) {
      char *dato = strtok(NULL, " ");
      if (dato == NULL) {
        Serial.println(F("ERR faltan angulos"));
        return;
      }
      destino[i] = atof(dato);
    }
    moverA(destino);
    Serial.println(F("OK"));

  } else if (strcmp(comando, "GRIP") == 0) {
    char *dato = strtok(NULL, " ");
    if (dato == NULL) {
      Serial.println(F("ERR falta 0 o 1"));
      return;
    }
    servoPinza.writeMicroseconds(atoi(dato) ? PINZA_CERRADA : PINZA_ABIERTA);
    delay(400);
    Serial.println(F("OK"));

  } else if (strcmp(comando, "SPEED") == 0) {
    char *dato = strtok(NULL, " ");
    if (dato == NULL) {
      Serial.println(F("ERR falta valor"));
      return;
    }
    msPorPaso = constrain(atoi(dato), 1, 60);
    Serial.println(F("OK"));

  } else if (strcmp(comando, "HOME") == 0) {
    float destino[N_JUNTAS];
    for (int i = 0; i < N_JUNTAS; i++) destino[i] = junta[i].anguloHome;
    moverA(destino);
    Serial.println(F("OK"));

  } else if (strcmp(comando, "GET") == 0) {
    responderPosicion();

  } else {
    Serial.println(F("ERR comando desconocido"));
  }
}

// --- Programa principal ---------------------------------------------------

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < N_JUNTAS; i++) {
    servo[i].attach(junta[i].pin, junta[i].pulsoMin, junta[i].pulsoMax);
    pulsoActual[i] = anguloAPulso(i, junta[i].anguloHome);
    anguloActual[i] = junta[i].anguloHome;
    servo[i].writeMicroseconds(pulsoActual[i]);
  }
  servoPinza.attach(PIN_PINZA);
  servoPinza.writeMicroseconds(PINZA_ABIERTA);

  delay(600);                 // deja que los servos lleguen al reposo
  Serial.println(F("LISTO"));
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (cuantos == 0) continue;
      linea[cuantos] = '\0';
      ejecutar(linea);
      cuantos = 0;
    } else if (cuantos < LARGO_MAX - 1) {
      linea[cuantos++] = c;
    } else {
      cuantos = 0;
      Serial.println(F("ERR linea muy larga"));
    }
  }
}
