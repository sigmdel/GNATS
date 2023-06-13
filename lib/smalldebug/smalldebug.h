// smalldebug.h

/* With thanks to

    Ralph Bacon for *224 Superior Serial.print statements*
      @ https://github.com/RalphBacon/224-Superior-Serial.print-statements

  and

    Li Feng for *DFRobot_DHT20.h*
      @ https://github.com/DFRobot/DFRobot_DHT20/blob/master/DFRobot_DHT20.h
*/
#pragma once

#if (!defined(ENABLE_DBG))
#define ENABLE_DBG 0
#endif

#if (ENABLE_DBG == 1)
#define DBG(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print("] "); Serial.println(__VA_ARGS__);}
#define DBGF(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print("] "); Serial.printf(__VA_ARGS__);}
#else
#define DBG(...)
#define DBGF(...)
#endif
