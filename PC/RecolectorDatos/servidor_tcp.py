import socket
import json
import mysql.connector
from datetime import datetime

DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "",
    "database": "Terrario"
}

HOST = ""  # IP
PORT = 5000

def guardar_datos(data):
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
    print("Datos guardados")

def iniciar_servidor():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Servidor TCP escuchando en {HOST}:{PORT}...")

        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Conexión desde {addr}")
                data = conn.recv(2048).decode('utf-8')
                try:
                    json_data = json.loads(data)
                    guardar_datos(json_data)
                except Exception as e:
                    print("Error al procesar datos:", e)

if __name__ == "__main__":
    iniciar_servidor()
