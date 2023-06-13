
// NTP server for ESP32 based on
//   ntp_server in 180662-mini-NTP-ESP32 by ElektorLabs
//   @ https://github.com/ElektorLabs/180662-mini-NTP-ESP32/tree/master/Firmware/src
//
// Instead of using callbacks to getUTCTime() and getSubsecond(), this server reads
// the ESP RTC directly with microsecond precision. Outside time sources (presently
// there is only a GPS receiver) are used to update the ESP RTC at regular intervals.
//
// References:
//   Get Current Time in ESP-IDF Programming - Guide System Time
//   @ https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#get-current-time
//
//   Network Time Protocol Version 4: Protocol and Algorithms Specification
//   @ https://www.rfc-editor.org/rfc/rfc5905
//
#include "Arduino.h"
#include "ntp_server.h"
#include <lwip/def.h>
#include "smalldebug.h"

// The UNIX epoch starts on 1.1.1970 and the NTP epoch starts on 1.1.1900
#define NTP_TIMESTAMP_DELTA  2208988800ull   // 70 years worth of seconds

typedef struct{
    uint8_t mode:3;               // mode. Three bits. Client will pick mode 3 for client.
    uint8_t vn:3;                 // vn.   Three bits. Version number of the protocol.
    uint8_t li:2;                 // li.   Two bits.   Leap indicator.
}ntp_flags_t;

typedef union {
    uint32_t data;
    uint8_t byte[4];
    char c_str[4];
} refID_t;

typedef struct {
  ntp_flags_t flags;
  uint8_t stratum;         // Eight bits. Stratum level of the local clock.
  uint8_t poll;            // Eight bits. Maximum interval between successive messages.
  int8_t  precision;       // Eight bits signed. Precision of the local clock.

  uint32_t rootDelay;      // 32 bits. Total round trip delay time.
  uint32_t rootDispersion; // 32 bits. Max error allowed from primary clock source.
  refID_t refId;           // 32 bits. Reference clock identifier.

  // Reference Timestamp: Time when the system clock was last set or
  // corrected, in NTP timestamp format.
  uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
  uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

  // Origin Timestamp: Time at the client when the request departed
  // for the server, in NTP timestamp format.
  uint32_t origTm_s;       // 32 bits. Origin time-stamp seconds.
  uint32_t origTm_f;       // 32 bits. Origin time-stamp fraction of a second.

  // Receive Timestamp: Time at the server when the request arrived
  // from the client, in NTP timestamp format.
  uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
  uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

  // Transmit Timestamp: Time at the server when the response left
  // for the client, in NTP timestamp format.
  uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
  uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet_t;

#if (ENABLE_DBG > 0)
void dumpNTP_packet(char * msg, ntp_packet_t ntpp) {
  DBG(msg);
  DBGF("flags: 0x%x\n", ntpp.flags);
  DBGF("stratum: %d\n", ntpp.stratum);
  DBGF("poll: %d\n", ntpp.poll);
  DBGF("precision: %d\n", ntpp.precision);
  DBGF("rootDelay: 0x%x\n", ntpp.rootDelay);
  DBGF("rootDispersion: 0x%x\n", ntpp.rootDispersion);

  refID_t ref;
  ref.data = htonl(ntpp.refId.data);
  DBGF("refID: %s\n", ref.c_str);

  DBGF("refTm_s: 0x%x\n", ntpp.refTm_s);
  DBGF("refTm_f: 0x%x\n", ntpp.refTm_f);

  DBGF("origTm_s: 0x%x\n", ntpp.origTm_s);
  DBGF("origTm_f: 0x%x\n", ntpp.origTm_f);

  DBGF("rxTm_s: 0x%x\n", ntpp.rxTm_s);
  DBGF("rxTm_f: 0x%x\n", ntpp.rxTm_f);

  DBGF("txTm_s: 0x%x\n", ntpp.txTm_s);
  DBGF("txTm_f: 0x%x\n\n", ntpp.txTm_f);
}
#endif

int8_t __calloverhead = 0;

int8_t DeterminePrecision( void ){
  /*
  timespec res;
  int intres = clock_getres(CLOCK_REALTIME, &res);
  DBGF("clock_getres = %s, res.tv_sec = %d, res.tv_nsec = %d\n", (intres) ? "err" : "ok", res.tv_sec, res.tv_nsec);
  yields [setup(): 260] clock_getres = 0, res.tv_sec = 0, res.tv_nsec = 1000 = 1 microsecond
  log2(1/1000000) = -19.93

  Note that the antilog of -18 base 2 is 0.000003814697 or about 4 µs not 1 µs.
  */

  /*
  Source: RFC 5905 pg 21 https://www.rfc-editor.org/rfc/rfc5905#section-7.3

   Precision: 8-bit signed integer representing the precision of the
   system clock, in log2 seconds.  For instance, a value of -18
   corresponds to a precision of about one microsecond.  The precision
   can be determined when the service first starts up as the minimum
   time of several iterations to read the system clock.

   Below the *average* time to call gettimeofday() is used.

  struct timeval tv;
  // time call to gettimeofday()
  uint32_t start = micros();
  for(uint32_t i=0;i<1024;i++)
    gettimeofday(&tv, NULL);
  uint32_t end = micros();
  double runtime = ((double)(end-start) ) / ( (double) (1024000000.0) ) ;
  __calloverhead = log2(runtime);
  DBGF("DeterminePrecision: runtime: %f, __calloverhead %d\n", runtime, __calloverhead);
  return __calloverhead;

  [DeterminePrecision(): 119 ] DeterminedPrecision: runtime: 0.000015, __calloverhead -16

   Below the *minimum* time to call gettimeofday()

   Gives the same result

   [DeterminePrecision(): 139] DeterminePrecision: run: 15, runtime: 0.000015, __calloverhead -16
  */

  struct timeval tv;
  uint32_t run = 10000000UL;
  for(uint32_t i=0;i<1024;i++) {
    // time call to gettimeofday()
    uint32_t start = micros();
    gettimeofday(&tv, NULL);
    uint32_t end = micros();
    if (end - start < run)
      run = end - start;
  }
  double runtime = (double) ((double) run / 1000000.0);
  __calloverhead = log2(runtime);
  DBGF("DeterminePrecision: run: %u, runtime: %f, __calloverhead %d\n", run, runtime, __calloverhead);
  return __calloverhead;
}


AsyncUDP udp;

NTP_Server::NTP_Server( ){
}

NTP_Server::~NTP_Server(){
}

bool NTP_Server::begin(uint16_t port){
  DeterminePrecision();
  if (udp.listen(port)) {
    udp.onPacket(NTP_Server::processUDPPacket);
    return true;
  }
  return false;
}

/* static function */
void NTP_Server::processUDPPacket(AsyncUDPPacket& packet) {
  uint32_t start_us = micros();
  ntp_packet_t ntp_req;
  struct timeval tv_now;
  if (gettimeofday(&tv_now, NULL)) {
    DBG("NTP_Server unable to get time of day");
    return;  // error
  }
  //DBGF("NTP_Server tv_now = (%u sec, %u usec)\n", tv_now.tv_sec, tv_now.tv_usec);
  if (packet.length() != sizeof(ntp_packet_t))
    return; // this is not what we want !

  memcpy(&ntp_req, packet.data(), sizeof(ntp_packet_t));

  //dumpNTP_packet("incoming", ntp_req);

  ntp_req.flags.li = 0;   // No impending leap second insertion
  ntp_req.flags.vn = 4;   // NTP Version 4
  ntp_req.flags.mode = 4; // Server
  ntp_req.stratum = 1;

  // We don't touch ntp_req.poll

  ntp_req.precision = __calloverhead;
  ntp_req.rootDelay=1;
  ntp_req.rootDispersion=1;
  strncpy(ntp_req.refId.c_str ,"GPS", sizeof(ntp_req.refId.c_str) );
  // Set to NTP byte order
  ntp_req.rootDelay = htonl( ntp_req.rootDelay );
  ntp_req.rootDispersion = htonl( ntp_req.rootDispersion );
  ntp_req.refId.data = ntohl( ntp_req.refId.data );

  // Set the origin Timestamp (origTm) which is the time at the client when
  // the request departed for the server, in NTP timestamp format.
  // In other words, it's the transmit time of the packet from the client.
  // Already in NTP byte order
  //
  // A systemd-timesyncd client will not update the system time if origTm
  // is not set to a "reasonable" value, even if txTm is correctly defined.
  ntp_req.origTm_s = ntp_req.txTm_s;
  ntp_req.origTm_f = ntp_req.txTm_f;

  // Set the receive Timestamp (rxTm) which is the time at the server
  // when the request arrived from the client, in NTP timestamp format.
  // About conversion of microseconds to 32-bit fractions of second
  // see https://gist.github.com/sigmdel/bea3b4065c6fdf2cc2d3c9c7fb1ddca0
  ldiv_t delta = div(tv_now.tv_usec, 1000000UL);
  ntp_req.rxTm_s= tv_now.tv_sec + delta.quot + NTP_TIMESTAMP_DELTA;
  ntp_req.rxTm_f= (uint32_t) ( ((uint64_t) delta.rem << 32) / 1000000L );
  // Set to NTP byte order
  ntp_req.rxTm_s = htonl(ntp_req.rxTm_s);
  ntp_req.rxTm_f = htonl(ntp_req.rxTm_f);

  // Set reference timestamp (refTm), which is the time when the system clock
  // was last set or corrected, to the received timestamp (rxTm).
  // - rxTm is already in NTP byte order
  ntp_req.refTm_s = ntp_req.rxTm_s;
  ntp_req.refTm_f = ntp_req.rxTm_f;

  // Set the transmit timestamp (txTm) which is the time at the server
  // when the response left for the client, in NTP timestamp format.
  // Using the original timestamp obtained at the beginning of the
  // routine plus the number of micro seconds elapsed since then.
  delta = div((long) (tv_now.tv_usec + micros() - start_us), 1000000L);
  ntp_req.txTm_s= tv_now.tv_sec + delta.quot + NTP_TIMESTAMP_DELTA;
  ntp_req.txTm_f= (uint32_t) ( ((uint64_t) delta.rem << 32) / 1000000L );
  // set to NTP byte order
  ntp_req.txTm_s = htonl(ntp_req.txTm_s);
  ntp_req.txTm_f = htonl(ntp_req.txTm_f);

  packet.write((uint8_t*)&ntp_req, sizeof(ntp_packet_t));

  #if (ENABLE_DBG > 0)
  DBGF("NTP response sent to %s:%d\n", packet.remoteIP().toString().c_str(), packet.remotePort());
  ntp_req.txTm_s = htonl(ntp_req.txTm_s);
  ntp_req.txTm_f = htonl(ntp_req.txTm_f);
  DBGF("txTm_s %u sec, txTm_f %u fraction\n", ntp_req.txTm_s, ntp_req.txTm_f);
  //ntp_req.txTm_s = htonl(ntp_req.txTm_s);
  //ntp_req.txTm_f = htonl(ntp_req.txTm_f);
  //dumpNTP_packet("outgoing", ntp_req);
  #endif
}
