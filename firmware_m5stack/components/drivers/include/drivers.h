#pragma once
#include "esp_err.h"
esp_err_t drivers_init(void);
/* TODO : I2C externe (GPIO.EXT 7 et 8) : PCA9548A, PCA9685, PCF8574 ; JSN-SR04T ; relais DO1 a DO4 ;
   Nema 23 STEP G1 / DIR G4 / EN G5 ; BTS7960 en tout ou rien via PCA9685. */
