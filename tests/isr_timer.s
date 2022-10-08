# file: isr_timer.s

.section isr
# interrupt routine for timer
.global isr_timer
isr_timer:
  iret

.end
