ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x70000000, LENGTH = 64K
    RAM (rwx) : ORIGIN = 0x20200000, LENGTH = 128K
}
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
    .fcb ORIGIN(FLASH) : { KEEP(*(.fcb)) } > FLASH
    .boot_ivt 0x70001000 : { KEEP(*(.boot_ivt)) } > FLASH
    .boot_data 0x70001020 : { KEEP(*(.boot_data)) } > FLASH
    .vectors 0x70002000 : { KEEP(*(.vectors)) } > FLASH
    .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
    .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT>FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment*) }
}
ASSERT(SIZEOF(.fcb) == 512, "FCB must be exactly 512 bytes")
ASSERT(_ebss < _stack_top - 4096, "Insufficient stack")
