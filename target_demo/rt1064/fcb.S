// Preserve the target's already working FlexSPI configuration, not its SDRAM DCD.
.section .fcb,"a",%progbits
.incbin "artifacts/rt1064/original-fcb.bin"
