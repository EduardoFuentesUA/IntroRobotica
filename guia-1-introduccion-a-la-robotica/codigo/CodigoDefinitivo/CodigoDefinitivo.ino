// Seguidor de línea con control PID y control manual por Bluetooth (Capítulo 7).
// El HC-05 va al puerto serial del NodeMCU (RX y TX): desconéctalo antes de
// subir el programa. La app manda dos tipos de mensaje, cada uno en una línea:
//   KP:400,KI:0,KD:0   ganancias del PID: inicia el modo automático
//   M:150,-150         velocidad de cada motor (-255 a 255): modo manual

// --- PINES DE MOTORES (módulo L298N, ver Tabla 4.2, Capítulo 4) ---
int in1 = D7;
int in2 = D6;
int in3 = D5;
int in4 = D4;
int ENA = D8;   // motor izquierdo
int ENB = D3;   // motor derecho

// --- PINES DE SENSORES (ver Tabla 4.3, Capítulo 4) ---
// Con los módulos del libro, cada sensor entrega LOW sobre la superficie
// clara y HIGH sobre la cinta negra (o si no recibe luz, por ejemplo al
// levantar el robot).
int irLeft = D1;
int irRight = D0;

// --- VARIABLES DEL PID ---
float Kp = 400.0;
float Ki = 0.0;
float Kd = 0.0;
int velocidad_base = 200;

// Variables matemáticas internas
float P = 0, I = 0, D = 0, PID_value = 0;
float error = 0, previous_error = 0;

// --- VARIABLES BLUETOOTH Y SISTEMA ---
bool modoAuto = true; // true = Seguidor de Línea, false = Control Manual

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(10);

  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Parche de seguridad
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  pinMode(irLeft, INPUT);
  pinMode(irRight, INPUT);

  Serial.println("Sistema encendido. Esperando datos...");
  delay(3000);
}

void loop() {
  // 1. LEER BLUETOOTH
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    if (data.length() > 0) {
      procesarDatos(data);
    }
  }

  // 2. EJECUTAR NAVEGACIÓN CONTINUA
  if (modoAuto) {
    calcularError();
    calcularPID();
    aplicarMotores();
  }
}

// =============================================================
// BLOQUE 1: CÁLCULO DEL ERROR (con identificación de pérdida)
// =============================================================
void calcularError() {
  int leftVal   = digitalRead(irLeft);
  int rightVal  = digitalRead(irRight);

  if (leftVal == LOW && rightVal == HIGH) {
    error = -1;   // la cinta quedó bajo el sensor derecho
  }
  else if (leftVal == HIGH && rightVal == LOW) {
    error = 1;    // la cinta quedó bajo el sensor izquierdo
  }
  else if (leftVal == LOW && rightVal == LOW) {
    error = 0;    // ambos sobre la superficie clara: la cinta pasa entre ellos
  }
  else if (leftVal == HIGH && rightVal == HIGH) {
    // Ambos sensores sin luz de vuelta: se mira la memoria
    if (previous_error == 0) {
      error = 100; // Freno de seguridad si se levanta en recta
    }
    else if (previous_error < 0) {
      error = -10; // Perdió la cinta con el último error negativo
    }
    else if (previous_error > 0) {
      error = 10;  // Perdió la cinta con el último error positivo
    }
  }
}

// =============================================================
// BLOQUE 2: MATEMÁTICA PID
// =============================================================
void calcularPID() {
  // Ignorar matemática si está perdido haciendo el trompo o frenado
  if (abs(error) == 10 || error == 100) return;

  P = error;
  I = I + error;
  D = error - previous_error;

  PID_value = (Kp * P) + (Ki * I) + (Kd * D);
}

// =============================================================
// BLOQUE 3: POTENCIA Y DIRECCIÓN (con giro de tanque)
// =============================================================
void aplicarMotores() {

  // --- RECUPERACIÓN: BÚSQUEDA SOBRE SU PROPIO EJE ---
  if (abs(error) == 10) {
    int searchSpeed = 150; // Velocidad de giro. Ajustar si gira muy brusco.

    if (error == -10) {
      // Gira a la izquierda (rueda izquierda atrás, rueda derecha adelante)
      digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
      digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
      analogWrite(ENA, searchSpeed);
      analogWrite(ENB, searchSpeed);
    }
    else if (error == 10) {
      // Gira a la derecha (rueda izquierda adelante, rueda derecha atrás)
      digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
      digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
      analogWrite(ENA, searchSpeed);
      analogWrite(ENB, searchSpeed);
    }
    return; // Salir de la función hasta que vuelva a ver la línea
  }

  // --- FRENO TOTAL (si error es 100) ---
  if (error == 100) {
    digitalWrite(in1, LOW); digitalWrite(in2, LOW);
    digitalWrite(in3, LOW); digitalWrite(in4, LOW);
    analogWrite(ENA, 0); analogWrite(ENB, 0);
    return;
  }

  // --- AVANCE NORMAL CON PID ---
  int speedLeft = velocidad_base - PID_value;
  int speedRight = velocidad_base + PID_value;

  speedLeft = constrain(speedLeft, 0, 1023);
  speedRight = constrain(speedRight, 0, 1023);

  // Dirección hacia adelante
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);

  analogWrite(ENA, speedLeft);
  analogWrite(ENB, speedRight);

  // Solo guardamos memoria si estamos pisando la pista
  if (error == -1 || error == 1 || error == 0) {
    previous_error = error;
  }
}

// =============================================================
// BLOQUE 4: COMUNICACIÓN CON LA APP
// =============================================================
void procesarDatos(String d) {
  if (d.startsWith("KP:")) {
    modoAuto = true;

    int iKP = d.indexOf("KP:");
    int iKI = d.indexOf(",KI:");
    int iKD = d.indexOf(",KD:");

    if (iKP != -1 && iKI != -1 && iKD != -1) {
      Kp = d.substring(iKP + 3, iKI).toFloat();
      Ki = d.substring(iKI + 4, iKD).toFloat();
      Kd = d.substring(iKD + 4).toFloat();

      Serial.print("NUEVO PID -> P:"); Serial.print(Kp);
      Serial.print(" I:"); Serial.print(Ki);
      Serial.print(" D:"); Serial.println(Kd);

      I = 0;
    }
  }
  else if (d.startsWith("M:")) {
    modoAuto = false;

    int coma = d.indexOf(",");
    if (coma != -1) {
      int motorIzq = d.substring(2, coma).toInt();
      int motorDer = d.substring(coma + 1).toInt();

      controlManualApp(motorIzq, motorDer);
    }
  }
}

// =============================================================
// BLOQUE 5: CONTROL MANUAL
// =============================================================
void controlManualApp(int mIzq, int mDer) {
  if (mIzq >= 0) {
    digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
    analogWrite(ENA, mIzq);
  } else {
    digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
    analogWrite(ENA, abs(mIzq));
  }

  if (mDer >= 0) {
    digitalWrite(in3, HIGH); digitalWrite(in4, LOW); // Hacia adelante
    analogWrite(ENB, mDer);
  } else {
    digitalWrite(in3, LOW); digitalWrite(in4, HIGH); // Reversa
    analogWrite(ENB, abs(mDer));
  }
}
