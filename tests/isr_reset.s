# file: isr_reset.s

.extern my_start

.section isr
# interrupt routine for reset
.global isr_reset
isr_reset:
  jmp my_start

.end
