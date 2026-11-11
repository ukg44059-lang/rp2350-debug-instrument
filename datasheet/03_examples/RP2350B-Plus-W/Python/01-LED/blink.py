from machine import Pin, Timer

led_1 = Pin("LED", Pin.OUT)
led_2 = Pin(23, Pin.OUT)
led_1.value(0)
led_2.value(0)

tim = Timer()
def tick(timer):
    global led_1,led_2
    led_1.toggle()
    led_2.toggle()

tim.init(freq=2.5, mode=Timer.PERIODIC, callback=tick)
