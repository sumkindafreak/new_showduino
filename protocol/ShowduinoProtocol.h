#ifndef SHOWDUINO_PROTOCOL_H
#define SHOWDUINO_PROTOCOL_H

#include <Arduino.h>

// =========================================================
// Showduino OS Protocol v1
// Shared by Director, C6 bridge, P4 Stage Engine, and future nodes.
// =========================================================

#define SHOWDUINO_PROTOCOL_MAGIC   0x5348444FUL  // ASCII: SHDO
#define SHOWDUINO_PROTOCOL_VERSION 1
#define SHOWDUINO_DEVICE_NAME_MAX  24
#define SHOWDUINO_PAYLOAD_MAX      96

// Packet types describe the purpose of each message.
enum ShowduinoPacketType : uint8_t {
  SHDO_PACKET_COMMAND   = 1,
  SHDO_PACKET_ACK       = 2,
  SHDO_PACKET_STATUS    = 3,
  SHDO_PACKET_HEARTBEAT = 4,
  SHDO_PACKET_DISCOVERY = 5,
  SHDO_PACKET_ERROR     = 6
};

// Roles identify which part of Showduino sent the packet.
enum ShowduinoDeviceRole : uint8_t {
  SHDO_ROLE_UNKNOWN      = 0,
  SHDO_ROLE_DIRECTOR     = 1,
  SHDO_ROLE_C6_BRIDGE    = 2,
  SHDO_ROLE_STAGE_ENGINE = 3,
  SHDO_ROLE_ACTOR        = 4
};

// Priority allows emergency traffic to jump ahead of normal commands.
enum ShowduinoPriority : uint8_t {
  SHDO_PRIORITY_LOW       = 0,
  SHDO_PRIORITY_NORMAL    = 1,
  SHDO_PRIORITY_HIGH      = 2,
  SHDO_PRIORITY_EMERGENCY = 3
};

// ACK result codes let the receiver report success or failure clearly.
enum ShowduinoAckCode : uint8_t {
  SHDO_ACK_OK             = 0,
  SHDO_ACK_REJECTED       = 1,
  SHDO_ACK_BAD_PACKET     = 2,
  SHDO_ACK_UNSUPPORTED    = 3,
  SHDO_ACK_BUSY           = 4,
  SHDO_ACK_INTERNAL_ERROR = 5
};

struct __attribute__((packed)) ShowduinoPacketV1 {
  uint32_t magic;
  uint8_t version;
  uint8_t packetType;
  uint8_t senderRole;
  uint8_t priority;
  uint16_t sequence;
  uint16_t ackSequence;
  uint32_t sentMillis;
  uint16_t checksum;
  char deviceName[SHOWDUINO_DEVICE_NAME_MAX];
  char payload[SHOWDUINO_PAYLOAD_MAX];
};

// Lightweight checksum for packet corruption detection.
inline uint16_t showduinoChecksum(const ShowduinoPacketV1 &packet) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&packet);
  const size_t checksumOffset = offsetof(ShowduinoPacketV1, checksum);
  uint16_t sum = 0;

  for (size_t i = 0; i < sizeof(ShowduinoPacketV1); i++) {
    if (i == checksumOffset || i == checksumOffset + 1) continue;
    sum = static_cast<uint16_t>((sum + bytes[i]) & 0xFFFF);
  }

  return sum;
}

inline void showduinoPreparePacket(
  ShowduinoPacketV1 &packet,
  ShowduinoPacketType type,
  ShowduinoDeviceRole role,
  ShowduinoPriority priority,
  uint16_t sequence,
  uint16_t ackSequence,
  const char *deviceName,
  const char *payload
) {
  memset(&packet, 0, sizeof(packet));
  packet.magic = SHOWDUINO_PROTOCOL_MAGIC;
  packet.version = SHOWDUINO_PROTOCOL_VERSION;
  packet.packetType = static_cast<uint8_t>(type);
  packet.senderRole = static_cast<uint8_t>(role);
  packet.priority = static_cast<uint8_t>(priority);
  packet.sequence = sequence;
  packet.ackSequence = ackSequence;
  packet.sentMillis = millis();

  if (deviceName != nullptr) {
    strncpy(packet.deviceName, deviceName, SHOWDUINO_DEVICE_NAME_MAX - 1);
  }

  if (payload != nullptr) {
    strncpy(packet.payload, payload, SHOWDUINO_PAYLOAD_MAX - 1);
  }

  packet.checksum = showduinoChecksum(packet);
}

inline bool showduinoValidatePacket(const ShowduinoPacketV1 &packet) {
  if (packet.magic != SHOWDUINO_PROTOCOL_MAGIC) return false;
  if (packet.version != SHOWDUINO_PROTOCOL_VERSION) return false;
  return packet.checksum == showduinoChecksum(packet);
}

inline bool showduinoIsEmergencyCommand(const char *payload) {
  if (payload == nullptr) return false;
  return strcmp(payload, "EMERGENCY:STOP") == 0 ||
         strcmp(payload, "EMERGENCY:CLEAR") == 0;
}

#endif
