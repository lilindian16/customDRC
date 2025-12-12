# Custom DRC for the Bit10 DSP

## Reverse Engineering

![Bit10 DRC PCB with screen removed](/Images/Boards/audisonDRCBoard.png)

### Bit10 DRC Schema

![Bit10 DRC schema](/Images/Schema/Bit10DRC_rev_eng.png)

## CustomDRC

### Bare PCB

![Rev B PCB Front](/Images/Boards/RevB/revb_front_pcb.png)

![Rev B PCB Rear](/Images/Boards/RevB/revb_rear_pcb.png)

### Assembled Board

![Reb B PCB Assembled](/Images/Boards/RevB/revb_front_pop.png)

Boards can be programmed via TC2030 header or 2.54mm pin header (left unpopulated). Once programmed,
further updates can be handled via the OTA Update interface through the local webapp

### Board Bringup - Scope Captures

![Encoder Pin B during rotation](/Images/Traces/enc_b_cw.png)

![Capture of button signal on single press](/Images/Traces/enc_button_press_single.png)

![Capture of button signal with double press](/Images/Traces/enc_button_double_press.png)
