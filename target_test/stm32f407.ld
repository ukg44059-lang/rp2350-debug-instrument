ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 16K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .data : {
        _sdata = .; *(.data*) _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : {
        _sbss = .; *(.bss*) *(COMMON) _ebss = .;
    } > RAM
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    /DISCARD/ : { *(.comment*) *(.ARM.exidx*) *(.ARM.extab*) }
}
