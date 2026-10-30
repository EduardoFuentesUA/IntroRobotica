"""Anfitrion del brazo articulado: habla con el firmware por puerto serial.

Regla del protocolo: se envia una linea y se espera la respuesta antes de
enviar la siguiente. El buffer del Arduino son 64 bytes; si el anfitrion
dispara comandos sin esperar, se pierden caracteres a mitad de camino.

Requiere pyserial:
    pip install pyserial

Ejecutalo con:
    python control_brazo.py
"""

import time

import serial
import serial.tools.list_ports


class ErrorBrazo(Exception):
    """El firmware respondio ERR, o no respondio a tiempo."""


def buscar_puerto():
    """Devuelve el primer puerto que parezca una placa Arduino."""
    for puerto in serial.tools.list_ports.comports():
        descripcion = (puerto.description or "").lower()
        if any(marca in descripcion
               for marca in ("arduino", "ch340", "ch910", "cp210", "ftdi")):
            return puerto.device
    return None


class Brazo:
    """Conexion con el firmware del brazo."""

    def __init__(self, puerto=None, baudios=115200, tiempo_limite=5.0):
        puerto = puerto or buscar_puerto()
        if puerto is None:
            raise ErrorBrazo("no se encontro ningun puerto; conectalo o "
                             "indicalo a mano, por ejemplo COM4")

        self.tiempo_limite = tiempo_limite
        self.serie = serial.Serial(puerto, baudios, timeout=1.0)

        # Abrir el puerto reinicia la placa: hay que esperar a que arranque.
        # El firmware avisa con "LISTO" cuando termina de posicionar los servos.
        self._esperar("LISTO")

    def _esperar(self, texto):
        limite = time.time() + self.tiempo_limite
        while time.time() < limite:
            linea = self.serie.readline().decode("utf-8", "ignore").strip()
            if linea == texto:
                return
        raise ErrorBrazo("el firmware nunca respondio %r" % texto)

    def comando(self, texto):
        """Envia una linea y devuelve la respuesta del firmware."""
        self.serie.reset_input_buffer()
        self.serie.write((texto + "\n").encode("utf-8"))

        limite = time.time() + self.tiempo_limite
        while time.time() < limite:
            respuesta = self.serie.readline().decode("utf-8", "ignore").strip()
            if respuesta.startswith("ERR"):
                raise ErrorBrazo("%s -> %s" % (texto, respuesta))
            if respuesta.startswith("OK"):
                return respuesta
        raise ErrorBrazo("sin respuesta a %r" % texto)

    # --- Ordenes de alto nivel ---
    def ping(self):
        return self.comando("PING") == "OK PONG"

    def mover(self, angulos):
        """angulos: los cuatro angulos del modelo, en grados."""
        return self.comando("MOVE " + " ".join("%.2f" % a for a in angulos))

    def pinza(self, cerrada):
        return self.comando("GRIP 1" if cerrada else "GRIP 0")

    def velocidad(self, ms_por_paso):
        return self.comando("SPEED %d" % ms_por_paso)

    def home(self):
        return self.comando("HOME")

    def posicion(self):
        """Devuelve los cuatro angulos que el firmware tiene realmente."""
        respuesta = self.comando("GET")           # "OK POS a0 a1 a2 a3"
        return [float(v) for v in respuesta.split()[2:]]

    def cerrar(self):
        self.serie.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.cerrar()


if __name__ == "__main__":
    with Brazo() as brazo:
        print("Conectado. PING ->", brazo.ping())

        brazo.velocidad(8)
        brazo.home()
        print("Reposo:", brazo.posicion())

        for postura in ([0, 60, -80, -40], [30, 45, -60, -30], [-30, 70, -95, -20]):
            brazo.mover(postura)
            print("Pedido:", postura, " Real:", brazo.posicion())
            time.sleep(0.5)

        brazo.pinza(True)
        time.sleep(0.5)
        brazo.pinza(False)
        brazo.home()
        print("Listo.")
