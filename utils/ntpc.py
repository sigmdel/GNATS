#!/usr/bin/python3

## Basic NTP client
##
## Source: a number of responses in stackoverflow exchange
##   https://stackoverflow.com/questions/39466780/simple-sntp-python-script
## Avoiding use of ntplib that would have to be installed


#-- user defined macros --

DEFAULT_SERVER = 'pool.ntp.org'

# PRINT_RESPSONSE defines the format used to print the NTP server 
# response or if it is printed at all. Possible values
#   1 - print as an escaped hexadecimal sequence
#   2 - print as a hexadecimal string
#   3 - print as a decimal array
#   0 (or anything else) - does not print the response.
PRINT_RESPONSE = 0

#------------------------------

def usage():
    print("""nptc.py is a basic NTP client that queries a specified NTP server and returns the current time

Usage:
  nptc.py [-? | -h |--help] [ip | url]
    -?, -h, --help - print this message
    ip - the IPv4 address of the NTP server to query
    url - the host name of the NTP server to query.""")    
    print("    default - {}\n".format(DEFAULT_SERVER))
    print("""Example:
  $ ntpc.py""")
    print("  NTP server {} will be queried".format(DEFAULT_SERVER))
    print("""  Received 48 bytes from ('51.222.111.13', 123)
  Time = Thu Oct 30 13:20:46 2025
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

def sntp_client():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        data = '\x1b' + 47 * '\0'
        client.sendto(data.encode('utf-8'), (server, 123))
        client.settimeout(5)
        data, address = client.recvfrom(1024)

        print('Received {} bytes from {}:'.format(len(data), address))
        
        if PRINT_RESPONSE == 1:
             ## print bytes as escaped string of hexadecimals
             print(data)
        elif PRINT_RESPONSE == 2:     
             ## print bytes a a hex string
             print(data.hex())
        elif PRINT_RESPONSE == 3:    
             ## print bytes as array of decimal values
             upacket = struct.unpack( '!48B', data )
             print(upacket)
        else:
             ## don't print data
             pass      

        t = struct.unpack('!12I', data)[10] - TIME1970
        print('Time = %s' % time.ctime(t))

if __name__ == '__main__':
    sntp_client()
