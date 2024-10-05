
#ifndef PICO_MODBUS_EEPROM_H
#define PICO_MODBUS_EEPROM_H

#include <stdio.h>
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#define I2C0_SDA_PIN 16
#define I2C0_SCL_PIN 17

#define DEVADDR 0x50
#define BAUDRATE 100000
#define WRITE_CYCLE_TIME_PER_BYTE 5
#define BITS_PER_BYTE 8

#define EEPROM_ADDR_LEN 2
#define SSID_ADDR (0)
#define PASS_ADDR (64)
#define CO2_SETPOINT_ADDR (128)
#define WRITE_CYCLE_TIME 5

void write_to_eeprom(uint16_t memory_address, const uint8_t *data, size_t length);
void read_from_eeprom(uint16_t memory_address, uint8_t *data_read, size_t length);
void get_network_eeprom(uint16_t memory_address, uint8_t *value);
void write_network_eeprom(uint16_t memory_address, const char *value);
#endif //PICO_MODBUS_EEPROM_H
