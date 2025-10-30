# Python NTP Client Utilities

1. ntpc.py displays
    - (optionally) the raw NTP response 
    - the local time

    The format of the printed raw NTP response is set by the value of `PRINT_RESPONSE` at the start of the script.
    | value | format of raw packet |
    | :---: | --- |
    |  1  | escaped hexadecimal sequence |
    |  2  | hexadecimal string |
    |  3  | decimal array |
    |  0  | nothing is printed **(default)**|


2. ntp2c.py displays
    - all the fields of the returned NTP packet
    - the local time

## Usage

  path_to_python3 ntpc.py [-? | -h | --help] [server]    
  path_to_python3 ntpc2.py [-? | -h | --help] [server]

  - `path_to_python3` can be omitted if it is `/usr/bin/python3`
  - `server` can be the IPv4 address of the NTP server or its URL.


###  Examples

<pre>
$ <b>ntpc.py ubuntu.pool.ntp.org</b>
NTP server 'ubuntu.pool.ntp.org' will be queried
Received 48 bytes from ('45.33.53.84', 123):
Time = Tue Jun 13 17:51:21 2023
</pre>

<pre>
$ <b>ntpc2.py</b>
NTP server '2.ca.pool.ntp.org' will be queried
Received 48 bytes from ('216.197.228.230', 123):

ntp_flags flags:          28 (leap 0, version 3, mode 4)
uint8_t stratum:          1
uint8_t poll:             0
int8_t precision:         -23
unint32_t rootDelay:      1
unint32_t rootDispersion: 2
unint32_t refId:          SPP

unint32_t refTm_s: 3895678505  Tue Jun 13 20:55:05 2023 UTC
unint32_t refTm_f: 822817160  (fraction 0.191577)

unint32_t origTm_s: 0  Mon Jan  1 00:00:00 1900 UTC
unint32_t origTm_f: 0  (fraction 0.000000)

unint32_t rxTm_s: 3895678528  Tue Jun 13 20:55:28 2023 UTC
unint32_t rxTm_f: 2222262574  (fraction 0.517411)

unint32_t txTm_s: 3895678528  Tue Jun 13 20:55:28 2023 UTC
unint32_t txTm_f: 2222429071  (fraction 0.517450)

Local time: Tue Jun 13 17:55:28 2023
</pre>

### Note

The refID can be 
  - a four-character ASCII string, called the "kiss code", use for debugging and 
   used for debugging and monitoring purposes.    
  - a 1 to four character ASCII string assigned to the reference clock.
  - a reference identifier of the server used to detect timing loops
  
depending on the stratum of the server.  The script simply tries to decode the 32-bit field
as a string and shows that if it can, otherwise it displays the 32-bit value in hexadecimal value.  

Reference: [Network Time Protocol Version 4: Protocol and Algorithms Specification](https://www.rfc-editor.org/rfc/rfc5905).


## Requirement

  Python 3
