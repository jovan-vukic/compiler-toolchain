# file: isr_terminal.s

.section isr
# interrupt routine for terminal
.global isr_terminal
isr_terminal:
  iret
  
.end
