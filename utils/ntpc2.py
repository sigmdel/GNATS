#!/usr/bin/python

## Source: a number of responses in stackoverflow exchange
##   https://stackoverflow.com/questions/39466780/simple-sntp-python-script
## Avoiding use of ntplib that would have to be installed

import socket
import struct
import sys
import time

#DEFAULT_SERVER = '0.ca.pool.ntp.org'
#DEFAULT_SERVER = '1.uk.pool.ntp.org'
DEFAULT_SERVER = 'pool.ntp.org'

if not len(sys.argv) == 2:
    server = DEFAULT_SERVER
else:
    server = sys.argv[1]

print("NTP server '{}' will be queried".format(server))


TIME1970 = 2208988800  # seconds since 1970.1.1 00:00:00
po2 = 4294967296       # 2^32

def showtime(name, s, f):
    ti = time.gmtime(s -TIME1970)
    print('unint32_t {}_s: {}  {} UTC'.format(name, s, time.asctime(ti)))
    print('unint32_t {}_f: {}  (fraction {:f})'.format(name, f, f/po2))
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
        print('ntp_flags flags:          {} (leap {}, version {}, mode {})'.format(data[0], li, vn, mode))
        print('uint8_t stratum:          {}'.format(data[1]))
        print('uint8_t poll:             {}'.format(data[2]))
        sb = struct.unpack('!1b', data[3:4])
        print('int8_t precision:         {}'.format(sb[0]))

        stamps = struct.unpack('!12I', data)
        print('unint32_t rootDelay:      {}'.format(stamps[1]))
        print('unint32_t rootDispersion: {}'.format(stamps[2]))

        try:
           s = struct.pack("I", stamps[3]).decode()
        except:
           s = data[12:16].hex() 
        print('unint32_t refId:          {}'.format(s))

        print()
        showtime('refTm',  stamps[4], stamps[5])
        showtime('origTm', stamps[6], stamps[7])
        showtime('rxTm',   stamps[8], stamps[9])
        showtime('txTm',   stamps[10], stamps[11])

        print('Local time: {}'.format(time.ctime(stamps[10]-TIME1970)))

if __name__ == '__main__':
    sntp_client()
