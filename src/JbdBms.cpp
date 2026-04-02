#include "JbdBms.h"

// Protocol Constants
#define FRAME_START 0xDD
#define FRAME_STOP 0x77
#define STATE_READ 0xA5
#define STATE_WRITE 0x5A

#define CMD_BASIC_INFO 0x03
#define CMD_MOS_CTRL 0xE1

// Debug flag - set to 1 to enable Serial debug output
#define BMS_DEBUG 1

JbdBms::JbdBms(HardwareSerial &serial) : _serial(serial) {
  data.isConnected = false;
  data.lastUpdate = 0;
  data.voltage = 0;
  data.current = 0;
  data.soc = 0;
}

void JbdBms::begin() {
  // Serial is initialized in main setup
}

uint16_t JbdBms::calculateChecksum(uint8_t *buf, uint8_t len) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    sum += buf[i];
  }
  return (uint16_t)((~sum + 1) & 0xFFFF);
}

void JbdBms::sendCommand(uint8_t cmd, uint8_t *txData, uint8_t len) {
  // Build frame: DD [A5/5A] [CMD] [LEN] [DATA...] [CS_H] [CS_L] 77
  uint8_t frame[24];
  uint8_t idx = 0;

  frame[idx++] = FRAME_START;
  frame[idx++] = (txData == nullptr) ? STATE_READ : STATE_WRITE;
  frame[idx++] = cmd;
  frame[idx++] = len;

  // Data (if any)
  for (uint8_t i = 0; i < len; i++) {
    frame[idx++] = txData[i];
  }

  // Checksum (over CMD + LEN + DATA)
  uint8_t csData[24];
  csData[0] = cmd;
  csData[1] = len;
  for (uint8_t i = 0; i < len; i++) {
    csData[2 + i] = txData[i];
  }
  uint16_t cs = calculateChecksum(csData, 2 + len);
  frame[idx++] = (cs >> 8) & 0xFF;
  frame[idx++] = cs & 0xFF;
  frame[idx++] = FRAME_STOP;

  // Clear RX buffer
  while (_serial.available())
    _serial.read();

  // Send
  _serial.write(frame, idx);
  _serial.flush();

#if BMS_DEBUG
  Serial.print("TX: ");
  for (uint8_t i = 0; i < idx; i++) {
    Serial.printf("%02X ", frame[i]);
  }
  Serial.println();
#endif
}

bool JbdBms::update() {
  // Send read basic info command
  sendCommand(CMD_BASIC_INFO, nullptr, 0);

  // Wait for response with timeout
  uint8_t response[64];
  uint8_t idx = 0;
  unsigned long start = millis();

  // Read all available bytes with timeout
  while (millis() - start < 500) {
    if (_serial.available()) {
      if (idx < 64) {
        response[idx++] = _serial.read();
      }
      start = millis(); // Reset timeout on each byte
    }

    // Check if we have a complete frame
    if (idx >= 7 && response[0] == FRAME_START &&
        response[idx - 1] == FRAME_STOP) {
      break;
    }
  }

#if BMS_DEBUG
  Serial.print("RX (");
  Serial.print(idx);
  Serial.print(" bytes): ");
  for (uint8_t i = 0; i < idx; i++) {
    Serial.printf("%02X ", response[i]);
  }
  Serial.println();
#endif

  // Validate response
  // Format: DD [CMD] [STATUS] [LEN] [DATA...] [CS_H] [CS_L] 77
  if (idx < 7) {
    data.isConnected = false;
#if BMS_DEBUG
    Serial.println("BMS: No response or too short");
#endif
    return false;
  }

  if (response[0] != FRAME_START || response[idx - 1] != FRAME_STOP) {
    data.isConnected = false;
#if BMS_DEBUG
    Serial.println("BMS: Invalid frame markers");
#endif
    return false;
  }

  if (response[1] != CMD_BASIC_INFO) {
    data.isConnected = false;
#if BMS_DEBUG
    Serial.printf("BMS: Wrong command response: 0x%02X\n", response[1]);
#endif
    return false;
  }

  if (response[2] != 0x00) {
    data.isConnected = false;
#if BMS_DEBUG
    Serial.printf("BMS: Error status: 0x%02X\n", response[2]);
#endif
    return false;
  }

  uint8_t dataLen = response[3];
  if (idx != dataLen + 7) { // DD + CMD + STS + LEN + DATA + CS_H + CS_L + 77
    data.isConnected = false;
#if BMS_DEBUG
    Serial.printf("BMS: Length mismatch: got %d, expected %d\n", idx,
                  dataLen + 7);
#endif
    return false;
  }

  // Parse the data (starts at response[4])
  return parseBasicInfo(&response[4], dataLen);
}

bool JbdBms::parseBasicInfo(uint8_t *buffer, uint8_t len) {
  if (len < 23) {
#if BMS_DEBUG
    Serial.printf("BMS: Data too short: %d bytes\n", len);
#endif
    return false;
  }

  // Bytes 0-1: Total Voltage (10mV units, big-endian)
  uint16_t voltageRaw = (buffer[0] << 8) | buffer[1];
  data.voltage = voltageRaw * 0.01f;

#if BMS_DEBUG
  Serial.printf("V: [%02X %02X] = %d = %.2fV\n", buffer[0], buffer[1],
                voltageRaw, data.voltage);
#endif

  // Bytes 2-3: Current (10mA units, signed big-endian)
  int16_t currentRaw = (int16_t)((buffer[2] << 8) | buffer[3]);
  data.current = currentRaw * 0.01f;

#if BMS_DEBUG
  Serial.printf("I: [%02X %02X] = %d = %.2fA\n", buffer[2], buffer[3],
                currentRaw, data.current);
  Serial.printf("SOC: [%02X] = %d%%\n", buffer[19], buffer[19]);
#endif

  // Bytes 4-5: Remaining Capacity (10mAh units)
  uint16_t remCap = (buffer[4] << 8) | buffer[5];
  data.remainingCapacity = remCap * 0.01f;

  // Bytes 6-7: Nominal Capacity (10mAh units)
  uint16_t nomCap = (buffer[6] << 8) | buffer[7];
  data.nominalCapacity = nomCap * 0.01f;

  // Bytes 8-9: Cycle Count
  data.cycles = (buffer[8] << 8) | buffer[9];

  // Bytes 16-17: Protection Status
  data.protectionStatus = (buffer[16] << 8) | buffer[17];

  // Byte 19: RSOC (State of Charge %)
  data.soc = buffer[19];

  // Byte 20: FET Status (bit 0 = charge, bit 1 = discharge)
  data.chargeMosfetOn = (buffer[20] & 0x01) != 0;
  data.dischargeMosfetOn = (buffer[20] & 0x02) != 0;

  data.isConnected = true;
  data.lastUpdate = millis();

#if BMS_DEBUG
  Serial.printf("BMS: V=%.2fV, I=%.2fA, SOC=%d%%, CHG=%d\n", data.voltage,
                data.current, data.soc, data.chargeMosfetOn);
#endif

  return true;
}

bool JbdBms::enableCharging() {
  // 0xE1 with data [0x00, 0x00] to release lock / enable all
  uint8_t cmdData[2] = {0x00, 0x00};
  sendCommand(CMD_MOS_CTRL, cmdData, 2);
  delay(80);
  sendCommand(CMD_MOS_CTRL, cmdData, 2);
  delay(120);
  return true;
}

bool JbdBms::disableCharging() {
  // 0xE1 with data [0x00, 0x01] to disable charging
  uint8_t cmdData[2] = {0x00, 0x01};
  sendCommand(CMD_MOS_CTRL, cmdData, 2);
  delay(100);
  return true;
}
