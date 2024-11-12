/****************************************************************************** 
HackerBot Industries, LLC
Ian Bernstein
April 2024

This sketch is written for the "Dynamixel Controller" PCB and moves the head
around in random but natural looking patterns.

TODO - Add I2C Slave code so other parts of hackerbot can send commands to look
in a specified directions. Also, commands to enable/diable idle mode. 
*********************************************************************************/

#include <Dynamixel2Arduino.h>
#include <Wire.h>

// Set up variables and constants for dynamixel control
#define DXL_SERIAL   Serial1

const int DXL_DIR_PIN = 2;
const uint8_t DXL_TURN_ID = 1;
const uint8_t DXL_VERT_ID = 2;
const float DXL_PROTOCOL_VERSION = 2.0;

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
using namespace ControlTableItem;

// Set up other valiables and constants
#define DEBUG_SERIAL Serial

unsigned long startMillis;
unsigned long currentMillis;
byte RxArray[16];
int idle = 1;
int newMovement;
float position_turn;
float position_vert;
int position_delay;
int position_speed;


// I2C Rx Handler
void I2C_RxHandler(int numBytes) {
  DEBUG_SERIAL.print("I2C Byte Received... ");
  for (int i = 0; i < numBytes; i++) {
    RxArray[i] = Wire.read();
    DEBUG_SERIAL.print("0x");
    DEBUG_SERIAL.print(RxArray[i], HEX);
    DEBUG_SERIAL.print(" ");
  }

  Serial.println();

  // Parse incoming commands
  switch (RxArray[0]) {
    case 0x00: // Set_IDLE Command - Params(0 = Off, 1 = On)
      Serial.println("Set_IDLE command received");
      if (RxArray[1] == 0x00) {
        idle = 0;
      } else {
        idle = 1;
      }
      break;
    case 0x03: // Set_Direction Command - Params(rotation h, rotation l, pitch h, pitch l, speed)
      Serial.println("Set_Direction command received");
      position_turn = ((RxArray[1] << 8) + RxArray[2]) * 0.1;
      position_vert = ((RxArray[3] << 8) + RxArray[4]) * 0.1;
      position_speed = RxArray[5];
      newMovement = 1;
      break;
  }

  /*for (int i = 0; i < numBytes; i++) { DEBUG_SERIAL.print(RxArray[i]); }
    DEBUG_SERIAL.println();
  */
}


// ----------------------- setup() -----------------------
void setup() {
  DEBUG_SERIAL.begin(115200);

  dxl.begin(57600);
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);

  Wire.begin(4); // Initialize I2C (Slave Mode: address=0x5A)
  Wire.onReceive(I2C_RxHandler);

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

  DEBUG_SERIAL.print("INFO: Starting application...");
}


// ----------------------- loop() ------------------------
void loop() {
  currentMillis = millis();
  //if (currentMillis - startMillis >= period) {
    // Periodic items here
  //}

  if (idle == 1) {
    if (currentMillis - startMillis >= position_delay) {
      position_turn = random(150.0, 210.0);
      position_vert = random(175.0, 230.0);
      position_delay = random(500, 20000);
      position_speed = random(5, 60);

      // Set the movement speed
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_TURN_ID, position_speed);
      dxl.writeControlTableItem(PROFILE_VELOCITY, DXL_VERT_ID, position_speed);

      // Set the goal positions in degrees
      dxl.setGoalPosition(DXL_TURN_ID, position_turn, UNIT_DEGREE);
      dxl.setGoalPosition(DXL_VERT_ID, position_vert, UNIT_DEGREE);

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
      dxl.setGoalPosition(DXL_TURN_ID, position_turn, UNIT_DEGREE);
      dxl.setGoalPosition(DXL_VERT_ID, position_vert, UNIT_DEGREE);

      newMovement = 0;
    }
  }
}
