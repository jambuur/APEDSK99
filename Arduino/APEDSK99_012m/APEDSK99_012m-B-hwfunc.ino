//-DSR generic---------------------------------------------------------------------------------- Hardware functions

//short delay function to let bus/signals settle. 
//6us is the minumum stable value on my TI but your mileage may vary
inline void NOP() __attribute__((always_inline));
void NOP() {
  //uncomment for 74LS541
  //delayMicroseconds(3);
  //uncomment for 74HCT541
  delayMicroseconds(8);  
}

//databus:
//D0,0,PD0 / D1,1,PD1 / D2,4,PD4 / D3,5,PD5 / D4,6,PD6 / D5,7,PD7 / D6,8,PD8 / D7,9,PD9
//switch databus to INPUT state for reading from RAM

void dbus_in() {
  DDRD  &= B00001100;  //set PD7-PD4 and PD1-PD0 to input (D5-D2, D1-D0)
  PORTD &= B00001100;  //initialise input pins
  DDRB  &= B11111100;  //set PB1-PB0 to input (D7-D6)
  PORTB &= B11111100;  //initialise input pins
}

//switch databus to OUTPUT state for writing to RAM
void dbus_out() {
  DDRD  |= B11110011;  //set PD7-PD4 and PD1-PD0 to output (D5-D2, D1-D0)
  DDRB  |= B00000011;  //set PB1-PB0 to output (D7-D6)
}

//disable Arduino control bus; CE* and WE* both HighZ
void dis_cbus() {
  pinAsInput(CE);
  pinAsInput(WE);
}

//enable Arduino control bus; CE* and WE* both HIGH due to 5.1K pull-ups
void ena_cbus() {
  pinAsOutput(CE);
  pinAsOutput(WE);
}

//read a byte from the databus
byte dbus_read()
{
  NOP();                                    //long live the Logic Analyzer
  return ( ((PIND & B00000011)     ) +      //read PD1,PD0 (D1-D0)
           ((PIND & B11110000) >> 2) +      //read PD7-PD3 (D5-D1)     
           ((PINB & B00000011) << 6) );     //read PB1-PBO (D7-D6)
}

//place a byte on the databus
void dbus_write(byte data)
{
  PORTD |= ((data     ) & B00000011);       //write PD1 (D0)
  PORTD |= ((data << 2) & B11110000);       //write PD7-PD3 (D5-D1)
  PORTB |= ((data >> 6) & B00000011);       //write PB1-PBO (D7-D6)
}

//shift out the given address to the 74HC595 registers
void set_abus(unsigned int address)
{
  //disable latch line
  digitalLow(LATCH);
  //clear data pin
  digitalLow(DS);

  //OK, repetitive, slightly ugly code but ... 1.53x as fast as the more elegant for() - if() - else()
  //for every address bit (13 bits to address 8Kbytes) set:
  //CLOCK -> LOW, address bit -> DS bit, CLOCK -> HIGH to shift and DS bit -> LOW to prevent bleed-through
  digitalLow(CLOCK);
  PORTC |= ( (address >>  7) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A12
  digitalLow(CLOCK);
  PORTC |= ( (address >>  6) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A11
  digitalLow(CLOCK);
  PORTC |= ( (address >>  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A10
  digitalLow(CLOCK);
  PORTC |= ( (address >>  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A9
  digitalLow(CLOCK);
  PORTC |= ( (address >>  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A8
  digitalLow(CLOCK);
  PORTC |= ( (address >>  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A7
  digitalLow(CLOCK);
  PORTC |= ( (address >>  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A6
  digitalLow(CLOCK);
  PORTC |= ( (address      ) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A5
  digitalLow(CLOCK);
  PORTC |= ( (address <<  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A4
  digitalLow(CLOCK);
  PORTC |= ( (address <<  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A3
  digitalLow(CLOCK);
  PORTC |= ( (address <<  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A2
  digitalLow(CLOCK);
  PORTC |= ( (address <<  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A1
  digitalLow(CLOCK);
  PORTC |= ( (address <<  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A0

  //stop shifting
  digitalLow(CLOCK);
  //enable latch and set address
  digitalHigh(LATCH);
}

//read a byte from RAM address
byte Rbyte(unsigned int address)
{
  byte data = 0x00;
  //set address bus
  set_abus(address);
  //set databus for reading
  dbus_in();
  //enable RAM chip select
  digitalLow(CE);
  //get databus value
  data = dbus_read();
  //disable RAM chip select
  digitalHigh(CE);
  return data;
}

//write a byte to RAM address
void Wbyte(unsigned int address, byte data)
{
  //set address bus
  set_abus(address);
  //set databus for writing
  dbus_out();
  //set data bus value
  dbus_write(data);
  //enable write
  digitalLow(WE);
  //enable RAM chip select
  digitalLow(CE);
  //disable chip select
  digitalHigh(CE);
  //disable write
  digitalHigh(WE);
  //set databus to (default) input state
  dbus_in();
}

//enable TI I/O, disable Arduino shift registers and control bus
//INLINE: need for speed
inline void TIgo() __attribute__((always_inline));
void TIgo()
{
  dis_cbus();               //cease Arduino RAM control
  pinAsOutput(TI_BUFFERS);  //enable 74LS541's 
  EIFR = bit (INTF0);       // clear flag for interrupt 0 
  EIMSK |= B00000001;       //enable INT0
  pinAsInput(TI_READY);     //switch to HIGH: disables 74HC595's and wakes up TI
}

//disable TI I/O, enable Arduino shift registers and control bus
//INLINE: need for speed in ISR
inline void TIstop() __attribute__((always_inline));
void TIstop() {
  EIMSK &= B11111110;         //disable INT0
  pinAsOutput(TI_READY);      //bring READY line LOW (halt TI)
  NOP();                      //long live the Logic Analyzer
  pinAsInput(TI_BUFFERS);     //disables 74LS541's    
  ena_cbus();                 //Arduino in control of RAM
}
