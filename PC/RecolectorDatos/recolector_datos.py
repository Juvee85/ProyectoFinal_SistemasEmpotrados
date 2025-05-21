import requests
import mysql.connector
from datetime import datetime

# -------------------- CONFIGURACIÓN --------------------

ESP32_IP = ""  
ENDPOINT = "/datos"

DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "",
    "database": "Terrario"
}

# -------------------- FUNCIÓN PRINCIPAL --------------------

def recolectar_y_guardar():
    try:
        url = f"{ESP32_IP}{ENDPOINT}"
        response = requests.get(url)
        data = response.json()

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
        print("Datos insertados correctamente.")
    
    except Exception as e:
        print("Error:", e)
    
    finally:
        if 'cursor' in locals(): cursor.close()
        if 'conn' in locals(): conn.close()

# -------------------- EJECUCIÓN --------------------

if __name__ == "__main__":
    recolectar_y_guardar()
