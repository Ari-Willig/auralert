# inference.py

import numpy as np
import torch
from scipy.signal import welch

#############################################
# Bandpower Feature Extraction
#############################################

def compute_bandpower_features(X_raw, sfreq=256):
    bands = {
        "delta": (0.5, 4),
        "theta": (4, 8),
        "alpha": (8, 13),
        "beta": (13, 30),
        "gamma": (30, 40),
    }

    features = []

    # NumPy 1.x had np.trapz; in NumPy 2.x it's renamed to np.trapezoid.
    # Pick whichever exists so this works in both environments.
    if hasattr(np, "trapezoid"):
        _trapz = np.trapezoid
    else:
        _trapz = np.trapz

    for i in range(X_raw.shape[0]):
        sig = X_raw[i, 0, :]

        freqs, psd = welch(sig, fs=sfreq, nperseg=sfreq)

        row = []
        total_power = _trapz(psd, freqs) + 1e-8

        for lo, hi in bands.values():
            idx = np.logical_and(freqs >= lo, freqs <= hi)
            bp = _trapz(psd[idx], freqs[idx])
            row.append(bp / total_power)

        row.append(np.log(np.var(sig) + 1e-8))
        features.append(row)

    return np.array(features, dtype=np.float32)


#############################################
# Inference Pipeline
#############################################

class Predictor:
    def __init__(self, model, bp_mean, bp_std, device="cpu"):
        self.model = model
        self.bp_mean = np.array(bp_mean)
        self.bp_std = np.array(bp_std)
        self.device = device

    def preprocess(self, raw_signal):
        """
        raw_signal: list or numpy array
        Expected shape: (n_timepoints,)
        """

        X_raw = np.array(raw_signal)[None, None, :]

        bp = compute_bandpower_features(X_raw)

        # Normalize bandpower features
        bp = (bp - self.bp_mean) / self.bp_std

        return torch.tensor(X_raw, dtype=torch.float32), \
               torch.tensor(bp, dtype=torch.float32)

    def predict(self, raw_signal):
        X_raw, X_bp = self.preprocess(raw_signal)

        X_raw = X_raw.to(self.device)
        X_bp = X_bp.to(self.device)

        with torch.no_grad():
            logits = self.model(X_raw, X_bp)
            probs = torch.softmax(logits, dim=1)

        return probs.cpu().numpy()[0]