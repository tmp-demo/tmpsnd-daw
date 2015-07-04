#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>

struct ProtocolMessage
{
  ProtocolMessage(char* aData, uint32_t aLength)
    : mData(aData)
    , mLength(aLength)
  {}
  ProtocolMessage()
    : mData(nullptr)
    , mLength(0)
  {}
  ProtocolMessage(const ProtocolMessage& aProtocolMessage)
  {
    mData = aProtocolMessage.mData;
    mLength = aProtocolMessage.mLength;
  }

  char* mData;
  uint32_t mLength;
};

class Protocol
{
  public:
    Protocol()
      : mPrevTimeStamp(0.0f)
    {}
    ProtocolMessage ParameterChange(float aTimestamp, uint32_t aIndex, float aValue) {
      uint32_t len = sprintf(mStorage, "%f,%d,%f", aTimestamp, aIndex, aValue);
      printf("sending |%s|\n", mStorage);
      return ProtocolMessage(mStorage, len);
    }
    ProtocolMessage NoteOn(float aTimestamp, uint32_t aIndex, uint32_t aNote, uint8_t aVelocity) {
      uint32_t len = sprintf(mStorage, "%f,%d,%d,%d", aTimestamp, aIndex, aNote, aVelocity);
      printf("sending |%s|\n", mStorage);
      return ProtocolMessage(mStorage, len);
    }
    ProtocolMessage NoteOff(float aTimestamp, uint32_t aIndex) {
      uint32_t len = sprintf(mStorage, "%f,%d", aTimestamp, aIndex);
      printf("sending |%s|\n", mStorage);
      return ProtocolMessage(mStorage, len);
    }
  private:
    char mStorage[256];
    float mPrevTimeStamp;
};

#endif // PROTOCOL_H
