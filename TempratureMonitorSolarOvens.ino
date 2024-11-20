#include <EEPROM.h>
#include <math.h>

// Define pins
const int thermistorPin1 = A0;
const int thermistorPin2 = A1;
const int buttonTogglePin = 2;
const int buttonLoadPin = 3;

// Constants for thermistor calculation
const float BETA = 3950; // Beta parameter of the thermistor
const float R0 = 10000;  // Resistance of the thermistor at 25 degrees C
const float T0 = 298.15; // Temperature in Kelvin for 25 degrees C
const int SERIESRESISTOR = 10000; // Value of the resistor in the voltage divider

// Calibration offsets for temperature readings
float calibrationOffset1 = 80.0;
float calibrationOffset2 = 80.0;

// Constants for energy calculation
const float specificHeatCapacityAir = 1.005; // J/g°C (for air)
const float volumeInches = 10.0; // 10-inch cube
const float volumeLiters = volumeInches * volumeInches * volumeInches * (2.54 * 2.54 * 2.54) / 1000.0;
const float densityAir = 1.225; // g/L
const float massAir = volumeLiters * densityAir; // Mass of air in grams

// Logging control
bool logging = false;
bool buttonState = LOW;
bool lastButtonState = LOW;
unsigned long buttonPressStartTime = 0;
const unsigned long eraseHoldTime = 5000; // Time in milliseconds to hold the button to erase data
bool eraseMode = false; // Flag to indicate if erase mode is active

// Data storage
const int maxReadings = 60; // Maximum number of readings to store
const int eepromStartAddress = 4; // Starting address in EEPROM for storing data
int readingIndex = 0;

// Test duration in minutes
int testDurationMinutes = 120; // Set the test duration (in minutes)
unsigned long readDelay = (testDurationMinutes * 60UL * 1000UL) / maxReadings; // Calculate delay in milliseconds
unsigned long previousMillis = 0;

// Anomaly detection threshold
const int anomalyThreshold = 10; // Maximum allowable change in temperature between successive readings

void setup() {
  Serial.begin(9600);

  // Set up buttons
  pinMode(buttonTogglePin, INPUT_PULLUP);
  pinMode(buttonLoadPin, INPUT_PULLUP);

  // Retrieve the last reading index from EEPROM
  EEPROM.get(0, readingIndex);

  // Calculate delay in minutes and seconds
  unsigned long readDelaySeconds = readDelay / 1000;
  unsigned int readDelayMinutes = readDelaySeconds / 60;
  readDelaySeconds = readDelaySeconds % 60;

  // Initial debug message
  Serial.println("Temperature Logging System Initialized");
  Serial.println("-------------------------------------");
  Serial.println("Press button to toggle logging.");
  Serial.println("Press second button to load data.");
  Serial.println("Hold second button for 5 seconds to erase data.");
  Serial.println("Adjust 'testDurationMinutes' variable to set the test duration in minutes.");
  Serial.println("-------------------------------------");
  Serial.print("Read delay calculated as: ");
  Serial.print(readDelayMinutes);
  Serial.print(" minutes ");
  Serial.print(readDelaySeconds);
  Serial.println(" seconds");
  Serial.print("Starting from reading index: ");
  Serial.println(readingIndex);
  Serial.print("Max readings that can be stored: ");
  Serial.println(maxReadings);
}

void loop() {
  // Read toggle button state
  buttonState = digitalRead(buttonTogglePin);

  // Check if the toggle button was pressed
  if (buttonState == LOW && lastButtonState == HIGH) {
    logging = !logging; // Toggle logging state
    if (logging) {
      Serial.println("Logging started.");
      readingIndex = 0; // Reset index when logging starts
    } else {
      Serial.println("Logging stopped.");
    }
    delay(500); // Debounce delay
  }

  lastButtonState = buttonState;

  // Check if it's time to read the temperature
  unsigned long currentMillis = millis();
  if (logging && (currentMillis - previousMillis >= readDelay)) {
    previousMillis = currentMillis;

    // Read temperatures
    int analogValue1 = analogRead(thermistorPin1);
    int analogValue2 = analogRead(thermistorPin2);

    if (analogValue1 > 0 && analogValue2 > 0) {
      // Convert analog values to resistance
      float resistance1 = SERIESRESISTOR / ((1023.0 / analogValue1) - 1);
      float resistance2 = SERIESRESISTOR / ((1023.0 / analogValue2) - 1);

      // Calculate temperature in Kelvin, then convert to Fahrenheit
      float temperatureK1 = 1 / ((log(resistance1 / R0) / BETA) + (1 / T0));
      float temperatureK2 = 1 / ((log(resistance2 / R0) / BETA) + (1 / T0));

      float temperatureC1 = temperatureK1 - 273.15;
      float temperatureC2 = temperatureK2 - 273.15;

      int temperatureF1 = (int)((temperatureC1 * 9.0 / 5.0) + 32.0 + calibrationOffset1);
      int temperatureF2 = (int)((temperatureC2 * 9.0 / 5.0) + 32.0 + calibrationOffset2);

      // Check for anomalous readings
      if (readingIndex > 0) {
        int prevTemperatureF1, prevTemperatureF2;
        int prevAddress1 = eepromStartAddress + (readingIndex - 1) * 4;
        int prevAddress2 = prevAddress1 + 2;

        EEPROM.get(prevAddress1, prevTemperatureF1);
        EEPROM.get(prevAddress2, prevTemperatureF2);

        if (abs(temperatureF1 - prevTemperatureF1) > anomalyThreshold || abs(temperatureF2 - prevTemperatureF2) > anomalyThreshold) {
          Serial.println("Anomalous reading detected, discarding...");
          return; // Discard this reading
        }
      }

      // Store temperatures in EEPROM
      if (readingIndex < maxReadings) {
        int address1 = eepromStartAddress + readingIndex * 4;
        int address2 = address1 + 2;

        if (address2 + 1 < EEPROM.length()) {
          EEPROM.put(address1, temperatureF1);
          EEPROM.put(address2, temperatureF2);
          readingIndex++;
          EEPROM.put(0, readingIndex); // Update the reading index in EEPROM
          Serial.print("Stored Temperature 1: ");
          Serial.print(temperatureF1);
          Serial.print(", Temperature 2: ");
          Serial.println(temperatureF2);
          Serial.print("Stored at addresses: ");
          Serial.print(address1);
          Serial.print(" and ");
          Serial.println(address2);
        } else {
          Serial.println("EEPROM overflow. Stopping logging.");
          logging = false;
        }
      } else {
        Serial.println("Maximum readings reached. Stopping logging.");
        logging = false;
      }

      // Print current readings to serial monitor
      Serial.print("Current Temperature 1: ");
      Serial.print(temperatureF1);
      Serial.print(" *F, ");

      Serial.print("Current Temperature 2: ");
      Serial.print(temperatureF2);
      Serial.println(" *F");
    } else {
      if (analogValue1 <= 0) {
        Serial.println("Analog value for Thermistor 1 is 0. Check connections.");
      }
      if (analogValue2 <= 0) {
        Serial.println("Analog value for Thermistor 2 is 0. Check connections.");
      }
    }
  }

  // Read load button state
  if (digitalRead(buttonLoadPin) == LOW) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
      eraseMode = false; // Reset erase mode flag
      Serial.println("Load button pressed.");
    }
    // Check if button is held for the erase hold time
    if (millis() - buttonPressStartTime >= eraseHoldTime && !eraseMode) {
      Serial.println("Erasing stored data...");
      for (int i = 0; i < maxReadings * 4; i++) {
        EEPROM.write(eepromStartAddress + i, 0);
      }
      readingIndex = 0;
      EEPROM.put(0, readingIndex); // Reset the reading index in EEPROM
      Serial.println("Data erased.");
      eraseMode = true; // Set erase mode flag
      delay(500); // Debounce delay
    }
  } else {
    if (buttonPressStartTime != 0 && millis() - buttonPressStartTime < eraseHoldTime && !eraseMode) {
      // If button was released before hold time and not in erase mode, load the data
      Serial.println("Loading data...");
      int highestTemp1 = -9999;
      int highestTemp2 = -9999;
      long totalTemp1 = 0;
      long totalTemp2 = 0;
      int firstTemp1 = 0;
      int lastTemp1 = 0;
      int firstTemp2 = 0;
      int lastTemp2 = 0;
      for (int i = 0; i < readingIndex; i++) {
        int address1 = eepromStartAddress + i * 4;
        int address2 = address1 + 2;
        int temperatureF1 = 0;
        int temperatureF2 = 0;

        if (address2 + 1 < EEPROM.length()) {
          EEPROM.get(address1, temperatureF1);
          EEPROM.get(address2, temperatureF2);
        } else {
          Serial.println("Invalid EEPROM address.");
          break;
        }

        Serial.print(temperatureF1);
        Serial.print(",");
        Serial.println(temperatureF2);

        // Update statistics
        if (i == 0) {
          firstTemp1 = temperatureF1;
          firstTemp2 = temperatureF2;
        }
        if (i == readingIndex - 1) {
          lastTemp1 = temperatureF1;
          lastTemp2 = temperatureF2;
        }
        if (temperatureF1 > highestTemp1) highestTemp1 = temperatureF1;
        if (temperatureF2 > highestTemp2) highestTemp2 = temperatureF2;
        totalTemp1 += temperatureF1;
        totalTemp2 += temperatureF2;
      }
      if (readingIndex > 0) {
        int avgTemp1 = totalTemp1 / readingIndex;
        int avgTemp2 = totalTemp2 / readingIndex;

        // Ensure temperature differences are positive for energy calculation
        float tempDiff1 = abs(lastTemp1 - firstTemp1);
        float tempDiff2 = abs(lastTemp2 - firstTemp2);

        // Calculate total energy collected (simplified)
        float totalEnergy1 = specificHeatCapacityAir * massAir * tempDiff1;
        float totalEnergy2 = specificHeatCapacityAir * massAir * tempDiff2;

        // Display statistics
        Serial.println("Data loading complete.");
        Serial.print("Highest Temperature 1: ");
        Serial.println(highestTemp1);
        Serial.print("Average Temperature 1: ");
        Serial.println(avgTemp1);
        Serial.print("Measurements Count 1: ");
        Serial.println(readingIndex);
        Serial.print("Total Energy Collected 1: ");
        Serial.print(totalEnergy1);
        Serial.println(" J");

        Serial.print("Highest Temperature 2: ");
        Serial.println(highestTemp2);
        Serial.print("Average Temperature 2: ");
        Serial.println(avgTemp2);
        Serial.print("Measurements Count 2: ");
        Serial.println(readingIndex);
        Serial.print("Total Energy Collected 2: ");
        Serial.print(totalEnergy2);
        Serial.println(" J");
      } else {
        Serial.println("No data to load.");
      }

      delay(500); // Debounce delay
    }
    buttonPressStartTime = 0; // Reset button press start time
  }
}
