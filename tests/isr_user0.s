# file: isr_user0.s

.extern value0

.section isr
# interrupt routine for free entry 0
.global isr_user0
isr_user0:
  push r0
  push r1
  ldr r0, $0xABCD
  ldr r1, $value0
  str r0, [r1]
  pop r1
  pop r0
  iret
  
.end
