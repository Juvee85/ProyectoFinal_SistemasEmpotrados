import socket
import json
import mysql.connector
from datetime import datetime
import csv
import os

# ---------------- CONFIGURACIÓN ----------------

HOST = "0.0.0.0"  # Escucha en todas las interfaces de red
PORT = 5000       # Puerto por el que ESP32 enviará

DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "juvito",
    "database": "Terrario"
}

CSV_FILE = "respaldo_tcp.csv"

# ---------------- FUNCIONES ----------------

def guardar_en_mysql(data):
    fecha = data.get("fecha", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    sensores = ["temperatura", "humedad", "iluminacion"]

    conn = mysql.connector.connect(**DB_CONFIG)
    cursor = conn.cursor()

    for sensor in sensores:
        valores = data[sensor]
        sql = """
            INSERT INTO lecturas (fecha, sensor, minima, maxima, actual)
            VALUES (%s, %s, %s, %s, %s)
        """
        valores_sql = (
            fecha,
            sensor,
            valores["minima"],
            valores["maxima"],
            valores["actual"]
        )
        cursor.execute(sql, valores_sql)

    conn.commit()
    cursor.close()
    conn.close()
    print("Datos guardados en MySQL")


def guardar_en_csv(data):
    sensores = ["temperatura", "humedad", "iluminacion"]
    fecha = data.get("fecha", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    archivo_nuevo = not os.path.exists(CSV_FILE)
    with open(CSV_FILE, mode='a', newline='') as file:
        writer = csv.writer(file)
        if archivo_nuevo:
            writer.writerow(["fecha", "sensor", "minima", "maxima", "actual"])

        for sensor in sensores:
            valores = data[sensor]
            writer.writerow([fecha, sensor, valores["minima"], valores["maxima"], valores["actual"]])

    print("Datos respaldados en CSV")


def iniciar_servidor():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Servidor TCP escuchando en {HOST}:{PORT}...")

        while True:
            conn, addr = s.accept()
            with conn:
                print(f"\nConexión desde {addr}")
                try:
                    buffer = conn.recv(2048).decode("utf-8")
                    print("Recibido:", buffer)
                    json_data = json.loads(buffer)
                    guardar_en_mysql(json_data)
                    guardar_en_csv(json_data)
                except Exception as e:
                    print("Error al procesar datos:", e)

# ---------------- EJECUCION ----------------

if __name__ == "__main__":
    iniciar_servidor()
