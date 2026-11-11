import network
import socket
import time

from machine import Pin

"""
RP2350B Plus W Web Server - LED Control
=======================================

A simple web server running on Waveshare RP2350B Plus W
that allows controlling an LED remotely via WiFi.

Usage Instructions:
---------------------------------------
1. Configuration:
   - Replace 'YOUR NETWORK NAME' with your WiFi SSID
   - Replace 'YOUR NETWORK PASSWORD' with your WiFi password
   - Adjust the LED pin if needed (currently GPIO23)

2. Operation:
   - Run the program
   - Note the IP address printed in the console
   - Access these URLs in your browser:
     * http://<IP>/light/on  - Turns LED on
     * http://<IP>/light/off - Turns LED off
"""

led = Pin(23, Pin.OUT)

ssid = 'YOUR NETWORK NAME'
password = 'YOUR NETWORK PASSWORD'

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect(ssid, password)

html = """<!DOCTYPE html>
<html>
    <head> <title>Waveshare RP2350B Plus W</title> </head>
    <body> <h1>Waveshare RP2350B Plus W</h1>
        <p>%s</p>
    </body>
</html>
"""

max_wait = 10
while max_wait > 0:
    if wlan.status() < 0 or wlan.status() >= 3:
        break
    max_wait -= 1
    print('network connectting...')
    time.sleep(1)

if wlan.status() != 3:
    raise RuntimeError('network connection failed')
else:
    print('connected')
    status = wlan.ifconfig()
    print( 'ip = ' + status[0] )

addr = socket.getaddrinfo(status[0], 80)[0][-1]

print("Please visit http://{}/light/on to turn on the LED".format(status[0]))
print("Please visit http://{}/light/off to turn off the LED".format(status[0]))

s = socket.socket()
s.bind(addr)
s.listen(1)

print('listening on', addr)

# Listen for connections
while True:
    try:
        cl, addr = s.accept()
        print('client connected from', addr)
        request = cl.recv(1024)
        print(request)

        request = str(request)
        led_on = request.find('/light/on')
        led_off = request.find('/light/off')
        print( 'led on = ' + str(led_on))
        print( 'led off = ' + str(led_off))
        
        stateis = "ERROR"

        if led_on == 6:
            print("led on")
            led.value(1)
            stateis = "LED2 is ON"

        if led_off == 6:
            print("led off")
            led.value(0)
            stateis = "LED2 is OFF"

        response = html % stateis

        cl.send('HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n')
        cl.send(response)
        cl.close()

    except OSError as e:
        cl.close()
        print('connection closed')
