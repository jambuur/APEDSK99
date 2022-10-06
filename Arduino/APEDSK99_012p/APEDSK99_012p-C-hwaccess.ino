//databus:
//D0,0,PD0 / D1,1,PD1 / D2,4,PD4 / D3,5,PD5 / D4,6,PD6 / D5,7,PD7 / D6,8,PD8 / D7,9,PD9
//switch databus to INPUT state for reading from RAM

void databus_input( void ) {
  DDRD  &= B00001100;                                                                                       //set PD7-PD4 and PD1-PD0 to input (D5-D2, D1-D0)
  PORTD &= B00001100;                                                                                       //initialise input pins
  DDRB  &= B11111100;                                                                                       //set PB1-PB0 to input (D7-D6)
  PORTB &= B11111100;                                                                                       //initialise input pins
}

//switch databus to OUTPUT state for writing to RAM
void databus_output( void ) {
  DDRD  |= B11110011;                                                                                       //set PD7-PD4 and PD1-PD0 to output (D5-D2, D1-D0)
  DDRB  |= B00000011;                                                                                       //set PB1-PB0 to output (D7-D6)
}

//disable Arduino control bus; CE* and WE* both HighZ
void disable_controlbus( void ) {
  pinAsInput(CE);
  pinAsInput(WE);
}

//enable Arduino control bus; CE* and WE* both HIGH due to 5.1K pull-ups
void enable_controlbus( void ) {
  pinAsOutput(CE);
  pinAsOutput(WE);
}

//read a byte from the databus
byte read_databus( void ) {
  NOP();                                                                                                    //long live the Logic Analyzer
  return ( ((PIND & B00000011)     ) +                                                                      //read PD1,PD0 (D1-D0)
           ((PIND & B11110000) >> 2) +                                                                      //read PD7-PD3 (D5-D1)     
           ((PINB & B00000011) << 6) );                                                                     //read PB1-PBO (D7-D6)
}

//place a byte on the databus
void write_databus( byte data ) {
  PORTD |= ((data     ) & B00000011);                                                                       //write PD1 (D0)
  PORTD |= ((data << 2) & B11110000);                                                                       //write PD7-PD3 (D5-D1)
  PORTB |= ((data >> 6) & B00000011);                                                                       //write PB1-PBO (D7-D6)
}

//shift out the given address to the 74HC595 registers
void set_addressbus( unsigned int address ) {
  digitalLow(LATCH);                                                                                        //disable latch line
  digitalLow(DS);                                                                                           //clear data pin

  //OK, repetitive, slightly ugly code but ... 1.53x as fast as the more elegant for() - if() - else()
  //for every address bit set:
  //CLOCK -> LOW, address bit -> DS bit, CLOCK -> HIGH to shift and DS bit -> LOW to prevent bleed-through
  digitalLow(CLOCK);
  
  PORTC |= ( (address >> 10) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A15
  digitalLow(CLOCK);
  PORTC |= ( (address >>  9) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A14
  digitalLow(CLOCK);
  PORTC |= ( (address >>  8) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A13
  digitalLow(CLOCK);
  PORTC |= ( (address >>  7) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A12
  digitalLow(CLOCK);
  PORTC |= ( (address >>  6) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A11
  digitalLow(CLOCK);
  PORTC |= ( (address >>  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A10
  digitalLow(CLOCK);
  PORTC |= ( (address >>  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A9
  digitalLow(CLOCK);
  PORTC |= ( (address >>  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A8
  digitalLow(CLOCK);
  PORTC |= ( (address >>  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A7
  digitalLow(CLOCK);
  PORTC |= ( (address >>  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A6
  digitalLow(CLOCK);
  PORTC |= ( (address      ) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A5
  digitalLow(CLOCK);
  PORTC |= ( (address <<  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A4
  digitalLow(CLOCK);
  PORTC |= ( (address <<  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A3
  digitalLow(CLOCK);
  PORTC |= ( (address <<  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A2
  digitalLow(CLOCK);
  PORTC |= ( (address <<  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A1
  digitalLow(CLOCK);
  PORTC |= ( (address <<  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); // A0
  
  digitalLow(CLOCK);                                                                                        //stop shifting
  digitalHigh(LATCH);                                                                                       //enable latch and set address
}

//read a byte from RAM address
byte read_DSRAM( unsigned int address ) {
  byte data = 0x00;
  set_addressbus(address);                                                                                  //set address bus
  databus_input();                                                                                          //set databus for reading
  digitalLow(CE);                                                                                           //enable RAM chip select                               
  data = read_databus();                                                                                    //get databus value
  digitalHigh(CE);                                                                                          //disable RAM chip select
  return data;
}

//write a byte to RAM address
void write_DSRAM( unsigned int address, byte data ) {
  set_addressbus(address);                                                                                  //set address bus
  databus_output();                                                                                         //set databus for writing  
  write_databus(data);                                                                                      //set data bus value
  digitalLow(WE);                                                                                           //enable write
  digitalLow(CE);                                                                                           //enable RAM chip select
  digitalHigh(CE);                                                                                          //disable RAM chip select
  digitalHigh(WE);                                                                                          //disable write
  databus_input();                                                                                          //set databus to (default) input state
} 

//disable TI I/O, enable Arduino shift registers and control bus
//INLINE: need for speed in ISR
inline void TIstop() __attribute__((always_inline));
void TIstop( void ) {
  pinAsInput(TI_BUFFERS);                                                                                   //disables 74HCT541 address buffers (TI sees >FF and waits)
  pinAsOutput(AR_BUF);                                                                                      //enables 74HC595 shift registers
  enable_controlbus();                                                                                      //Arduino in control of RAM
} 

//enable TI I/O, disable Arduino shift registers and control bus
//INLINE: need for speed
inline void TIgo() __attribute__((always_inline));
void TIgo( void ) {
  disable_controlbus();                                                                                     //cease Arduino RAM control
  pinAsInputPullUp(AR_BUF);                                                                                       //disables 74HC595 shift registers
  digitalHigh(AR_BUF);
  pinAsOutput(TI_BUFFERS);                                                                                  //enables 74HCT541 address buffers (TI sees !FF and continues)
}

 
