# model.py

import torch
import torch.nn as nn

#############################################
# Model Architecture
#############################################

class ResBlock(nn.Module):
    def __init__(self, channels, kernel_size=3):
        super().__init__()
        padding = kernel_size // 2

        self.conv1 = nn.Conv1d(channels, channels, kernel_size, padding=padding)
        self.bn1 = nn.BatchNorm1d(channels)

        self.conv2 = nn.Conv1d(channels, channels, kernel_size, padding=padding)
        self.bn2 = nn.BatchNorm1d(channels)

    def forward(self, x):
        residual = x
        x = torch.relu(self.bn1(self.conv1(x)))
        x = self.bn2(self.conv2(x))
        return torch.relu(x + residual)


class EEGNetDual(nn.Module):
    def __init__(self, n_bp_features):
        super().__init__()

        # Raw CNN branch
        self.conv1 = nn.Conv1d(1, 32, kernel_size=25, padding=12)
        self.bn1 = nn.BatchNorm1d(32)
        self.pool1 = nn.MaxPool1d(4)

        self.res1 = ResBlock(32, kernel_size=7)
        self.pool2 = nn.MaxPool1d(4)

        self.conv2 = nn.Conv1d(32, 64, kernel_size=5, padding=2)
        self.bn2 = nn.BatchNorm1d(64)
        self.pool3 = nn.MaxPool1d(4)

        self.res2 = ResBlock(64, kernel_size=3)

        self.global_avg = nn.AdaptiveAvgPool1d(1)

        # Bandpower MLP branch
        self.bp_fc1 = nn.Linear(n_bp_features, 32)
        self.bp_bn1 = nn.BatchNorm1d(32)
        self.bp_fc2 = nn.Linear(32, 32)

        # Fusion classifier head
        self.dropout = nn.Dropout(0.5)
        self.fc = nn.Linear(64 + 32, 3)

    def forward(self, x_raw, x_bp):
        # CNN branch
        x = torch.relu(self.bn1(self.conv1(x_raw)))
        x = self.pool1(x)
        x = self.res1(x)
        x = self.pool2(x)

        x = torch.relu(self.bn2(self.conv2(x)))
        x = self.pool3(x)
        x = self.res2(x)

        x = self.global_avg(x).squeeze(-1)

        # Bandpower branch
        b = torch.relu(self.bp_bn1(self.bp_fc1(x_bp)))
        b = torch.relu(self.bp_fc2(b))

        # Fusion
        out = torch.cat([x, b], dim=1)
        out = self.dropout(out)

        return self.fc(out)


#############################################
# Model Loader
#############################################

def load_model(model_path, n_bp_features, device="cpu"):
    model = EEGNetDual(n_bp_features)
    model.load_state_dict(torch.load(model_path, map_location=device))
    model.to(device)
    model.eval()
    return model