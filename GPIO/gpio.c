/* Lighting LED using PWM0 on spresense for a short while */
#include <nuttx/config.h>
#include <stdio.h>

//GPIO API header files
#include <arch/board/board.h>
#include <arch/chip/pin.h>

int main(int argc, FAR char *argv[])
{
  printf("Lighting\n");
  board_gpio_config(PIN_PWM0,0,false,false,PIN_FLOAT);
  board_gpio_write(PIN_PWM0,1);
  sleep(10);
  board_gpio_config(PIN_PWM0,0,false,false,PIN_FLOAT);
  //board_gpio_write(PIN_PWM0,0);
}
