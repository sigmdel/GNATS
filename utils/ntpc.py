#!/usr/bin/python

## Source: a number of responses in stackoverflow exchange
##   https://stackoverflow.com/questions/39466780/simple-sntp-python-script
## Avoiding use of ntplib that would have to be installed

import socket
import struct
import sys
import time

#DEFAULT_SERVER = '0.ca.pool.ntp.org'
#DEFAULT_SERVER = '0.uk.pool.ntp.org'
DEFAULT_SERVER = 'pool.ntp.org'

if not len(sys.argv) == 2:
    server = DEFAULT_SERVER
else:
    server = sys.argv[1]

print("NTP server '{}' will be queried".format(server))

TIME1970 = 2208988800  # seconds since 1970.1.1 00:00:00

def sntp_client():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        data = '\x1b' + 47 * '\0'
        client.sendto(data.encode('utf-8'), (server, 123))
        client.settimeout(5)
        data, address = client.recvfrom(1024)
        
        print('Received {} bytes from {}:'.format(len(data), address))
        #upacket = struct.unpack( '!48B', data )
        #print(upacket)
        print(data.hex())
        t = struct.unpack('!12I', data)[10] - TIME1970
        print('Time = %s' % time.ctime(t))

if __name__ == '__main__':
    sntp_client()
