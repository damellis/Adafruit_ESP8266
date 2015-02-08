/*------------------------------------------------------------------------
  An Arduino library for the ESP8266 WiFi-serial bridge

  https://www.adafruit.com/product/2282

  The ESP8266 is a 3.3V device.  Safe operation with 5V devices (most
  Arduino boards) requires a logic-level shifter for TX and RX signals.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried and Phil Burgess for Adafruit Industries.
  MIT license, all text above must be included in any redistribution.
  ------------------------------------------------------------------------*/

#include "Adafruit_ESP8266.h"

#define strIPD F("+IPD")

// Not the constructor
void Adafruit_ESP8266_Class::begin(Stream *s, Stream *d, int8_t r) {
  stream = s;
  debug = d;
  reset_pin = r;
  host = NULL;
  writing = false;
  buf[0] = 0; head = 0; tail = 0;
  setTimeouts();
};

// Override various timings.  Passing 0 for an item keeps current setting.
void Adafruit_ESP8266_Class::setTimeouts(
 uint32_t rcv, uint32_t rst, uint32_t con, uint32_t ipd) {
  if(rcv) {
    stream->setTimeout(rcv);
    receiveTimeout = rcv;
  }
  if(rst) resetTimeout   = rst;
  if(con) connectTimeout = con;
  if(ipd) ipdTimeout     = ipd;
}

// Override boot marker string, or pass NULL to restore default.
void Adafruit_ESP8266_Class::setBootMarker(Fstr *s) {
  bootMarker = s ? s : defaultBootMarker;
}

// Anything printed to the EPS8266 object will be split to both the WiFi
// and debug streams.  Saves having to print everything twice in debug code.
size_t Adafruit_ESP8266_Class::write(uint8_t c) {
  if(debug) {
    if(!writing) {
      debug->print(F("---> "));
      writing = true;
    }
    debug->write(c);
  }
  return stream->write(c);
}

// Equivalent to Arduino Stream find() function, but with search string in
// flash/PROGMEM rather than RAM-resident.  Returns true if string found
// (any further pending input remains in stream), false if timeout occurs.
// Can optionally pass NULL (or no argument) to read/purge the OK+CR/LF
// returned by most AT commands.
//
// Looks for unsolicited +IPD messages (incoming TCP data) and stores any
// received data to a buffer (for retrieval with readTCP(), etc.). Does not
// do any searching within this +IPD (incoming TCP) data.
boolean Adafruit_ESP8266_Class::find(Fstr *str) {
  uint8_t  stringLength, matchedLength = 0;
  uint8_t  stringLengthIPD, matchedLengthIPD = 0;
  int      c;
  boolean  found = false;
  uint32_t t, save = 0;
  uint16_t bytesToGo = 0;

  if(str == NULL) str = F("OK\r\n");
  stringLength = strlen_P((Pchr *)str);
  stringLengthIPD = strlen_P((Pchr *)strIPD);

  for(t = millis();;) {
    if((c = stream->read()) > 0) { // Character received?
      t = millis();     // Timeout resets w/each byte received

      if(debug) {
        if(writing) {
          debug->print(F("<--- '"));
          writing = false;
        }
        debug->write(c);   // Copy to debug stream
      }
      
      if(bytesToGo > 0) { // in a "+IPD" packet
        uint16_t next = (head + 1) % BUF_SIZE;
        buf[head] = c;
        head = next;
        if (head == tail) tail = (tail + 1) % BUF_SIZE;
        bytesToGo--;
        if (bytesToGo == 0) {
          setTimeouts(save);
        }
      } else {
        if((stringLength > 0) && (c == pgm_read_byte((Pchr *)str +
                                  matchedLength))) { // Match next byte?
          if(++matchedLength == stringLength) { // Matched whole string?
            found = true;                       // Winner!
            break;
          }
        } else {          // Character mismatch; reset match pointer+counter
          matchedLength = 0;
        }
        if(c == pgm_read_byte((Pchr *)strIPD +
                              matchedLengthIPD)) { // Match next byte?
          if(++matchedLengthIPD == stringLengthIPD) { // Matched whole string?
            for(;;) {
              if((c = stream->read()) > 0) { // Read subsequent chars...
                if(debug) debug->write(c);
                if(c == ':') {          // ...until delimiter.
                  save = receiveTimeout;
                  setTimeouts(ipdTimeout);
                  break;
                }
                bytesToGo = (bytesToGo * 10) + (c - '0'); // Keep count
                t = millis();    // Timeout resets w/each byte received
              } else if(c < 0) { // No data on stream, check for timeout
                if((millis() - t) > receiveTimeout) goto bail;
              } else goto bail; // EOD on stream
            }
          }
        } else {          // Character mismatch; reset match pointer+counter
          matchedLengthIPD = 0;
        }
      }
    } else if(c < 0) {  // No data on stream, check for timeout
      if((millis() - t) > receiveTimeout) break; // You lose, good day sir
    } else break;       // End-of-data on stream
  }

  bail: // Sorry, dreaded goto.  Because nested loops.
  if(debug) debug->println('\'');
  if (save != 0) setTimeouts(save);
  return found;
}

void Adafruit_ESP8266_Class::poll() {
  find(F(""));
}

// Read from ESP8266 stream into RAM, up to a given size.  Max number of
// chars read is 1 less than this, so NUL can be appended on string.
int Adafruit_ESP8266_Class::readLine(char *buf, int bufSiz) {
  if(debug && writing) {
    debug->print(F("<--- '"));
    writing = false;
  }
  int bytesRead = stream->readBytesUntil('\r', buf, bufSiz-1);
  buf[bytesRead] = 0;
  if(debug) {
    debug->print(buf);
    debug->println('\'');
  }
  while(stream->read() != '\n'); // Discard thru newline
  return bytesRead;
}

// ESP8266 is reset by momentarily connecting RST to GND.  Level shifting is
// not necessary provided you don't accidentally set the pin to HIGH output.
// It's generally safe-ish as the default Arduino pin state is INPUT (w/no
// pullup) -- setting to LOW provides an open-drain for reset.
// Returns true if expected boot message is received (or if RST is unused),
// false otherwise.
boolean Adafruit_ESP8266_Class::hardReset(void) {
  if(reset_pin < 0) return true;
  writing = true;             // Not really, but will initiate incoming data
  boolean  found = false;
  uint32_t save  = receiveTimeout; // Temporarily override recveive timeout,
  receiveTimeout = resetTimeout;   // reset time is longer than normal I/O.
  digitalWrite(reset_pin, LOW);
  pinMode(reset_pin, OUTPUT); // Open drain; reset -> GND
  delay(10);                  // Hold a moment
  pinMode(reset_pin, INPUT);  // Back to high-impedance pin state
  found = find(bootMarker);    // Purge boot message from stream
  receiveTimeout = save;
  return found;
}

// Soft reset.  Returns true if expected boot message received, else false.
boolean Adafruit_ESP8266_Class::softReset(void) {
  boolean  found = false;
  uint32_t save  = receiveTimeout; // Temporarily override recveive timeout,
  receiveTimeout = resetTimeout;   // reset time is longer than normal I/O.
  stream->setTimeout(resetTimeout);
  println(F("AT+RST"));            // Issue soft-reset command
  if(find(bootMarker)) {           // Wait for boot message
    println(F("ATE0"));            // Turn off echo
    found = find();                // OK?
  }
  receiveTimeout = save;           // Restore normal receive timeout
  return found;
}

// For interactive debugging...shuttle data between Serial Console <-> WiFi
void Adafruit_ESP8266_Class::debugLoop(void) {
  if(!debug) for(;;); // If no debug connection, nothing to do.

  debug->println(F("\n========================"));
  for(;;) {
    if(debug->available())  stream->write(debug->read());
    if(stream->available()) debug->write(stream->read());
  }
}

// Connect to WiFi access point.  SSID and password are flash-resident
// strings.  May take several seconds to execute, this is normal.
// Returns true on successful connection, false otherwise.
boolean Adafruit_ESP8266_Class::connectToAP(Fstr *ssid, Fstr *pass) {
  char buf[256];

  println(F("AT+CWMODE=1")); // WiFi mode = Sta
  delay(2000);
  readLine(buf, sizeof(buf));
  if(!(strstr_P(buf, (Pchr *)F("OK")) ||
       strstr_P(buf, (Pchr *)F("no change")))) return false;

  print(F("AT+CWJAP=\"")); // Join access point
  print(ssid);
  print(F("\",\""));
  print(pass);
  println('\"');
  uint32_t save = receiveTimeout;  // Temporarily override recv timeout,
  receiveTimeout = connectTimeout; // connection time is much longer!
  boolean found = find();          // Await 'OK' message
  receiveTimeout = save;           // Restore normal receive timeout
  if(found) {
    println(F("AT+CIPMUX=0"));     // Set single-client mode
    found = find();                // Await 'OK'
  }

  return found;
}

void Adafruit_ESP8266_Class::closeAP(void) {
  println(F("AT+CWQAP")); // Quit access point
  find(); // Purge 'OK'
}

// Open TCP connection.  Hostname is flash-resident string.
// Returns true on successful connection, else false.
boolean Adafruit_ESP8266_Class::connectTCP(Fstr *h, int port) {

  print(F("AT+CIPSTART=\"TCP\",\""));
  print(h);
  print(F("\","));
  println(port);
  
  if(find(F("Linked"))) {
    host = h;
    return true;
  }
  return false;
}

void Adafruit_ESP8266_Class::closeTCP(void) {
  println(F("AT+CIPCLOSE"));
  find(F("Unlink\r\n"));
  host = NULL;
}

boolean Adafruit_ESP8266_Class::connectedTCP(void) {
  // connection open or unread data remaining in buffer
  return (host != NULL || head != tail);
}

// Write byte over the currently-open TCP connection.
boolean Adafruit_ESP8266_Class::writeTCP(uint8_t c) {
  println(F("AT+CIPSEND=1"));
  if(find(F("> "))) { // Wait for prompt
    print(c);
    return(find()); // Gets 'SEND OK' line
  }
  return false;
}

// Write bytes over the currently-open TCP connection.
boolean Adafruit_ESP8266_Class::writeTCP(uint8_t *buf, size_t size) {
  print(F("AT+CIPSEND="));
  println(size);
  if(find(F("> "))) { // Wait for prompt
    for (int i = 0; i < size; i++) print(buf[i]);
    return(find()); // Gets 'SEND OK' line
  }
  return false;
}

int Adafruit_ESP8266_Class::peekTCP(void) {
  poll();
  if (tail == head) return -1;
  return buf[tail];
}

int Adafruit_ESP8266_Class::readTCP(void) {
  uint8_t c;
  poll();
  if (tail == head) return -1;
  c = buf[tail];
  tail = (tail + 1) % BUF_SIZE;
  return c;
}

int Adafruit_ESP8266_Class::availableTCP(void) {
  poll();
  return (head < tail) ? (head + BUF_SIZE - tail) : (head - tail);
}

// Requests page from currently-open TCP connection.  URL is
// flash-resident string.  Returns true if request issued successfully,
// else false.  Calling function should then handle data returned, may
// need to parse IPD delimiters (see notes in find() function.
// (Can call find(F("Unlink"), true) to dump to debug.)
boolean Adafruit_ESP8266_Class::requestURL(Fstr *url) {
  print(F("AT+CIPSEND="));
  println(25 + strlen_P((Pchr *)url) + strlen_P((Pchr *)host));
  if(find(F("> "))) { // Wait for prompt
    print(F("GET ")); // 4
    print(url);
    print(F(" HTTP/1.1\r\nHost: ")); // 17
    print(host);
    print(F("\r\n\r\n")); // 4
    return(find()); // Gets 'SEND OK' line
  }
  return false;
}

Adafruit_ESP8266_Class Adafruit_ESP8266;
