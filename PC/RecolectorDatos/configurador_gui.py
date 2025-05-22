import tkinter as tk
from tkinter import ttk, messagebox
import requests
from PIL import Image, ImageTk

# ------------------- CONFIGURACIÓN --------------------
ENDPOINTS = {
    "Temperatura": "/establecer/temperatura",
    "Humedad": "/establecer/humedad",
    "Iluminación": "/establecer/iluminacion"
}

# ------------------- ESTILOS Y COLORES --------------------
PRIMARY_COLOR = "#2A4D3A"
SECONDARY_COLOR = "#5B8C5A"
ACCENT_COLOR = "#C4D6B0"
BG_COLOR = "#1A2F1D"
FONT_TITLE = ("Verdana", 14, "bold")
FONT_TEXT = ("Verdana", 10)

# ------------------- FUNCIÓN PRINCIPAL --------------------
def enviar_config():
    ip = entry_ip.get().strip()
    sensor = combo_sensor.get()
    try:
        minima = float(entry_min.get())
        maxima = float(entry_max.get())
    except ValueError:
        messagebox.showerror("Error", "Ingresa valores numéricos válidos.")
        return

    if not ip:
        messagebox.showerror("Error", "Ingresa la IP del ESP32.")
        return

    url = f"http://{ip}{ENDPOINTS[sensor]}"
    json_data = {
        "minima": minima,
        "maxima": maxima
    }

    try:
        response = requests.post(url, json=json_data)
        if response.status_code == 200:
            messagebox.showinfo("Éxito", f"Configuración enviada para {sensor}.")
        else:
            messagebox.showerror("Error", f"Error HTTP {response.status_code}: {response.text}")
    except Exception as e:
        messagebox.showerror("Error", f"No se pudo conectar al ESP32:\n{e}")

def mostrar_estadisticas():
    ip = entry_ip.get().strip()
    if not ip:
        messagebox.showerror("Error", "Ingresa la IP del ESP32.")
        return

    url = f"http://{ip}/datos"
    try:
        response = requests.get(url)
        if response.status_code != 200:
            messagebox.showerror("Error", f"Error HTTP {response.status_code}")
            return

        datos = response.json()

        mensaje = ""
        for sensor, valores in datos.items():
            if sensor == "fecha":
                mensaje += f"\n📅 Fecha: {valores}\n"
                continue
            mensaje += f"\n{sensor.upper()}:\n"
            mensaje += f"  Mínimo: {valores['minima']}\n"
            mensaje += f"  Máximo: {valores['maxima']}\n"
            mensaje += f"  Actual: {valores['actual']}\n"

        messagebox.showinfo("Estadísticas Actuales", mensaje)

    except Exception as e:
        messagebox.showerror("Error", f"No se pudo conectar al ESP32:\n{e}")


# ------------------- INTERFAZ GRÁFICA --------------------
ventana = tk.Tk()
ventana.title("Administrador de Terrario")
ventana.geometry("500x650")
ventana.configure(bg=BG_COLOR)
ventana.resizable(False, False)

# Imagen
try:
    img_plants = ImageTk.PhotoImage(Image.open("imagen/huella.png").resize((100, 100)))  # Reemplaza por tu imagen real
except:
    img_plants = None


header = tk.Frame(ventana, bg=PRIMARY_COLOR, height=100)
header.pack(fill="x")

if img_plants:
    plants_label = tk.Label(header, image=img_plants, bg=PRIMARY_COLOR)
    plants_label.pack(side="left", padx=20)

title_label = tk.Label(header, text="Configurador", 
                      font=FONT_TITLE, 
                      bg=PRIMARY_COLOR, 
                      fg=ACCENT_COLOR)
title_label.pack(side="left", pady=20)

# Contenedor principal
main_frame = tk.Canvas(ventana, bg=BG_COLOR, highlightthickness=0)
main_frame.pack(pady=20, padx=20, fill="both", expand=True)

# IP
config_frame = tk.Frame(main_frame, bg=SECONDARY_COLOR, bd=2, relief="groove")
config_frame.pack(pady=10, padx=10, fill="x")

tk.Label(config_frame, 
        text="Dirección IP del Dispositivo:",
        font=FONT_TEXT,
        bg=SECONDARY_COLOR,
        fg="white").pack(pady=(10,5), anchor="w")

entry_ip = ttk.Entry(config_frame, width=25, font=FONT_TEXT)
entry_ip.pack(pady=5, padx=10, fill="x")

# Parametro
param_frame = tk.Frame(main_frame, bg=BG_COLOR)
param_frame.pack(pady=15, fill="x")

tk.Label(param_frame, 
        text="Seleccionar Parámetro:",
        font=FONT_TEXT,
        bg=BG_COLOR,
        fg=ACCENT_COLOR).pack(anchor="w")

combo_sensor = ttk.Combobox(param_frame, 
                           values=list(ENDPOINTS.keys()), 
                           font=FONT_TEXT,
                           state="readonly")
combo_sensor.set("Temperatura")
combo_sensor.pack(pady=5, fill="x")

# Valores
umbral_frame = tk.Frame(main_frame, bg=SECONDARY_COLOR, bd=2, relief="groove")
umbral_frame.pack(pady=15, fill="x")

tk.Label(umbral_frame, 
        text="Configurar Rango:",
        font=FONT_TEXT,
        bg=SECONDARY_COLOR,
        fg="white").pack(pady=10, anchor="w")

tk.Label(umbral_frame, 
        text="Mínimo:", 
        font=FONT_TEXT,
        bg=SECONDARY_COLOR,
        fg="white").pack(anchor="w", padx=10)
entry_min = ttk.Entry(umbral_frame, font=FONT_TEXT)
entry_min.pack(pady=5, padx=10, fill="x")

tk.Label(umbral_frame, 
        text="Máximo:", 
        font=FONT_TEXT,
        bg=SECONDARY_COLOR,
        fg="white").pack(anchor="w", padx=10)
entry_max = ttk.Entry(umbral_frame, font=FONT_TEXT)
entry_max.pack(pady=5, padx=10, fill="x")

# Botones
btn_style = ttk.Style()
btn_style.configure("TButton", 
                   font=FONT_TEXT,
                   background=PRIMARY_COLOR,
                   foreground="black",
                   borderwidth=2,
                   relief="raised")

btn_enviar = ttk.Button(main_frame, 
                       text="APLICAR CONFIGURACIÓN", 
                       command=enviar_config,
                       style="TButton")
btn_enviar.pack(pady=20, fill="x")

btn_estadisticas = ttk.Button(main_frame, 
                       text="VER ESTADÍSTICAS",
                       command=mostrar_estadisticas,
                       style="TButton")
btn_estadisticas.pack(pady=10, fill="x")


footer = tk.Frame(ventana, bg=PRIMARY_COLOR, height=30)
footer.pack(side="bottom", fill="x")

ventana.mainloop()
