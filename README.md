# auralert — Real-Time Seizure Prediction System

auralert is an end-to-end seizure prediction system that acquires EEG signals from a wearable device, classifies them in real time using a trained deep learning model hosted on a web server, and delivers classification alerts to a mobile application. The system is designed to provide advance warning before a seizure event, giving patients and caregivers critical response time.

---

## Table of Contents

- [System Overview](#system-overview)
- [Architecture](#architecture)
- [Repository Structure](#repository-structure)
- [Prerequisites](#prerequisites)
- [Setup & Usage](#setup--usage)
  - [1. Web Server](#1-web-server-web_server_hosting)
  - [2. ESP32 Firmware](#2-esp32-firmware-esp32_codeino)
  - [3. Mobile Application](#3-mobile-application-auralert_mobile_application)
  - [4. Model Notebook (Optional)](#4-model-notebook-final_modelipynb)
- [API Endpoints](#api-endpoints)
- [Dataset](#dataset)
- [Typical Workflow](#typical-workflow)
- [Notes & Limitations](#notes--limitations)

---

## System Overview

The pipeline consists of four interconnected components:

1. **EEG Acquisition Hardware** — An EEG click board captures brain electrical activity and feeds the analog signal to an ESP32 microcontroller.
2. **ESP32 Firmware** — The ESP32 digitizes the analog EEG signal, packages it as binary data, and transmits it over Wi-Fi to the hosted prediction server.
3. **Web Server** — A FastAPI server loads the trained CNN-MLP model and exposes prediction endpoints. It is made publicly accessible via an ngrok tunnel.
4. **Mobile Application** — An iOS Flutter app connects to the server and displays real-time classification reports for patient and caregiver monitoring.

---

## Repository Structure

```
.
├── web_server_hosting/          # FastAPI server + model state
│   ├── app.py                   # Server entry point; registers all routes
│   ├── inference.py             # Preprocessing and model inference logic
│   └── model.py                 # Model architecture definition
│
├── auralert_mobile_application/ # iOS Flutter application
│   └── ...                      # Flutter project files
│
├── ESP32_code.ino               # Arduino firmware for the ESP32
│
└── Final_Model.ipynb            # Jupyter notebook — model training & evaluation
```

---

## Prerequisites

### General
- An ESP32 microcontroller connected to an EEG acquisition board
- A machine capable of running Python to host the server
- An ngrok account ([sign up free at ngrok.com](https://ngrok.com))
- Flutter SDK installed for running the mobile application
- Arduino IDE (or PlatformIO) for uploading ESP32 firmware

### Python Dependencies (Web Server)
Install required packages before starting the server:

```bash
pip install fastapi uvicorn numpy torch
```

> Refer to any `requirements.txt` inside `web_server_hosting/` if present, and install from there instead:
> ```bash
> pip install -r web_server_hosting/requirements.txt
> ```

### Flutter Dependencies (Mobile App)
From inside the `auralert_mobile_application/` directory:

```bash
flutter pub get
```

---

## Setup & Usage

### 1. Web Server (`web_server_hosting/`)

The server uses FastAPI served via uvicorn and is exposed publicly using ngrok. It loads the saved model state and handles incoming EEG data from the ESP32.

#### Step 1 — Start the FastAPI server

Open a terminal, navigate to the `web_server_hosting/` directory, and run:

```bash
uvicorn app:app --host 0.0.0.0 --port 8000
```

The server will start locally on port `8000`. You can verify it is running by visiting `http://localhost:8000/status` in a browser.

#### Step 2 — Expose the server via ngrok

Open a **second terminal** and run:

```bash
ngrok http 8000
```

ngrok will display a public forwarding URL such as:

```
Forwarding   https://abc123.ngrok-free.app -> http://localhost:8000
```

Copy this URL — you will need it for the ESP32 firmware configuration.

> **Important:** You must be signed into your ngrok account for the tunnel to stay active. Run `ngrok config add-authtoken <YOUR_TOKEN>` with the token from your [ngrok dashboard](https://dashboard.ngrok.com) before using ngrok.

---

### 2. ESP32 Firmware (`ESP32_code.ino`)

The ESP32 reads analog EEG data from the EEG click board, converts it to binary, and transmits it as HTTP POST requests to the prediction server.

#### Before uploading — required configuration changes

Open `ESP32_code.ino` in the Arduino IDE and update the following fields:

```cpp
// Wi-Fi credentials — replace with your network details
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ngrok server URL — replace with the URL from your ngrok terminal
const char* serverURL = "https://YOUR_NGROK_URL.ngrok-free.app/predict";
```

#### Upload the firmware

1. Connect the ESP32 to your computer via USB.
2. In the Arduino IDE, select the correct **Board** (`ESP32 Dev Module` or equivalent) and **Port**.
3. Click **Upload**.

Once uploaded, the ESP32 will automatically connect to Wi-Fi, begin reading EEG data, and POST binary windows to the server. Classification responses will be printed to the ESP32's Serial Monitor.

---

### 3. Mobile Application (`auralert_mobile_application/`)

The auralert iOS app connects to the server and displays real-time seizure classification reports. It is intended for patient and caregiver monitoring.

#### Run the app

Make sure you have a connected iOS device or simulator, then from the `auralert_mobile_application/` directory run:

```bash
flutter run
```

> Ensure the server URL configured within the app matches the active ngrok URL. If the ngrok URL changes between sessions (which it does on free accounts), update the URL in the app's configuration file and re-run.

The app polls the server for classification results and presents alerts when a pre-seizure pattern is detected.

---

### 4. Model Notebook (`Final_Model.ipynb`)

> **You do not need to use this notebook to run the system.** A trained model state is already included in the `web_server_hosting/` folder and is loaded automatically when the server starts.

Use this notebook only if you want to:
- Understand how the model was built and trained
- Retrain the model with different hyperparameters or data splits
- Evaluate model performance on CHB-MIT
- Save a new model state to replace the one in `web_server_hosting/`

#### Model Architecture

The model is a **dual-input CNN-MLP**:
- A **Convolutional Neural Network (CNN)** branch processes the raw EEG time-series to extract temporal and spectral features.
- A **Multi-Layer Perceptron (MLP)** branch processes bandpower features derived from the EEG window.
- The outputs of both branches are concatenated and passed through a fully connected layer to produce a classification.

#### Training Data

The model was trained and evaluated on the **CHB-MIT Scalp EEG Database**:
> Shoeb, A. H. (2009). *Application of Machine Learning to Epileptic Seizure Onset Detection and Treatment*. MIT.
> Dataset available at: [https://physionet.org/content/chbmit/1.0.0/](https://physionet.org/content/chbmit/1.0.0/)

To use the notebook, download the CHB-MIT dataset from PhysioNet, update the data path in the notebook, and run all cells. If you save a new model state at the end, copy it into the `web_server_hosting/` directory and update `model.py` or `inference.py` to reference the new filename.

---

## Typical Workflow

Here is the recommended sequence to get the full system running end-to-end:

1. **Start the FastAPI server** in Terminal 1:
   ```bash
   cd web_server_hosting
   uvicorn app:app --host 0.0.0.0 --port 8000
   ```

2. **Start the ngrok tunnel** in Terminal 2:
   ```bash
   ngrok http 8000
   ```
   Copy the displayed public URL.

3. **Update and upload ESP32 firmware** — paste your Wi-Fi credentials and ngrok URL into `ESP32_code.ino`, then upload it to your ESP32 via the Arduino IDE.

4. **Monitor output** — you can view classification results in any of three ways:
   - **Browser**: Navigate to `https://NGROK_URL`
   - **Serial Monitor**: Open the Arduino IDE Serial Monitor to see ESP32 output
   - **Mobile App**: Run `flutter run` in the `auralert_mobile_application/` directory

---

## Notes & Limitations

- **Wi-Fi dependency** — The ESP32 must be on the same network (or any network with internet access) to reach the ngrok tunnel. Update SSID/password whenever changing networks.
- **Model generalization** — The CNN-MLP was trained on the CHB-MIT dataset. Performance on data from different EEG hardware, electrode configurations, or patient populations may vary. Retraining on domain-specific data using `Final_Model.ipynb` is recommended for clinical or research deployment.
- **iOS only** — The auralert mobile application is currently built for iOS. Running on Android would require additional Flutter platform configuration.
- **Not a medical device** — auralert is a research and development prototype built for a senior design project. It has not been clinically validated or approved as a medical device, does not exhibit levels of accuracy on par with clinical devices, and should not be used for clinical decision-making.