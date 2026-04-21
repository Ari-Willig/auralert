import json
import torch
import numpy as np
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from model import load_model
from inference import Predictor

#############################################
# Configuration
#############################################
# uvicorn app:app --host 0.0.0.0 --port 8000
# ngrok http 8000
# curl -X POST https://alexa-overtrue-tomiko.ngrok-free.dev/test/0

MODEL_PATH = "new_model.pth"
ARTIFACT_PATH = "training_artifacts.json"
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

#############################################
# Load artifacts
#############################################

with open(ARTIFACT_PATH, "r") as f:
    artifacts = json.load(f)

bp_mean = artifacts["bp_mean"]
bp_std = artifacts["bp_std"]
n_bp_features = artifacts["n_bp_features"]

#############################################
# Load model
#############################################

model = load_model(MODEL_PATH, n_bp_features, DEVICE)
predictor = Predictor(
    model=model,
    bp_mean=bp_mean,
    bp_std=bp_std,
    device=DEVICE
)

#############################################
# FastAPI app
#############################################

app = FastAPI()

CLASS_NAMES = ["Interictal", "Preictal", "Ictal"]
EXPECTED_SAMPLES = 1280  # channels * sequence_length

# Stores the latest prediction state in memory
latest_prediction = {"prediction": -1, "acknowledged": False}

#############################################
# Endpoints
#############################################

@app.post("/test/{prediction_value}")
async def test_prediction(prediction_value: int):
    latest_prediction["prediction"] = prediction_value
    latest_prediction["acknowledged"] = False
    return {"status": "ok", "prediction": prediction_value}

@app.post("/predict")
async def predict(request: Request):
    body = await request.body()

    # Validate payload size
    expected_bytes = EXPECTED_SAMPLES * 4  # float32 = 4 bytes
    if len(body) != expected_bytes:
        return {
            "error": f"Expected {expected_bytes} bytes ({EXPECTED_SAMPLES} float32 values), got {len(body)} bytes"
        }, 400

    # Decode binary payload into float32 array
    signal = np.frombuffer(body, dtype=np.float32).tolist()
    probs = predictor.predict(signal)
    predicted_index = int(probs.argmax())

    # Store latest prediction for the app to poll
    # Only update if not currently in an unacknowledged seizure state
    if not (latest_prediction["prediction"] == 2 and not latest_prediction["acknowledged"]):
        latest_prediction["prediction"] = predicted_index
        latest_prediction["acknowledged"] = False

    return {
        "prediction": predicted_index,
        "label": CLASS_NAMES[predicted_index],
        "probabilities": {
            CLASS_NAMES[i]: round(float(probs[i]), 4)
            for i in range(len(CLASS_NAMES))
        }
    }

@app.get("/status")
async def status():
    # Phone polls this endpoint to check for seizure alerts
    return JSONResponse(latest_prediction)

@app.post("/acknowledge")
async def acknowledge():
    # Phone calls this when user dismisses the alert
    latest_prediction["acknowledged"] = True
    latest_prediction["prediction"] = -1
    return {"status": "ok"}

@app.get("/")
def health_check():
    return {"status": "ok"}