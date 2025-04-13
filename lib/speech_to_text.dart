import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:speech_to_text/speech_recognition_result.dart';
import 'package:speech_to_text/speech_to_text.dart';

class VoiceAssistant extends StatefulWidget {
  const VoiceAssistant({super.key});

  @override
  State<VoiceAssistant> createState() => _VoiceAssistantState();
}

class _VoiceAssistantState extends State<VoiceAssistant> {
  final SpeechToText _speechToText = SpeechToText();
  final DatabaseReference _dbDeviceRef = FirebaseDatabase.instance.ref('devices');
  Map<String, Map<String, String>> _validAppliances = {};

  bool _speechEnabled = false;
  String _wordsSpoken = "Say a command like 'Turn on the kitchen light'";
  double _confidenceLevel = 0;

  @override
  void initState() {
    super.initState();
    initSpeech();
    fetchAppliances();
    testFirebase();
  }

  void initSpeech() async{
    _speechEnabled = await _speechToText.initialize();
    setState(() {});
  }

  void _startListening() async{
    await _speechToText.listen(onResult: _onSpeechResult);
    setState(() {
      _confidenceLevel = 0;
    });
  }

  void _stopListening() async {
    await _speechToText.stop();
    setState(() {});
  }

  void _onSpeechResult(SpeechRecognitionResult result) {
    if(result.finalResult){
      setState(() {
        _wordsSpoken = result.recognizedWords;
        _confidenceLevel = result.confidence;
      });

      String? command = _processCommand(_wordsSpoken);
      print('command: $command');

      if (command != null) {
        sendToFirebase(command);
      }
    }
  }

  // Send valid command to Firebase
  void sendToFirebase(String command) {
    // Split multiple action parts: e.g., ["ON:kitchen_light,bathroom_light", "OFF:AC,fan"]
    List<String> commandParts = command.split(' ');

    for (String part in commandParts) {
      List<String> segments = part.split(':');
      if (segments.length != 2) continue;

      String action = segments[0]; // "ON" or "OFF"
      List<String> appliances = segments[1].split(',');

      for (String appliance in appliances) {
        _dbDeviceRef.child(appliance).set({"status": action});
      }
    }
  }

  String? _processCommand(String speech) {
    speech = speech.toLowerCase().trim();
    print('Processed speech: $speech');
    print('Appliance: $_validAppliances');

    // Pattern to match each command block
    RegExp blockPattern = RegExp(
      r"(turn on|turn off)\s+((?:(?!turn on|turn off).)*?)(?:\s+except\s+((?:(?!turn on|turn off).)*))?(?=turn on|turn off|$)",
      caseSensitive: false,
      dotAll: true,
    );

    Iterable<RegExpMatch> blocks = blockPattern.allMatches(speech);
    List<String> foundAppliancesOn = [];
    List<String> foundAppliancesOff = [];

    if (blocks.isEmpty) {
      _showConfirmationDialog("No valid command detected. Please say 'turn on' or 'turn off' followed by an appliance name.");
      return null;
    }

    for (var match in blocks) {
      String command = match.group(1)!.trim(); // "turn on" or "turn off"
      String targetsRaw = match.group(2)?.trim() ?? "";
      String excludeRaw = match.group(3)?.trim() ?? "";

      print("\n🧱 Processing Block: [$command] [$targetsRaw] [except $excludeRaw]");

      bool isTurnOn = command == "turn on";
      bool allDevices = targetsRaw.contains("all devices") || targetsRaw.contains("all device");

      for (String appliance in _validAppliances.keys) {
        String applianceLower = appliance.toLowerCase();
        String originalKey = _validAppliances[appliance]!["originalKey"]!;
        String currentStatus = _validAppliances[appliance]!["status"]!;

        bool isExcluded = excludeRaw.contains(applianceLower);
        bool isTargeted = targetsRaw.contains(applianceLower);

        // Handle all devices
        if (allDevices && !isExcluded) {
          if (isTurnOn && currentStatus != "ON") {
            foundAppliancesOn.add(originalKey);
            print('✅ Turn ON Appliance (all): $originalKey');
          } else if (!isTurnOn && currentStatus != "OFF") {
            foundAppliancesOff.add(originalKey);
            print('✅ Turn OFF Appliance (all): $originalKey');
          }
        }

        // Handle specific devices
        if (!allDevices && isTargeted && !isExcluded) {
          if (isTurnOn && currentStatus != "ON") {
            foundAppliancesOn.add(originalKey);
            print('✅ Turn ON Appliance: $originalKey');
          } else if (!isTurnOn && currentStatus != "OFF") {
            foundAppliancesOff.add(originalKey);
            print('✅ Turn OFF Appliance: $originalKey');
          }
        }

        if (isExcluded && isTargeted) {
          print('❌ Excluding appliance: $originalKey');
        }
      }
    }

    if (foundAppliancesOn.isEmpty && foundAppliancesOff.isEmpty) {
      print("❌ No matching appliances found or appliances are already in desired states");
      return null;
    }

    print("✅ Matched Appliances (ON): $foundAppliancesOn");
    print("✅ Matched Appliances (OFF): $foundAppliancesOff");

    String result = "";
    if (foundAppliancesOn.isNotEmpty) {
      result += "ON:${foundAppliancesOn.join(',')}";
    }
    if (foundAppliancesOff.isNotEmpty) {
      if (result.isNotEmpty) result += " ";
      result += "OFF:${foundAppliancesOff.join(',')}";
    }

    return result;
  }

  void _showConfirmationDialog(String message) {
    showDialog(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: Text("Confirm Command"),
          content: Text(message),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: Text("Cancel"),
            ),
            TextButton(
              onPressed: () {
                Navigator.pop(context);
                _startListening(); // Restart listening for a clearer command
              },
              child: Text("Retry"),
            ),
          ],
        );
      },
    );
  }

  // Fetch valid appliances from Firebase RTDB
  void fetchAppliances() {
    _dbDeviceRef.onValue.listen((event) {
      final data = event.snapshot.value;
      if (data == null) {
        print("❌ No data found in Firebase. Check database setup.");
        return;
      }
      print("Fetched data from Firebase: $data");
      if (data is Map) {
        setState(() {
          _validAppliances = {}; // Reset before updating
          data.forEach((key, value) {
            String applianceName = key.replaceAll("_", " "); // Extract device name
            String status = value["status"] ?? "Unknown"; // Extract status

            _validAppliances[applianceName] = {
              "originalKey": key,
              "status": status,
            };
          });
        });
      }

    });
  }
  void testFirebase() async {
    DatabaseReference testRef = FirebaseDatabase.instance.ref();
    DataSnapshot snapshot = await testRef.child("devices").get();

    if (snapshot.exists) {
      print("🔥 Firebase Test Success: ${snapshot.value}");
    } else {
      print("❌ Firebase Test Failed: No data found.");
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Voice Control")),
      body: SingleChildScrollView(
        child: Center(
          child: Column(
            children: [
              Container(
                padding: EdgeInsets.all(16),
                child: Text(
                    _speechToText.isListening
                        ? "Listening..."
                        : _speechEnabled
                        ? "Tap the microphone to start listening..."
                        : "Speech not available",
                  style: TextStyle(fontSize: 18),
                ),
              ),
        
              //Words Spoken Card
              SizedBox(
                height: 200, // Same height as appliance card
                child: Card(
                  margin: EdgeInsets.all(12),
                  elevation: 4,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Padding(
                    padding: EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          "Words Spoken",
                          style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                        ),
                        Divider(),
                        Expanded(
                          child: SingleChildScrollView(
                            child: Text(
                              _wordsSpoken,
                              style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
        
              // Microphone
              Padding(
                padding: EdgeInsets.symmetric(vertical: 10),
                child: ElevatedButton.icon(
                  onPressed: _speechToText.isListening ? _stopListening : _startListening,
                  icon: Icon(
                    _speechToText.isNotListening ? Icons.mic_off : Icons.mic,
                    color: Colors.white,
                  ),
                  label: Text(
                    _speechToText.isListening ? "Stop Listening" : "Start Listening",
                    style: TextStyle(fontSize: 16),
                  ),
                  style: ElevatedButton.styleFrom(
                    padding: EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                    backgroundColor: Colors.blue,
                    foregroundColor: Colors.white,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(8),
                    ),
                  ),
                ),
              ),
        
              //Available Appliances Card
              SizedBox(
                height: 300, // Fixed height for the card
                child: InkWell(
                  onTap: () {
                    showModalBottomSheet(
                      context: context,
                      isScrollControlled: true, // Allows full-screen modal
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
                      ),
                      builder: (context) {
                        // Available appliances Modal
                        return StatefulBuilder(
                          builder: (context, setModalState) {
                            return Padding(
                              padding: const EdgeInsets.all(16),
                              child: Column(
                                mainAxisSize: MainAxisSize.min,
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    "Available Appliances (${_validAppliances.length})",
                                    style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                                  ),
                                  Divider(),
                                  SizedBox(
                                    height: 300, // Give the list a fixed height in modal
                                    child: ListView(
                                      children: _validAppliances.entries.map((entry) {
                                        String appliance = entry.key;
                                        String applianceKey = entry.value["originalKey"]!;
                                        String status = entry.value["status"]!;
                                        return ListTile(
                                          title: Text(appliance),
                                          leading: Icon(
                                            status == "ON" ? Icons.power : Icons.power_off,
                                            color: status == "ON" ? Colors.green : Colors.red,
                                          ),
                                          trailing: Row(
                                            mainAxisSize: MainAxisSize.min, // Prevents excessive space usage
                                            children: [
                                              Text(
                                                status,
                                                style: TextStyle(
                                                  fontWeight: FontWeight.bold,
                                                  color: status == "ON" ? Colors.green : Colors.red,
                                                ),
                                              ),
                                              SizedBox(width: 8), // Adds some spacing
                                              Switch(
                                                value: status == "ON",
                                                onChanged: (bool newState) {
                                                  _toggleAppliance(applianceKey, newState, setModalState);
                                                },
                                              ),
                                            ],
                                          ),
                                        );
                                      }).toList(),
                                    ),
                                  ),
                                ],
                              ),
                            );
                          }
                        );
                      },
                    );
                  },
                  child: Card(
                    margin: EdgeInsets.all(12),
                    elevation: 4,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(10),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            "Available Appliances (${_validAppliances.length})",
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                          ),
                          Divider(),
                          Expanded(
                            child: SingleChildScrollView(
                              child: Column(
                                children: _validAppliances.entries.map((entry) {
                                  String appliance = entry.key;
                                  String status = entry.value["status"]!;
                                  return ListTile(
                                    title: Text(appliance),
                                    leading: Icon(
                                      status == "ON" ? Icons.power : Icons.power_off,
                                      color: status == "ON" ? Colors.green : Colors.red,
                                    ),
                                    trailing: Text(
                                      status,
                                      style: TextStyle(
                                        fontWeight: FontWeight.bold,
                                        color: status == "ON" ? Colors.green : Colors.red,
                                      ),
                                    ),
                                  );
                                }).toList(),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              if(_speechToText.isNotListening && _confidenceLevel >0)
                Padding(
                  padding: const EdgeInsets.only(bottom: 100),
                  child: Text(
                    "Confidence ${(_confidenceLevel * 100).toStringAsFixed(1)}%",
                    style: TextStyle(
                      fontSize: 30,
                      fontWeight: FontWeight.w200,
                    ),
                  ),
                )
            ],
          ),
        ),
      ),
    );
  }

  void _toggleAppliance(String applianceKey, bool newState, Function setModalState) {
    String newStatus = newState ? "ON" : "OFF";

    // Update Firebase database
    _dbDeviceRef.child(applianceKey).set({"status": newStatus}).then((_) {
      // Update local state
      setState(() {
        _validAppliances.forEach((key, value) {
          if (value["originalKey"] == applianceKey) {
            _validAppliances[key]!["status"] = newStatus;
          }
        });
      });

      // Update modal state as well
      setModalState(() {
        _validAppliances.forEach((key, value) {
          if (value["originalKey"] == applianceKey) {
            _validAppliances[key]!["status"] = newStatus;
          }
        });
      });
    }).catchError((error) {
      print("❌ Error updating appliance: $error");
    });
  }
}
