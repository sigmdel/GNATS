#!/usr/bin/python3

## Basic NTP client which breaks out the NTP response
##
## Source: a number of responses in stackoverflow exchange
##   https://stackoverflow.com/questions/39466780/simple-sntp-python-script
## Avoiding use of ntplib that would have to be installed

#-- user defined macro --

DEFAULT_SERVER = 'pool.ntp.org'

#------------------------------

def usage():
    print("""nptc2.py is a basic NTP client that queries a specified NTP server and prints 
the NTP response record and the current time to the console

Usage:
  nptc2.py [-? | -h |--help] [ip | url]
    -?, -h, --help - print this message
    ip - the IPv4 address of the NTP server to query
    url - the host name of the NTP server to query.""")    
    print("    default - {}\n".format(DEFAULT_SERVER))
    print("""Example:
  $ ntpc2.py""")
    print("  NTP server {} will be queried".format(DEFAULT_SERVER))
    print("""  Received 48 bytes from ('138.197.135.239', 123):

  ntp_flags flags:          28 (leap 0, version 3, mode 4)
  uint8_t stratum:          2
  uint8_t poll:             0
  int8_t precision:         -23
  unint32_t rootDelay:      746
  unint32_t rootDispersion: 2066
  unint32_t refId:          46335091

  unint32_t refTm_s: 3970843229  Thu Oct 30 20:00:29 2025 UTC
  unint32_t refTm_f: 2019904293  (fraction 0.470296)

  unint32_t origTm_s: 0  Mon Jan  1 00:00:00 1900 UTC
  unint32_t origTm_f: 0  (fraction 0.000000)

  unint32_t rxTm_s: 3970844270  Thu Oct 30 20:17:50 2025 UTC
  unint32_t rxTm_f: 2393838499  (fraction 0.557359)

  unint32_t txTm_s: 3970844270  Thu Oct 30 20:17:50 2025 UTC
  unint32_t txTm_f: 2394164620  (fraction 0.557435)

  Time = Thu Oct 30 13:20:18 2025

Note:
  The received from IP address is that of the queried server or pool. The actual NTP server
  that returned the time could be some other computer. That is always the case when an NTP
  pool is queried""")
    exit()

import socket
import struct
import sys
import time

if not len(sys.argv) == 2:
    server = DEFAULT_SERVER
else:
    server = sys.argv[1]

if server in {"-h", "--help", "-?"}:
    usage()

print("NTP server '{}' will be queried".format(server))


TIME1970 = 2208988800  # seconds since 1970.1.1 00:00:00
po2 = 4294967296       # 2^32

def showtime(name, s, f):
    ti = time.gmtime(s -TIME1970)
    print('  unint32_t {}_s: {}  {} UTC'.format(name, s, time.asctime(ti)))
    print('  unint32_t {}_f: {}  (fraction {:f})'.format(name, f, f/po2))
    print()
  

def sntp_client():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        data = '\x1b' + 47 * '\0'
        client.sendto(data.encode('utf-8'), (server, 123))
        client.settimeout(5)
        data, address = client.recvfrom(1024)
        
        print('Received {} bytes from {}:'.format(len(data), address))
        print()
        li = (data[0] >> 6) & 0b11
        vn = (data[0] >> 3) & 0b111
        mode = data[0] & 0b111
        print('  ntp_flags flags:          {} (leap {}, version {}, mode {})'.format(data[0], li, vn, mode))
        print('  uint8_t stratum:          {}'.format(data[1]))
        print('  uint8_t poll:             {}'.format(data[2]))
        sb = struct.unpack('!1b', data[3:4])
        print('  int8_t precision:         {}'.format(sb[0]))

        stamps = struct.unpack('!12I', data)
        print('  unint32_t rootDelay:      {}'.format(stamps[1]))
        print('  unint32_t rootDispersion: {}'.format(stamps[2]))

        try:
           s = struct.pack("I", stamps[3]).decode()
        except:
           s = data[12:16].hex() 
        print('  unint32_t refId:          {}'.format(s))

        print()
        showtime('refTm',  stamps[4], stamps[5])
        showtime('origTm', stamps[6], stamps[7])
        showtime('rxTm',   stamps[8], stamps[9])
        showtime('txTm',   stamps[10], stamps[11])

        print('Local time: {}'.format(time.ctime(stamps[10]-TIME1970)))

if __name__ == '__main__':
    sntp_client()
