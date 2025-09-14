#include <Servo.h>
#include <FastLED.h>

// LED strip configuration
#define LED_PIN 7
#define NUM_LEDS 30
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Create servo objects
Servo servo1, servo2, servo3, servo4, servo5, servo6;
Servo servos[6] = {servo1, servo2, servo3, servo4, servo5, servo6};

// Pin definitions
const int trigPin = 12;
const int echoPin = 13;
const int servoPins[6] = {3, 5, 6, 9, 10, 11}; // PWM pins for servos

// Constants
const int RESTING_ANGLE = 90;
const int MIN_WAVE_ANGLE = 30;  // Changed from 0 to 30
const int MAX_WAVE_ANGLE = 45;  // Changed from 15 to 45
const float DISTANCE_THRESHOLD = 40.0;
const float PHASE_OFFSET = PI / 3; // 60 degrees offset between servos
const float WAVE_SPEED = 0.05; // Controls how fast the wave moves

// Rest mode servo wave constants
const int REST_MIN_ANGLE = 90;
const int REST_MAX_ANGLE = 105;
const float REST_WAVE_SPEED = 0.02; // Slower wave speed for rest mode

// Variables
float waveTime = 0;
bool waveMode = false;
unsigned long lastDistanceCheck = 0;
const unsigned long DISTANCE_CHECK_INTERVAL = 50; // Check distance every 50ms
unsigned long waveStartTime = 0; // Track when wave mode started
const unsigned long WAVE_DURATION = 10000; // 10 seconds in milliseconds

// Rest mode servo wave variables
float restWaveTime = 0;

// LED variables
float ledWaveTime = 0;
const float LED_WAVE_SPEED = 0.01; // Much slower
unsigned long lastLEDUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 500; // Update LEDs every 500ms

void setup() {
  Serial.begin(9600);
  
  // Initialize LED strip
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(150); // Set overall brightness (0-255)
  FastLED.clear();
  FastLED.show();
  
  // Initialize ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Attach servos to pins and set to resting position
  for (int i = 0; i < 6; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(RESTING_ANGLE);
    delay(100); // Small delay to let servo reach position
  }
  
  Serial.println("Servo Wave Controller Initialized");
  Serial.println("Resting position: 90 degrees");
  Serial.println("Wave range: 30-45 degrees");
  Serial.println("Rest wave range: 90-105 degrees");
  Serial.println("Distance threshold: 40cm");
  Serial.println("LED Strip: 30 WS2812B LEDs on pin 7");
}

void loop() {
  // Check distance periodically ONLY when not in wave mode
  if (!waveMode && millis() - lastDistanceCheck >= DISTANCE_CHECK_INTERVAL) {
    float distance = getDistance();
    
    // Determine if we should be in wave mode
    bool shouldWave = (distance < DISTANCE_THRESHOLD && distance > 0);
    
    if (shouldWave) {
      // Entering wave mode - move all servos to starting position
      Serial.println("Object detected! Starting 10-second wave pattern...");
      waveMode = true;
      waveTime = 0;
      waveStartTime = millis(); // Record when wave mode started
      Serial.print("Wave start time: ");
      Serial.println(waveStartTime);
      moveAllServosTo(MIN_WAVE_ANGLE, 500); // Move to 30 degrees over 500ms
    }
    
    lastDistanceCheck = millis();
  }
  
  // Check if wave mode should end (after 10 seconds)
  if (waveMode) {
    unsigned long elapsedTime = millis() - waveStartTime;
    
    if (elapsedTime >= WAVE_DURATION) {
      // Exiting wave mode - return to resting position
      Serial.println("10 seconds completed. Returning to rest position...");
      Serial.print("Elapsed time: ");
      Serial.println(elapsedTime);
      moveAllServosTo(RESTING_ANGLE, 800); // Move to 90 degrees over 800ms
      waveMode = false;
    } else {
      // Execute wave pattern if in wave mode
      updateWavePattern();
      waveTime += WAVE_SPEED;
      
      // Print remaining time every 2 seconds for debugging
      if (elapsedTime % 2000 < 100) {
        Serial.print("Wave mode - remaining time: ");
        Serial.print((WAVE_DURATION - elapsedTime) / 1000);
        Serial.println(" seconds");
      }
    }
  }
  
  // Execute rest wave pattern when not in wave mode
  if (!waveMode) {
    updateRestWavePattern();
    restWaveTime += REST_WAVE_SPEED;
  }
  
  // Update LED pattern when distance > 40cm (only show LEDs when no object detected)
  static bool ledsSetToRed = false; // Track if LEDs have been set to red
  static bool ledsSetToBlue = false; // Track if LEDs have been set to blue
  
  if (!waveMode && !ledsSetToBlue) {
    // Set LEDs to solid blue ONCE when in rest mode
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, 255)); // Solid blue color
    FastLED.show();
    ledsSetToBlue = true;
    ledsSetToRed = false; // Reset red flag
    Serial.println("LEDs set to BLUE");
  } else if (waveMode && !ledsSetToRed) {
    // Set LEDs to red ONCE when object detected
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0)); // Red color
    FastLED.show();
    ledsSetToRed = true;
    ledsSetToBlue = false; // Reset blue flag
    Serial.println("LEDs set to RED");
  }
  
  delay(20); // Small delay for smooth operation
}

float getDistance() {
  // Send ultrasonic pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echo
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  
  if (duration == 0) {
    return -1; // No echo received
  }
  
  // Calculate distance in cm
  float distance = duration * 0.034 / 2;
  
  // Print distance readings
  Serial.print("Distance: ");
  if (distance > 0 && distance < 400) { // Valid range for HC-SR04
    Serial.print(distance);
    Serial.print(" cm");
    if (distance < DISTANCE_THRESHOLD) {
      Serial.println(" - OBJECT DETECTED!");
    } else {
      Serial.println(" - Clear");
    }
  } else {
    Serial.println("Out of range or error");
  }
  
  return distance;
}

void updateWavePattern() {
  for (int i = 0; i < 6; i++) {
    // Calculate the sine wave value for this servo
    // Each servo has a phase offset to create the wave effect
    float sineValue = sin(waveTime + (i * PHASE_OFFSET));
    
    // Map sine value (-1 to 1) to angle range (30 to 45 degrees)
    float angle = MIN_WAVE_ANGLE + ((sineValue + 1) / 2) * (MAX_WAVE_ANGLE - MIN_WAVE_ANGLE);
    
    // Apply the angle to the servo
    servos[i].write((int)angle);
  }
}

void updateRestWavePattern() {
  for (int i = 0; i < 6; i++) {
    // Calculate the sine wave value for this servo in rest mode
    // Each servo has a phase offset to create the wave effect
    float sineValue = sin(restWaveTime + (i * PHASE_OFFSET));
    
    // Map sine value (-1 to 1) to angle range (80 to 95 degrees)
    float angle = REST_MIN_ANGLE + ((sineValue + 1) / 2) * (REST_MAX_ANGLE - REST_MIN_ANGLE);
    
    // Apply the angle to the servo
    servos[i].write((int)angle);
  }
}

void updateLEDPattern() {
  // Very simple blue fade - minimal processing
  uint8_t brightness = 100 + (sin(ledWaveTime) * 50); // Range 50-150
  
  // Set all LEDs to same blue color - no loop needed
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, brightness));
  
  FastLED.show();
}

void moveAllServosTo(int targetAngle, int duration) {
  // Get current positions
  int startAngles[6];
  for (int i = 0; i < 6; i++) {
    startAngles[i] = servos[i].read();
  }
  
  // Move smoothly to target over specified duration
  int steps = duration / 20; // 20ms per step
  for (int step = 0; step <= steps; step++) {
    float progress = (float)step / steps;
    
    for (int i = 0; i < 6; i++) {
      int currentAngle = startAngles[i] + (targetAngle - startAngles[i]) * progress;
      servos[i].write(currentAngle);
    }
    
    delay(20);
  }
  
  // Ensure final position is exact
  for (int i = 0; i < 6; i++) {
    servos[i].write(targetAngle);
  }
}