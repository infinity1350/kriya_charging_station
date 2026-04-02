#ifndef JBD_BMS_H
#define JBD_BMS_H

#include <Arduino.h>

struct BmsData {
  float voltage;
  float current;
  float remainingCapacity;
  float nominalCapacity;
  uint16_t cycles;
  uint8_t soc;
  bool chargeMosfetOn;
  bool dischargeMosfetOn;
  uint16_t protectionStatus;
  bool isConnected;
  unsigned long lastUpdate;
};

class JbdBms {
public:
  JbdBms(HardwareSerial &serial);
  void begin();
  bool update();

  // Control
  bool enableCharging();
  bool disableCharging();

  // Getters
  const BmsData &getData() const { return data; }

private:
  HardwareSerial &_serial;
  BmsData data;

  void sendCommand(uint8_t cmd, uint8_t *txData, uint8_t len);
  uint16_t calculateChecksum(uint8_t *data, uint8_t len);
  bool readResponse(uint8_t cmd, uint8_t *buffer, uint16_t maxLen);
  bool parseBasicInfo(uint8_t *buffer, uint8_t len);
};

#endif
