# ESP32-S3 Parking Assistance System Firmware

## Course Project: Advanced Parking Assistance System

This repository contains the server-side firmware for an advanced parking assistance system, developed as a course project for "Circuit Engineering". The system utilizes an ESP32-S3 microcontroller to integrate a live video feed with real-time distance measurements from ultrasonic sensors, providing comprehensive situational awareness to the driver via a web-based client.

## Table of Contents

1.  [Project Overview](#project-overview)
2.  [System Architecture](#system-architecture)
3.  [Key Features](#key-features)
4.  [Hardware Requirements](#hardware-requirements)
5.  [Firmware Details](#firmware-details)
    *   [Core Technology](#core-technology)
    *   [Multitasking with FreeRTOS](#multitasking-with-freertos)
    *   [Communication Protocol](#communication-protocol)
    *   [Persistent Configuration](#persistent-configuration)
6.  [Installation and Setup](#installation-and-setup)
7.  [How It Works](#how-it-works)
8.  [Pinout Configuration](#pinout-configuration)

## Project Overview

The primary goal of this project is to design and implement a cost-effective yet powerful parking aid. The system activates automatically when the vehicle's reverse gear is engaged. Upon activation, it initiates a Wi-Fi Access Point, starts a web server, and begins streaming video from a rear-mounted camera. Simultaneously, three ultrasonic sensors measure the distance to obstacles, and this data is sent to the client in real-time to be overlaid on the video feed, accompanied by audible alerts.

## System Architecture

The system is architected around the ESP32-S3 microcontroller, leveraging its dual-core processing capabilities and integrated peripherals.

*   **Processing Core (ESP32-S3-WROOM-1-N16R8):** This powerful MCU serves as the brain of the system. Its dual-core architecture is ideal for handling concurrent tasks: one core is dedicated to camera data processing and streaming, while the other manages sensor polling, web server requests, and WebSocket communication.
*   **Visual System (OV5640 Camera):** An OV5640 camera module captures the video feed. It is interfaced with the ESP32-S3 via the parallel camera interface (I2S). The firmware configures the camera, grabs frames in JPEG format, and prepares them for streaming.
*   **Perception System (3x AJ-SR04M Ultrasonic Sensors):** An array of three waterproof ultrasonic sensors provides distance data. The firmware triggers each sensor sequentially and measures the time-of-flight of the echo pulse using GPIO interrupts. A smoothing algorithm is applied to the raw data to provide stable readings.
*   **Activation Circuit:** A simple voltage divider or logic-level converter circuit is used to safely detect the 12V signal from the vehicle's reverse light. This signal triggers an interrupt on the ESP32-S3, activating and deactivating the parking assistance mode.
*   **User Interface (Web Server & WebSockets):** The ESP32-S3 hosts a web server on a local Wi-Fi AP. The user interface is a single-page web application served to any connected client (smartphone, tablet). A dedicated WebSocket is used for the low-latency video stream (`/ws_stream`), while another WebSocket (`/ws`) transmits JSON-formatted sensor data and system status.

## Key Features

*   **Automatic Activation:** The system starts automatically upon engagement of the reverse gear.
*   **Real-time Video Streaming:** Low-latency MJPEG video stream over WebSocket, viewable in any modern web browser.
*   **Triple Ultrasonic Sensor Array:** Provides distance readings from the left, center, and right zones behind the vehicle.
*   **Web-Based Interface:** No native mobile app required. The interface is accessible via a web browser.
*   **Wi-Fi Access Point:** The device creates its own Wi-Fi network for easy client connection.
*   **Persistent Settings:** Camera and system settings are saved to the onboard flash memory (LittleFS) and can be configured through the web UI.
*   **Multitasked Operation:** A robust, real-time operating system (FreeRTOS) ensures smooth, concurrent operation of all subsystems (camera, sensors, networking).

## Hardware Requirements

| Component                          | Quantity | Description                                                              |
| ---------------------------------- | :------: | ------------------------------------------------------------------------ |
| ESP32-S3-WROOM-1-N16R8 Module      |    1     | The main microcontroller.                                                |
| OV5640 Camera Module               |    1     | 5MP camera with a parallel interface.                                    |
| AJ-SR04M Ultrasonic Sensor         |    3     | Waterproof ultrasonic distance measurement modules.                      |
| Piezo Buzzer / Speaker             |    1     | For audible distance alerts.                                             |
| Logic Level Converter / Optocoupler |    1     | To safely interface the 12V reverse gear signal with the 3.3V ESP32 GPIO. |
| 3.3V Power Supply                  |    1     | A stable power source capable of delivering at least 1A.                 |
| Miscellaneous                      |   N/A    | Wires, connectors, resistors, and a suitable enclosure.                  |

## Firmware Details

### Core Technology

*   **Framework:** Arduino
*   **Build System:** PlatformIO
*   **Core Libraries:**
    *   `espressif32` Platform for ESP32 support.
    *   `ESPAsyncWebServer` for handling HTTP requests and WebSocket communication efficiently.
    *   `ArduinoJson` for serialization and deserialization of settings and sensor data.
    *   `LittleFS` for the onboard file system to store settings.

### Multitasking with FreeRTOS

The firmware's stability and performance rely on a task-based architecture using FreeRTOS. Key tasks are pinned to specific cores to optimize performance:

*   **`camera_task` (Core 0):** Manages the lifecycle of the camera. It handles the complex and time-consuming `esp_camera_init()` and `esp_camera_deinit()` operations. It is controlled by event bits, activating only when a video stream is requested.
*   **`frame_grab_task` (Core 0):** Continuously captures frames from the initialized camera and places them into a single-item queue (`xFrameQueue`). This decouples the high-frequency frame grabbing from the network streaming logic.
*   **`stream_task` (Core 1):** Waits for a frame to appear in the queue, retrieves it, and broadcasts it to all connected WebSocket clients. It also handles returning the frame buffer back to the camera driver.
*   **`sensors_task` (Core 1):** A dedicated task that periodically triggers the three ultrasonic sensors, reads their echo times via interrupts, calculates the distances, and updates the global application state.
*   **`broadcast_sensors_task` (Core 1):** Reads the latest sensor data from the global state and pushes it as a JSON payload to clients connected to the main WebSocket.
*   **`async_tcp` (Core 0/1):** The underlying tasks for the web server, managed by the ESPAsyncWebServer library.

### Communication Protocol

*   **HTTP Server (Port 80):**
    *   `GET /`: Serves the main `index.html` and other static assets (CSS, JS).
    *   `GET /api/settings`: Retrieves the current system settings as a JSON object.
    *   `POST /api/settings`: Updates system settings from a submitted JSON object.
    *   `GET /api/snapshot`: Provides a single JPEG snapshot from the camera.
*   **WebSocket Servers:**
    *   `/ws`: A general-purpose WebSocket for bi-directional communication. The server pushes sensor data through this channel.
    *   `/ws_stream`: A dedicated, high-throughput WebSocket for broadcasting binary JPEG frame data.

### Persistent Configuration

All system settings (e.g., camera resolution, JPEG quality, Wi-Fi credentials, sensor thresholds) are stored in a `settings.json` file in the LittleFS partition. This allows settings to be preserved across reboots. The `settings_manager` component handles loading settings on boot, saving them when changed, and resetting to defaults.

## Installation and Setup

1.  **Prerequisites:** Ensure you have [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/platformio-ide) installed.
2.  **Clone Repository:** Clone this repository to your local machine.
    ```bash
    git clone [your-repository-url]
    ```
3.  **Open Project:** Open the cloned folder in Visual Studio Code. PlatformIO should automatically recognize it as a project.
4.  **Hardware Connection:** Connect the camera and sensors to the ESP32-S3 according to the pin definitions in `include/config.h`.
5.  **Build and Upload:** Use the PlatformIO controls to build and upload the firmware to your ESP32-S3 board.

## How It Works

1.  **Power-Up:** On power-up, the ESP32-S3 initializes its file system, loads settings, and creates a Wi-Fi Access Point with the SSID defined in the settings (default: `ESP32_Park_AP`).
2.  **Client Connection:** The user connects their smartphone or tablet to this Wi-Fi network and navigates to the device's IP address (usually `192.168.4.1`) in a web browser.
3.  **Idle State:** The system is now in a standby state, waiting for the reverse gear signal. The camera remains uninitialized to save power.
4.  **Activation:** When the reverse gear is engaged, the corresponding GPIO pin on the ESP32-S3 is pulled HIGH. This signals the `camera_task` to initialize the camera and the `frame_grab_task` to start capturing frames.
5.  **Streaming:** The client-side application automatically connects to the `/ws_stream` WebSocket. The `stream_task` begins broadcasting video frames, and the `broadcast_sensors_task` starts sending distance data.
6.  **Deactivation:** When the reverse gear is disengaged, the GPIO pin goes LOW, and the system gracefully de-initializes the camera and stops the streams to conserve power.

## Pinout Configuration

All hardware pin assignments are centralized in the `include/config.h` file. Please review and adjust this file to match your specific hardware connections.

```c
// include/config.h

// --- Sensor Pinouts ---
const SensorConfig SENSOR_PINS[NUM_SENSORS] = {
    {42, 41}, // Left {Trig, Echo}
    {45, 48}, // Center {Trig, Echo}
    {47, 21}  // Right {Trig, Echo}
};

// --- Camera Pinouts (OV5640) ---
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
// ... other camera pins
```