import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:audioplayers/audioplayers.dart';

const String SERVER_URL = "https://alexa-overtrue-tomiko.ngrok-free.dev";

void main() => runApp(const MyApp());

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Seizure Alert',
      theme: ThemeData(colorSchemeSeed: Colors.blue),
      home: const AlertHomePage(),
    );
  }
}

class AlertHomePage extends StatefulWidget {
  const AlertHomePage({super.key});
  @override
  State<AlertHomePage> createState() => _AlertHomePageState();
}

class _AlertHomePageState extends State<AlertHomePage> {
  String _status = "Connecting to server...";
  int _lastPrediction = -1;
  bool _alertActive = false;
  Timer? _pollingTimer;
  final AudioPlayer _audioPlayer = AudioPlayer();

  @override
  void initState() {
    super.initState();
    _startPolling();
  }

  @override
  void dispose() {
    _pollingTimer?.cancel();
    _audioPlayer.dispose();
    super.dispose();
  }

  void _startPolling() {
    _pollingTimer = Timer.periodic(const Duration(seconds: 3), (_) {
      _checkStatus();
    });
  }

  Future<void> _checkStatus() async {
    try {
      final response = await http
          .get(Uri.parse("$SERVER_URL/status"))
          .timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        final prediction = data["prediction"] as int;
        final acknowledged = data["acknowledged"] as bool;

        setState(() {
          _lastPrediction = prediction;
          _status = "Connected";
        });

        if ((prediction == 2) && !acknowledged && !_alertActive) {
          setState(() => _alertActive = true);
          _triggerAlert();
        } else if (prediction == 1 && !_alertActive) {
          _triggerAlert();
        }
      }
    } catch (e) {
      setState(() => _status = "Server unreachable — retrying...");
    }
  }

  Future<void> _triggerAlert() async {
    await _audioPlayer.setVolume(1.0);
    await _audioPlayer.setReleaseMode(ReleaseMode.loop);
    await _audioPlayer.play(AssetSource('alert.mp3'));
  }

  Future<void> _dismissAlert() async {
    try {
      await http
          .post(Uri.parse("$SERVER_URL/acknowledge"))
          .timeout(const Duration(seconds: 5));
    } catch (e) {
      print("Failed to send ACK: $e");
    }
    setState(() => _alertActive = false);
    _audioPlayer.stop();
  }

  // Returns label, color, and icon based on prediction value
  Map<String, dynamic> _predictionInfo(int prediction) {
    switch (prediction) {
      case 0:
        return {
          "label": "Interictal",
          "sublabel": "Normal brain activity",
          "color": Colors.green,
          "icon": Icons.check_circle,
        };
      case 1:
        return {
          "label": "Preictal",
          "sublabel": "Pre-seizure activity detected",
          "color": Colors.orange,
          "icon": Icons.warning_rounded,
        };
      case 2:
        return {
          "label": "Ictal",
          "sublabel": "Seizure in progress",
          "color": Colors.red,
          "icon": Icons.emergency,
        };
      default:
        return {
          "label": "Waiting...",
          "sublabel": "No prediction received yet",
          "color": Colors.grey,
          "icon": Icons.hourglass_empty,
        };
    }
  }

  @override
  Widget build(BuildContext context) {
    final info = _predictionInfo(_lastPrediction);

    return Scaffold(
      appBar: AppBar(title: const Text("Seizure Monitor")),
      body: Stack(
        children: [
          Center(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [

                  // Big status icon
                  Icon(
                    info["icon"] as IconData,
                    size: 100,
                    color: info["color"] as Color,
                  ),
                  const SizedBox(height: 24),

                  // Prediction label
                  Text(
                    info["label"] as String,
                    style: TextStyle(
                      fontSize: 32,
                      fontWeight: FontWeight.bold,
                      color: info["color"] as Color,
                    ),
                  ),
                  const SizedBox(height: 8),

                  // Prediction sublabel
                  Text(
                    info["sublabel"] as String,
                    style: const TextStyle(fontSize: 16, color: Colors.grey),
                  ),
                  const SizedBox(height: 40),

                  // Connection status pill
                  Container(
                    padding: const EdgeInsets.symmetric(
                        horizontal: 16, vertical: 8),
                    decoration: BoxDecoration(
                      color: _status == "Connected"
                          ? Colors.green.withOpacity(0.1)
                          : Colors.red.withOpacity(0.1),
                      borderRadius: BorderRadius.circular(20),
                      border: Border.all(
                        color: _status == "Connected"
                            ? Colors.green
                            : Colors.red,
                      ),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          _status == "Connected"
                              ? Icons.wifi
                              : Icons.wifi_off,
                          size: 16,
                          color: _status == "Connected"
                              ? Colors.green
                              : Colors.red,
                        ),
                        const SizedBox(width: 8),
                        Text(
                          _status,
                          style: TextStyle(
                            color: _status == "Connected"
                                ? Colors.green
                                : Colors.red,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),

          // Full screen seizure alert overlay
          if (_alertActive)
            GestureDetector(
              onTap: _dismissAlert,
              child: Container(
                color: Colors.red.withOpacity(0.95),
                child: Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const Icon(Icons.emergency,
                          size: 120, color: Colors.white),
                      const SizedBox(height: 20),
                      const Text("SEIZURE DETECTED",
                          style: TextStyle(
                              fontSize: 36,
                              fontWeight: FontWeight.bold,
                              color: Colors.white)),
                      const SizedBox(height: 12),
                      const Text("Ictal activity in progress",
                          style:
                              TextStyle(fontSize: 18, color: Colors.white70)),
                      const SizedBox(height: 40),
                      ElevatedButton.icon(
                        onPressed: _dismissAlert,
                        icon: const Icon(Icons.check),
                        label: const Text("Acknowledge & Dismiss"),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: Colors.white,
                          foregroundColor: Colors.red,
                          padding: const EdgeInsets.symmetric(
                              horizontal: 24, vertical: 12),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          // Preictal warning banner
        if (_lastPrediction == 1 && !_alertActive)
          Positioned(
            bottom: 0,
            left: 0,
            right: 0,
            child: Container(
              color: Colors.orange,
              padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Row(
                    children: [
                      Icon(Icons.warning_rounded, color: Colors.white),
                      SizedBox(width: 8),
                      Text(
                        "Pre-seizure activity detected",
                        style: TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.bold,
                          fontSize: 14,
                        ),
                      ),
                    ],
                  ),
                  ElevatedButton(
                    onPressed: () async {
                      try {
                        await http
                            .post(Uri.parse("$SERVER_URL/acknowledge"))
                            .timeout(const Duration(seconds: 5));
                      } catch (e) {
                        print("Failed to send ACK: $e");
                      }
                      setState(() => _lastPrediction = -1);
                      _audioPlayer.stop();
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.white,
                      foregroundColor: Colors.orange,
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    ),
                    child: const Text("Dismiss"),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}