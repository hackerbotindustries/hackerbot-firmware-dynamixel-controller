/****************************************************************************** 
HackerBot Industries, LLC
Ian Bernstein
April 2024
Updated: 2025.01.07

This sketch is written for the "Dynamixel Controller" PCB and moves the head
around in random but natural looking patterns.

TODO - Add I2C Slave code so other parts of hackerbot can send commands to look
in a specified directions. Also, commands to enable/diable idle mode. 
*********************************************************************************/

#include <Dynamixel2Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <SerialCmd.h>
#include <Wire.h>

// Dynamixel Controller software version
#define VERSION_NUMBER 2

// I2C address (0x5B)
#define I2C_ADDRESS 91

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
SerialCmd mySerCmd(Serial);
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
    case 0x01: // Ping
      cmd = 0x01;
      I2CTxArray[0] = 0x01;
      break;
    case 0x02: // Version
      cmd = 0x02;
      I2CTxArray[0] = VERSION_NUMBER;
      break;
    case 0x08: // Set_IDLE Command - Params(0 = off, 1 = on)
      Serial.println("INFO: Set_IDLE command received");
      if (I2CRxArray[1] == 0x00) {
        ret = mySerCmd.ReadString((char *) "IDLE,0");
      } else {
        ret = mySerCmd.ReadString((char *) "IDLE,1");
      }
      break;
    case 0x09: // Set_LOOK Command - Params(yaw h, yaw l, pitch h, pitch l, speed)
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
    case 0x01: // Ping
      Wire.write(I2CTxArray[0]);
      break;
    case 0x02: // Version
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

void set_IDLE(void) {
  char * sParam;
  sParam = mySerCmd.ReadNext();
  if (sParam == NULL) {
    mySerCmd.Print((char *) "ERROR: Missing idle parameter\r\n" );
    return;
  }

  if (strtoul(sParam, NULL, 10) == 0) {
    mySerCmd.Print((char *) "INFO: Idle mode off\r\n" );
    idle = 0;
    onboard_pixel.setPixelColor(0, onboard_pixel.Color(0, 10, 0));
    onboard_pixel.show();
  } else {
    mySerCmd.Print((char *) "INFO: Idle mode on\r\n" );
    idle = 1;
    onboard_pixel.setPixelColor(0, onboard_pixel.Color(0, 0, 10));
    onboard_pixel.show();
    startTimeoutMillis = millis();
  }

  sendOK();
}

void set_LOOK(void) {
  float turnParam = atof(mySerCmd.ReadNext());
  float vertParam = atof(mySerCmd.ReadNext());
  float speedParam = atof(mySerCmd.ReadNext());

  if ((turnParam == NULL) || (vertParam == NULL) || (speedParam == NULL)) {
    mySerCmd.Print((char *) "ERROR: Missing parameter\r\n");
    return;
  }

  ret = mySerCmd.ReadString((char *) "IDLE,0");

  if (turnParam < 100.0) {
    position_yaw = 100.0;
  } else if (turnParam > 260.0) {
    position_yaw = 260.0;
  } else {
    position_yaw = turnParam;
  }

  if (vertParam < 150.0) {
    position_pitch = 150.0;
  } else if (vertParam > 250.0) {
    position_pitch = 250.0;
  } else {
    position_pitch = vertParam;
  }

  if (speedParam < 6) {
    position_speed = 6;
  } else if (speedParam > 70) {
    position_speed = 70;
  } else {
    position_speed = round(speedParam);
  }

  mySerCmd.Print((char *) "STATUS: Looking to position turn: ");
  mySerCmd.Print(position_yaw);
  mySerCmd.Print((char *) ", vert: ");
  mySerCmd.Print(position_pitch);
  mySerCmd.Print((char *) ", at speed: ");
  mySerCmd.Print(position_speed);
  mySerCmd.Print((char *) "\r\n");

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
  mySerCmd.AddCmd("IDLE", SERIALCMD_FROMALL, set_IDLE);
  mySerCmd.AddCmd("LOOK", SERIALCMD_FROMALL, set_LOOK);

  // Set up the dynamixel serial port
  dxl.begin(57600);
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);

  // Initialize I2C (Slave Mode: address=0x5B)
  Wire.begin(I2C_ADDRESS);
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
  onboard_pixel.setPixelColor(0, onboard_pixel.Color(0, 0, 10));
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
