ENTRY(_start)
EXTERN(_start)
PHDRS
{
  text PT_LOAD PHDRS ;
}
SECTIONS
{
  .vectors 0x0000 : { *(.vectors) } :text
  .text 0x0040 : { *(.text) } :text
  .rodata : { *(.rodata) } :text
  /*. = 0x1400;*/
  .data : { *(.data) } :text
  .bss : { *(.bss) } :text
}
