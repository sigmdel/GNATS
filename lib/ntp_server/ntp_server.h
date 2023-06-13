#include "Arduino.h"
#include "AsyncUDP.h"

class NTP_Server {
public:
  NTP_Server( );
  ~NTP_Server();
  bool begin(uint16_t port = 123);
private:
  static void processUDPPacket(AsyncUDPPacket& packet);
};
