ENTRY(_start)
EXTERN(_start)

MEMORY
{
  ram : ORIGIN = 0x100000, LENGTH = 0x200000
}

SECTIONS
{
  .text : {
    *(.text.start)
    *(.text)
  } > ram

  .rodata : {
    *(.rodata)
  } > ram

  .data : {
    *(.data)
  } > ram

  __bss_start = .;
  .bss : {
    *(.bss)
  } > ram
  __bss_end = .;

  .stack (NOLOAD) : {
    /* minimum 4k stack */
    . = 4096;
    . = ALIGN(4096);
  } > ram
  __end = .;
}
