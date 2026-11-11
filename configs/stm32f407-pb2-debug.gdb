set pagination off
set confirm off
set remotetimeout 10
file artifacts/pb2-demo/stm32f407-pb2-blink.elf
target extended-remote 127.0.0.1:3333
echo === Symbols and variable inspection ===\n
print/x demo_signature
print toggle_count
print led_level
if demo_signature != 0x407b0002
    error "Wrong demo signature"
end
echo === Hardware function breakpoint ===\n
hbreak led_toggle
continue
set $before = toggle_count
print toggle_count
print led_level
echo === Instruction single-step ===\n
x/i $pc
stepi
x/i $pc
echo === Source step and return ===\n
next
finish
print toggle_count
print led_level
if toggle_count != $before + 1
    error "LED toggle function did not increment the variable"
end
info registers pc sp xpsr
info breakpoints
delete breakpoints
echo PB2_BREAKPOINT_STEP_VARIABLE_CHECK_PASSED\n
detach
quit
