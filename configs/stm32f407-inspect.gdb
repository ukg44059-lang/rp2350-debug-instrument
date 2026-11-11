set pagination off
set confirm off
set remotetimeout 10
target extended-remote 127.0.0.1:3333
echo === Read-only register and Flash inspection; target temporarily halted ===\n
info registers pc sp xpsr
x/4wx 0x08000000
detach
quit
