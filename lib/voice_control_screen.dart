import 'package:flutter/material.dart';
import 'package:speech_to_text/speech_to_text.dart' as stt;
import 'package:firebase_database/firebase_database.dart';

class VoiceControlScreen extends StatefulWidget {
  const VoiceControlScreen({super.key});
  @override
  _VoiceControlScreenState createState() => _VoiceControlScreenState();
}

class _VoiceControlScreenState extends State<VoiceControlScreen> {
  final stt.SpeechToText _speech = stt.SpeechToText();
  bool _isListening = false;
  String _command = "Say 'Turn on light' or 'Turn off light'";

  final DatabaseReference _dbRef =
      FirebaseDatabase.instance.ref().child("lightStatus");

  void _listen() async {
    if (!_isListening) {
      bool available = await _speech.initialize(
        onStatus: (val) => print("Status: $val"),
        onError: (val) => print("Error: $val"),
      );
      if (available) {
        setState(() {
          _isListening = true;
        });
        _speech.listen(
          onResult: (val) {
            setState(() {
              _command = val.recognizedWords;
            });
            if (_command.toLowerCase().contains("turn on light")) {
              _dbRef.set("ON"); // Update Firebase RTDB
            } else if (_command.toLowerCase().contains("turn off light")) {
              _dbRef.set("OFF"); // Update Firebase RTDB
            }
          },
        );
      }
    } else {
      setState(() {
        _isListening = false;
      });
      _speech.stop();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Voice Control")),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(_command, style: TextStyle(fontSize: 20)),
            SizedBox(height: 20),
            FloatingActionButton(
              onPressed: _listen,
              child: Icon(_isListening ? Icons.mic : Icons.mic_none),
            ),
          ],
        ),
      ),
    );
  }
}