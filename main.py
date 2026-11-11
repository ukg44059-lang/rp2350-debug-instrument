from machine import Pin
from time import sleep_ms

# Waveshare RP2350B-Plus-W: LED2 is active-high on GPIO23.
led = Pin(23, Pin.OUT, value=0)

while True:
    led.on()
    sleep_ms(200)
    led.off()
    sleep_ms(200)
