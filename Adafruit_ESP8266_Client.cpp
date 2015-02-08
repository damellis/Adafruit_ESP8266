//#include "Adafruit_ESP8266.h"
//
//int Adafruit_ESP8266_Client::connect(IPAddress ip, uint16_t port) {
//}
//
//int Adafruit_ESP8266_Client::connect(const char *host, uint16_t port) {
//  return Adafruit_ESP8266.connectTCP(host, port);
//}
//
//size_t Adafruit_ESP8266_Client::write(uint8_t c) {
//  return Adafruit_ESP8266.write(c);
//}
//
//size_t Adafruit_ESP8266_Client::write(const uint8_t *buf, size_t size) {
//  return Adafruit_ESP8266.write(buf, size);
//}
//
//int Adafruit_ESP8266_Client::available() {
//  return Adafruit_ESP8266.availableTCP();
//}
//
//int Adafruit_ESP8266_Client::read() {
//  return Adafruit_ESP8266.readTCP();
//}
//
//int Adafruit_ESP8266_Client::read(uint8_t *buf, size_t size) {
//  if (Adafruit_ESP8266.availableTCP()) {
//    buf[0] = Adafruit_ESP8266.readTCP();
//    return 1;
//  }
//  return 0;
//}
//int Adafruit_ESP8266_Client::peek() {
//  return Adafruit_ESP8266.peekTCP();
//}
//
//void Adafruit_ESP8266_Client::flush() {}
//
//void Adafruit_ESP8266_Client::stop() {
//  Adafruit_ESP8266.closeTCP();
//}
//
//uint8_t Adafruit_ESP8266_Client::connected() {
//  return Adafruit_ESP8266.connectedTCP();
//}
//
//operator Adafruit_ESP8266_Client::bool();
