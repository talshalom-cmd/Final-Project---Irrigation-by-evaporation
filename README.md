# Smart Irrigation System (Penman-Monteith) 💧🌱

## Project Overview
This project implements an autonomous irrigation system designed to optimize water usage based on real-time climatic conditions. [cite_start]Unlike traditional time-based controllers, our system calculates the exact water requirements using the **Penman-Monteith evapotranspiration equation ($ET_0$)**[cite: 2].

[cite_start]The system collects environmental data via sensors, processes the calculations on a remote server (Python), and controls the irrigation valves automatically to maintain a perfect water balance for the plants[cite: 2].

## System Architecture ⚙️
The system operates in a closed loop between the greenhouse and the classroom server:

1.  [cite_start]**Data Collection:** An **ESP32** microcontroller in the greenhouse reads data from environment sensors (Wind, Rain, Temp, Humidity, Radiation)[cite: 14].
2.  [cite_start]**Cloud Upload:** The raw data is sent to **ThingSpeak** cloud platform via WiFi[cite: 14].
3.  [cite_start]**Data Processing:** A **Python script** running on a computer (Agrotech Classroom) fetches the raw data and calculates the daily Evapotranspiration ($ET_0$)[cite: 15].
4.  [cite_start]**Decision Making:** The calculated irrigation duration is sent back to ThingSpeak (via MQTT)[cite: 16].
5.  [cite_start]**Execution:** Every day at **19:00**, the ESP32 retrieves the command and activates the irrigation system accordingly[cite: 16].

![System Architecture](path/to/your_architecture_image.png)
*(Note: Upload the system diagram image to your repository and update this link)*

## The Algorithm: Penman-Monteith
[cite_start]We use the standard FAO-56 Penman-Monteith equation to calculate reference evapotranspiration ($ET_0$)[cite: 3]:

$$ET_0 = \frac{0.408\Delta(R_n - G) + \gamma \frac{900}{T + 273} u_2 (e_s - e_a)}{\Delta + \gamma(1 + 0.34 u_2)}$$

Where:
* $R_n$: Net radiation
* $G$: Soil heat flux density
* $T$: Mean daily air temperature
* $u_2$: Wind speed at 2m height
* $e_s - e_a$: Vapor pressure deficit
* $\Delta$: Slope of the vapor pressure curve
* $\gamma$: Psychrometric constant

## Hardware Components 🛠️
[cite_start]The monitoring station is installed at a height of **2 meters** for accurate meteorological readings[cite: 5, 6].

* **Microcontroller:** ESP32 Development Board
* [cite_start]**ADC Module:** ADS1115 (16-Bit) [cite: 18]
* [cite_start]**Temperature & Humidity:** SHT31 Sensor [cite: 6]
* [cite_start]**Wind Speed:** Anemometer (measured at 2m) [cite: 5]
* [cite_start]**Rain Gauge:** Tipping Bucket mechanism (0.5mm resolution) [cite: 8, 10]
* [cite_start]**Solar Radiation:** Pyranometer / Light Sensor [cite: 11]
* **Actuator:** Relay Module & Water Pump

## Wiring Diagram
![Wiring Schematic](path/to/your_wiring_image.png)
*(Note: Upload the wiring diagram image to your repository and update this link)*

## Software Stack 💻
* **Firmware:** C++ (Arduino IDE) for ESP32 sensor management and MQTT communication.
* [cite_start]**Backend Analysis:** Python script for retrieving data and performing the complex math ($ET_0$ calculation)[cite: 13].
* **Cloud Platform:** ThingSpeak (Data logging and MQTT broker).

## How to Run
1.  **ESP32:** Upload the `.ino` code from the `src` folder to the board. Ensure WiFi credentials and ThingSpeak API keys are configured.
2.  **Python Server:** Run the analysis script on a computer with internet access. The script should be scheduled to run before 19:00 daily.
