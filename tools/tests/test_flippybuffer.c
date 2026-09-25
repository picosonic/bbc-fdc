#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* Compile the production implementation directly; linker GC drops unrelated tools. */
#define main bbcfdc_program_main
#include "../bbcfdc.c"
#undef main

int main(void)
{
  const unsigned char input[] = { 0x01, 0x96, 0x80, 0xF0 };
  const unsigned char expected[] = { 0x0F, 0x01, 0x69, 0x80 };
  unsigned char guarded[] = { 0xA5, 0xCC, 0xCC, 0xCC, 0xCC, 0x5A };
  size_t index;

  flippybuffer=guarded+1;
  fillflippybuffer(input, sizeof(input));

  assert(guarded[0]==0xA5);
  assert(guarded[sizeof(guarded)-1]==0x5A);
  for (index=0; index<sizeof(input); index++)
    assert(guarded[index+1]==expected[index]);

  fillflippybuffer(NULL, 0);
  assert(guarded[0]==0xA5);
  assert(guarded[sizeof(guarded)-1]==0x5A);

  flippybuffer=NULL;
  puts("flippy buffer reversal: ok");
  return 0;
}
