/*
  Project: Arduino Nano-based CMRI SMINI Node (48 outputs & 24 inputs)
  Author: Thomas Seitz (thomas.seitz@tmrci.org)
  Version: 1.0.2
  Date: 2023-05-17
  Description: A sketch for an Arduino Nano-based CMRI SMINI Node board using RS-485
*/

// Import required libraries
#include <Auto485.h>                                // Library for RS485 communication
#include <CMRI.h>                                   // Library for CMRI communication
#include <SPI.h>                                    // Library for SPI communication

const byte CMRI_ADDR = 4;                           // Define CMRI node address
const byte DE_PIN = 2;                              // Define RS485 DE and RE pins on Arduino
#define NOP __asm__ __volatile__("nop")             // "nop" assembly instruction macro

// Define pins for 74HC165
const byte LATCH_165 = 9;

// Define pins for 74HC595
const byte LATCH_595 = 6;
const byte DATA_595 = 7;
const byte CLOCK_595 = 8;

Auto485 bus(DE_PIN);                                // Initialize RS485 bus transceiver
CMRI cmri(CMRI_ADDR, 24, 48, bus);                  // sets up an SMINI. SMINI = 24 inputs, 48 outputs

byte last_input_state[3];                           // Define variable to store the previous state of inputs
byte last_output_state[6];                          // Define variable to store the previous state of outputs

void setup() {
  // Open the RS485 bus at 19200bps
  bus.begin(19200, SERIAL_8N2);

  // 74HC165 setup
  pinMode(LATCH_165, OUTPUT);
  digitalWrite(LATCH_165, HIGH);                    // Initialize Latch High
  SPI.begin ();                                     // Start SPI communication to control 74HC165

  // 74HC595 setup
  pinMode(LATCH_595, OUTPUT);
  pinMode(DATA_595, OUTPUT);
  pinMode(CLOCK_595, OUTPUT);

  // Initialize last_input_state array with current input state
  digitalWrite(LATCH_165, LOW);                     // Pulse the parallel load latch
  NOP;                                              // Wait while data loads
  NOP;
  digitalWrite(LATCH_165, HIGH);
  last_input_state[0] = ~(SPI.transfer(0));
  last_input_state[1] = ~(SPI.transfer(0));
  last_input_state[2] = ~(SPI.transfer(0));
}

void loop() {
  // Step 1: Process incoming messages on the CMRI bus
  cmri.process();

  // Step 2: Update the output shift registers if the output state has changed
  byte currentOutputState[6];
  for (int i = 0; i < 6; i++) {
    currentOutputState[i] = cmri.get_byte(i);
  }

  if (memcmp(last_output_state, currentOutputState, 6) != 0) {
    memcpy(last_output_state, currentOutputState, 6);

    digitalWrite(LATCH_595, LOW);                     // Start by setting Latch Low
    for (int i = 5; i >= 0; i--) {
      shiftOut(DATA_595, CLOCK_595, MSBFIRST, ~currentOutputState[i]); // Send the ith byte
    }
    digitalWrite(LATCH_595, HIGH);                    // Set the Latch High to update the Data
  }

  // Step 3: Read the current state of the input pins
  digitalWrite(LATCH_165, LOW);                     // pulse the parallel load latch
  NOP;
  NOP;
  digitalWrite(LATCH_165, HIGH);
  byte currentInputState[3];
  currentInputState[0] = ~(SPI.transfer(0));
  currentInputState[1] = ~(SPI.transfer(0));
  currentInputState[2] = ~(SPI.transfer(0));

  // Step 4: If the input state has changed, update the CMRI input bytes
  if (memcmp(currentInputState, last_input_state, 3) != 0) {
    memcpy(last_input_state, currentInputState, 3);
    for (int i = 0; i < 3; i++) {
      cmri.set_byte(i, currentInputState[i]);
    }
  }
}
