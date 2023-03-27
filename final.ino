### Sources for writing this script
### https://github.com/NMSTEC/Volvo_EPS_FREE/blob/main/src/main.cpp
### https://gist.github.com/mck1117/626174d991183cc72802074f954db97b/
### https://github.com/rusefi/rusefi/blob/master/firmware/controllers/lua/examples/Volvo-electric-power-steering-pump.txt

#include <SPI.h>
#include <mcp2515.h>

// Pin number on Arduino Uno for CS connection on mcp2515 board
MCP2515 mcp2515(10);

// initializing frames
struct can_frame fromPump;
struct can_frame keepAlive;
struct can_frame pumpSpeed;

int start = 0;
int keepAliveCycle = 0; 
int speedModifier = 0; // 0 is fastest, 6000 is slowest
int keepAliveModifier = 0;
int pumpRunningCheck = 100;
int potValue = 0;

unsigned char keepAliveOptions[4] = {0x00, 0x40, 0x80, 0xC0};

void define_keep_alive(){
  keepAlive.can_id  = 0x1ae0092c | CAN_EFF_FLAG;
  keepAlive.can_dlc = 8; // data length code
  keepAlive.data[0] = 0x00;
  keepAlive.data[1] = 0x00;
  keepAlive.data[2] = 0x22;
  keepAlive.data[3] = 0xe0;
  keepAlive.data[4] = 0x41;
  keepAlive.data[5] = 0x90;
  keepAlive.data[6] = 0x00;
  keepAlive.data[7] = 0x00;
}

void define_pump_speed(){
  pumpSpeed.can_id = 0x02104136 | CAN_EFF_FLAG;
  pumpSpeed.can_dlc = 8;
  pumpSpeed.data[0] = 0xbb;
  pumpSpeed.data[1] = 0x00;
  pumpSpeed.data[2] = 0x3f;
  pumpSpeed.data[3] = 0xff;
  pumpSpeed.data[4] = 0x06;
  pumpSpeed.data[5] = 0xe0;
  pumpSpeed.data[6] = 0x00; // these last two
  pumpSpeed.data[7] = 0x00; // affect pump speed
}

void messageSender(){
  Serial.println("Made it to message sender");
  // we only send 1 keepAlive for every 30 pumpSpeed messages
  if (keepAliveCycle == 0){
    keepAlive.data[0] = keepAliveOptions[keepAliveModifier];
    // keepAliveModifier management
    keepAliveModifier++;
    if (keepAliveModifier == 4)
    {
      keepAliveModifier = 0;
    }
    // actually send our keepAlive signal
    mcp2515.sendMessage(&keepAlive);
    // reset keepAliveCycle to 30 to stagger sending these messages
    keepAliveCycle = 30;
  }

  potValue = analogRead(A0); // 0-1023
  speedModifier = potvalue * 5.859375
  
  if speedModifier < 500 {
    speedModifier = 0; 
  }

  // prepare to send pumpSpeed signal
  pumpSpeed.data[6] = (speedModifier >> 8) & 0xFF;
  pumpSpeed.data[7] = speedModifier & 0xFF;

  // send speed message
  mcp2515.sendMessage(&pumpSpeed); 

  keepAliveCycle--;
  delay(14);
}

void setup() {
  // setup Serial connection
  while(!Serial);
  Serial.begin(115200);

  mcp2515.reset();
  // 500KBPS is typical for automotive CANBUS and MCP_8MHZ is dependent on the crystal on your CANBUS board chip
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  // initialize the CANBUS messages we want to send the pump
  define_keep_alive();
  define_pump_speed();

  Serial.println("Setup complete");
}

void loop() {
  // We are waiting to receive a signal from the pump to start sending it our commands
  if (mcp2515.readMessage(&fromPump) == MCP2515::ERROR_OK && start == 0) {
    if (fromPump.can_id == 0x1B200002) {
        Serial.println(fromPump.can_id);
        start = 1;
        Serial.println("Saw CANBUS activity, starting message send");
    }
      //   start = 1;
      // if (fromPump.can_id == 0x1B200002) {
      //   if ((rxId & 0x80000000) == 0x80000000)
      //     sprintf(msgString, "%.8lX", (rxId & 0x1FFFFFFF));
      // }

      // if (!strcmp(msgString, "1B200002")){//This is a workaround since extID is 29 bit and arduino is 8 bit.
      //   Serial.println("Saw CANBUS activity, starting message send");
      //   start = 1;
      // }
  } 

  // Check every once in a while that the pump is still on
  if (pumpRunningCheck == 0 && start == 1 ) {
    if (mcp2515.readMessage(&fromPump) == MCP2515::ERROR_FAIL) {
      Serial.println("No activity, going to standby");
      start = 0;
      delay(14);
    }
    pumpRunningCheck = 100;
  }

  if (start == 1)
  {
    messageSender();
  }

  pumpRunningCheck--;
}


