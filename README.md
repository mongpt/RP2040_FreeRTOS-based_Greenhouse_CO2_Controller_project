
# 🌿 Greenhouse CO₂ Controller

A prototype CO₂ fertilization controller for greenhouse environments, developed as a school project on the
              Raspberry Pi Pico W using a FreeRTOS-based embedded software stack. The system integrates Vaisala’s
              industrial-grade sensors to monitor temperature, humidity, and CO₂ levels, coordinating fan and valve
              control through modular real-time tasks and robust inter-task communication mechanisms (queues,
              semaphores, event groups). Data is published to the cloud and visualized via ThingSpeak, allowing for
              remote monitoring and control. Designed for automation, reliability, and real-world sensor integration,
              this project demonstrates advanced RTOS design in a cloud-connected greenhouse control system.

> 🏫 Developed by Group 3 (Mong Phan, Sami Barbaglia, Xuan Dang)  
> 🎓 Metropolia University of Applied Sciences, School of ICT  
> 📅 October 2024

---

## 📌 Key Features

- Maintain CO₂ concentration at a configurable setpoint (up to 1500 ppm)
- Safety override: ventilation at 100% if CO₂ > 2000 ppm
- EEPROM-based storage for Wi-Fi and CO₂ setpoint
- Local user interface with OLED, rotary encoder, and buttons
- Cloud data logging and remote control via ThingSpeak API

---

## 🧭 How to Use the System

### 🖥️ Local User Interface (OLED + Encoder)

- **Welcome screen** → Shown briefly at startup
- **Selection menu** → Navigate using the rotary knob
  - **SHOW INFO**: Displays CO₂, temperature, humidity, fan speed, and setpoint
  - **SET CO₂ LEVEL**: Adjust setpoint using rotary (200–1500 ppm), confirm with OK
  - **CONFIG WIFI**: View/edit SSID and password

### 🔧 Button Functions

| Button         | Action                                    |
|----------------|-------------------------------------------|
| OK (SW_0)      | Short press = confirm / select            |
| OK (SW_0)      | Long press = enter/exit edit mode         |
| BACK (SW_2)    | Return to main menu                       |
| Rotary Knob    | Scroll/select items or characters         |

### ☁️ Cloud Interface (ThingSpeak)

- **Data upload**: Every 1 minute via REST API
  - Fields: CO₂, Temperature, Humidity, Fan Speed, Setpoint
- **Remote control**: Via ThingSpeak **TalkBack**
  - Add commands like new CO₂ setpoints (e.g., `900`)
  - System will poll and update EEPROM/setpoint if valid

---

## 🛠️ Hardware Components

| Component               | Description                                               |
|-------------------------|-----------------------------------------------------------|
| Raspberry Pi Pico W     | Main MCU (FreeRTOS-based)                                 |
| EEPROM (I²C)            | Persistent storage of Wi-Fi and setpoint                  |
| OLED (SSD1306)          | Display interface                                         |
| Rotary encoder + buttons| UI interaction                                            |
| GMP252 / HMP60          | CO₂, Temp, RH sensors via Modbus                          |
| SDP610                  | Pressure sensor via I²C                                   |
| Produal MIO 12-V        | Fan speed control via Modbus                              |
| GPIO27                  | CO₂ valve control                                         |

---

## 🧠 Software Architecture

- **RTOS Kernel**: FreeRTOS
- **Tasks**:
  - `gpio_task` – input handling
  - `display_task` – OLED UI
  - `modbus_task` – sensor reading and logic control
  - `eeprom_task` – config persistence
  - `tls_task` – ThingSpeak API interaction

---

## 🚦 CO₂ Control Logic

- If **CO₂ > 2000 ppm** → fan = 100%
- If **CO₂ > setpoint + 100 ppm** → fan = 50%
- If **CO₂ < setpoint − 100 ppm** → open valve
- Otherwise, fan = OFF

---

## 🌐 Cloud Integration

- Uses ThingSpeak REST API to:
  - Upload sensor data to cloud
  - Receive remote CO₂ setpoint via TalkBack queue

---

## 📋 Documents

- [📄 System Specification (PDF)](./Docs/Greenhouse%20CO2_controller_specification.pdf)
- [📄 Project Report (PDF)](./Docs/Greenhouse%20CO2%20controller%20Project%20report%20-%20G03.pdf)
- [📄 User Manual (PDF)](./Docs/Greenhouse%20CO2%20controller%20User%20manual%20-%20G03.pdf)

---

## 📌 Contributors

- **Mong Phan**
- **Sami Barbaglia**
- **Xuan Dang**

---

## 📜 License

This project was developed for academic purposes at **Metropolia University of Applied Sciences**. Users can freely use this project for your educational purpose ONLY.
