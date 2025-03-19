/****************************************************************************** 
HackerBot Industries, LLC
Created By: Ian Bernstein
Created:    April 2024
Updated:    2025.03.18

This sketch is written for the "Dynamixel Controller" PCBA and moves the head
around in random but natural looking patterns.

Special thanks to the following for their code contributions to this codebase:
Randy  - https://github.com/rbeiter
*******************************************************************************/

#include <Dynamixel2Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <SerialCmd.h>
#include <Wire.h>
#include "Hackerbot_Shared.h"
#include "SerialCmd_Helper.h"

// Dynamixel Controller software version
#define VERSION_NUMBER 4

// Set up variables and constants for dynamixel control
#define DXL_SERIAL   Serial1

const int DXL_DIR_PIN = 2;
const uint8_t DXL_TURN_ID = 1;
const uint8_t DXL_VERT_ID = 2;
const float DXL_PROTOCOL_VERSION = 2.0;

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
using namespace ControlTableItem;

// Timing variables
unsigned long startMillis;
unsigned long currentMillis;
unsigned long startTimeoutMillis;
const unsigned long timeoutMillis = 600000;

// Modes and movement variables
int idle = 1;
int newMovement;
float position_yaw;
float position_pitch;
int position_delay;
int position_speed;

// Other defines and variables
byte I2CRxArray[16];
byte I2CTxArray[16];
byte cmd = 0;

// Set up the onboard neopixel
Adafruit_NeoPixel onboard_pixel(1, PIN_NEOPIXEL);

// Set up the serial command processor
SerialCmdHelper mySerCmd(Serial);
int8_t ret;

// I2C Rx Handler
void I2C_RxHandler(int numBytes) {
  String query = String();
  char CharArray[32];

  Serial.print("INFO: I2C Byte Received... ");
  for (int i = 0; i < numBytes; i++) {
    I2CRxArray[i] = Wire.read();
    Serial.print("0x");
    Serial.print(I2CRxArray[i], HEX);
    Serial.print(" ");
  }

  Serial.println();

  // Parse incoming commands
  switch (I2CRxArray[0]) {
    case I2C_COMMAND_PING: // Ping
      cmd = I2C_COMMAND_PING;
      I2CTxArray[0] = 0x01;
      break;
    case I2C_COMMAND_VERSION: // Version
      cmd = I2C_COMMAND_VERSION;
      I2CTxArray[0] = VERSION_NUMBER;
      break;
    case I2C_COMMAND_H_IDLE: // Set_IDLE Command - Params(0 = off, 1 = on)
      Serial.println("INFO: Set_IDLE command received");
      if (I2CRxArray[1] == 0x00) {
        ret = mySerCmd.ReadString((char *) "IDLE,0");
      } else {
        ret = mySerCmd.ReadString((char *) "IDLE,1");
      }
      break;
    case I2C_COMMAND_H_LOOK: // Set_LOOK Command - Params(yaw h, yaw l, pitch h, pitch l, speed)
      Serial.println("INFO: Set_LOOK command received");

      query = "LOOK," + (String)(((I2CRxArray[1] << 8) + I2CRxArray[2]) * 0.1) + "," + (String)(((I2CRxArray[3] << 8) + I2CRxArray[4]) * 0.1) + "," + (String)(I2CRxArray[5]);
      
      // Convert the query string to a char array
      query.toCharArray(CharArray, query.length() + 1);
      Serial.println(CharArray);
      ret = mySerCmd.ReadString(CharArray);
      break;
  }
}

// I2C Tx Handler
void I2C_TxHandler(void) {
  switch (cmd) {
    case I2C_COMMAND_PING: // Ping
      Wire.write(I2CTxArray[0]);
      break;
    case I2C_COMMAND_VERSION: // Version
      Wire.write(I2CTxArray[0]);
      break;
  }
}


// -------------------------------------------------------
// User Functions
// -------------------------------------------------------
void sendOK(void) {
  mySerCmd.Print((char *) "INFO: OK\r\n");
}


// -------------------------------------------------------
// Functions for SerialCmd
// -------------------------------------------------------
void send_PING(void) {
  sendOK();
}


// Reports the current fw version
// Example - "VERSION"
void Get_Version(void) {
  mySerCmd.Print((char *) "INFO: Dynamixel Controller Firmware (v");
  mySerCmd.Print(VERSION_NUMBER);
  mySerCmd.Print((char *) ".0)\r\n");

  sendOK();
}


void set_IDLE(void) {
  uint8_t idleParam = 0;

  if (!mySerCmd.ReadNextUInt8(&idleParam)) {
    mySerCmd.Print((char *) "ERROR: Missing parameter\r\n");
    return;
  }

  // Constrain values to acceptable range
  idleParam = constrain(idleParam, 0, 1);

  if (idleParam == 0) {
    mySerCmd.Print((char *) "INFO: Idle mode off\r\n" );
    idle = 0;
    onboard_pixel.setPixelColor(0, onboard_pixel.Color(10, 10, 0));
    onboard_pixel.show();
  } else {
    mySerCmd.Print((char *) "INFO: Idle mode on\r\n" );
    idle = 1;
    onboard_pixel.setPixelColor(0, onboard_pixel.Color(0, 10, 0));
    onboard_pixel.show();
    startTimeoutMillis = millis();
  }

  sendOK();
}


// Sets the position of the Hackerbot head's neck
// Parameters
// float: yaw (rotation angle between 100.0 and 260.0 degrees - 180.0 is looking straight ahead)
// float: pitch (vertical angle between 150.0 and 250.0 degrees - 180.0 is looking straight ahead)
// Example - "LOOK,180.0,180.0"
void set_LOOK(void) {
  float turnParam = 0.0;
  float vertParam = 0.0;
  uint8_t speedParam = 0;

  if (!mySerCmd.ReadNextFloat(&turnParam) || !mySerCmd.ReadNextFloat(&vertParam) || !mySerCmd.ReadNextUInt8(&speedParam)) {
    mySerCmd.Print((char *) "ERROR: Missing parameter\r\n");
    return;
  }

  ret = mySerCmd.ReadString((char *) "IDLE,0");

  // Constrain values to acceptable range
  position_yaw = constrain(turnParam, 100.0, 260.0);
  position_pitch = constrain(vertParam, 150.0, 250.0);
  position_speed = constrain(speedParam, 6, 70);

  char buf[128] = {0};
  sprintf(buf, "STATUS: Looking to position turn: %0.2f, vert: %0.2f, at speed: %d\r\n", position_yaw, position_pitch, position_speed);
  mySerCmd.Print(buf);

  newMovement = 1;

  sendOK();
}


// -------------------------------------------------------
// setup()
// -------------------------------------------------------
void setup() {
  unsigned long serialTimout = millis();

  Serial.begin(115200);
  while(!Serial && millis() - serialTimout <= 5000);

  startTimeoutMillis = millis();

  // Define serial commands
  mySerCmd.AddCmd("PING", SERIALCMD_FROMALL, send_PING);
  mySerCmd.AddCmd("VERSION", SERIALCMD_FROMALL, Get_Version);
  mySerCmd.AddCmd("H_IDLE", SERIALCMD_FROMALL, set_IDLE);
  mySerCmd.AddCmd("H_LOOK", SERIALCMD_FROMALL, set_LOOK);

  // Set up the dynamixel serial port
  dxl.begin(57600);
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);

  // Initialize I2C (Slave Mode: address=0x5B)
  Wire.begin(DYN_I2C_ADDRESS);
  Wire.onReceive(I2C_RxHandler);
  Wire.onRequest(I2C_TxHandler);

  // Configure the two neck motors
  dxl.ping(DXL_TURN_ID);
  dxl.ping(DXL_VERT_ID);

  dxl.torqueOff(DXL_TURN_ID);
  dxl.setOperatingMode(DXL_TURN_ID, OP_POSITION);
  dxl.torqueOn(DXL_TURN_ID);
  
  dxl.torqueOff(DXL_VERT_ID);
  dxl.setOperatingMode(DXL_VERT_ID, OP_POSITION);
  dxl.torqueOn(DXL_VERT_ID);

  dxl.writeControlTableItem(PROFILE_ACCELERATION, DXL_TURN_ID, 10);
  dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_TURN_ID, 40);

  dxl.writeControlTableItem(PROFILE_ACCELERATION, DXL_VERT_ID, 10);
  dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_VERT_ID, 40);

  onboard_pixel.begin();
  onboard_pixel.setPixelColor(0, onboard_pixel.Color(0, 10, 0));
  onboard_pixel.show();

  Serial.println("INFO: Starting application...");
}


// ----------------------- loop() ------------------------
void loop() {
  //int8_t ret;

  currentMillis = millis();
  
  if (currentMillis - startTimeoutMillis >= timeoutMillis) {
    ret = mySerCmd.ReadString((char *) "IDLE,0");
    startTimeoutMillis = millis();
  }

  if (idle == 1) {
    if (currentMillis - startMillis >= position_delay) {
      position_yaw = random(150.0, 210.0);
      position_pitch = random(175.0, 230.0);
      position_delay = random(500, 20000);
      position_speed = random(5, 60);

      // Set the movement speed
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_TURN_ID, position_speed);
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_VERT_ID, position_speed);

      // Set the goal positions in degrees
      dxl.setGoalPosition(DXL_TURN_ID, position_yaw, UNIT_DEGREE);
      dxl.setGoalPosition(DXL_VERT_ID, position_pitch, UNIT_DEGREE);

      // Wait for the dynamixels to finish their motions before proceeding
      //while(dxl.readControlTableItem(MOVING, DXL_TURN_ID));
      //while(dxl.readControlTableItem(MOVING, DXL_VERT_ID));

      startMillis = currentMillis;

    // Wait for the dynamixels to finish their motions before starting the timer between motions
    } else if (dxl.readControlTableItem(MOVING, DXL_TURN_ID) || dxl.readControlTableItem(MOVING, DXL_VERT_ID)) {
      startMillis = currentMillis;
    }
  } else {
    if (newMovement == 1) {
      // Set the movement speed
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_TURN_ID, position_speed);
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_VERT_ID, position_speed);

      // Set the goal positions in degrees
      dxl.setGoalPosition(DXL_TURN_ID, position_yaw, UNIT_DEGREE);
      dxl.setGoalPosition(DXL_VERT_ID, position_pitch, UNIT_DEGREE);

      newMovement = 0;
    }
  }

  ret = mySerCmd.ReadSer();
  if (ret == 0) {
    mySerCmd.Print((char *) "ERROR: Urecognized command\r\n");
  }
}
