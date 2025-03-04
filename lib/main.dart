import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'package:voice_assistant/speech_to_text.dart';
import 'package:voice_assistant/voice_control_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp();

  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Voice Assistant',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: const VoiceAssistant(),
    );
  }
}