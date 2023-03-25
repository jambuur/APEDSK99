//74HC595 shift-out definitions
#define CLOCK     17                                                          //PC3
#define LATCH     18                                                          //PC4
#define DS        19                                                          //PC5

//IO lines for Arduino RAM control
#define CE  14                                                                //PC0  LED flashes for both Arduino and TI RAM access
#define WE  16                                                                //PC2

//IO lines for TI99/4a control
#define TI_READY       3                                                      //PD0; manage READY and enable/disable 74HC595 shift registers
#define TI_INT         2                                                      //PD2; GAL16V8 flag (DSR write & A15); shared with Ethernet CS (ETH_CS) 
#define TI_BUFFERS    15                                                      //PC1; 74HCT541 and GAL output bufers enable/disable

//error blinking parameters
#define LED_ON       500                                                      //on
#define LED_OFF      250                                                      //off
#define LED_REPEAT  1500                                                      //delay between sequence

//Short delay function to let bus/signals settle. 
//Probably not necessary in most cases but conservative timing ensures most consoles work with APEDSK99 "out of the box"
inline void NOP() __attribute__((always_inline));
void NOP(){
  delayMicroseconds(6);  
}

//Short delay function to let TI finish the Instruction Acquisition (IAQ) cycle before READY is made inactive
inline void IAQ() __attribute__((always_inline));
void IAQ(){
  delayMicroseconds(4);  
}

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
  digitalLow(CE);                                                                                           //enable RAM chip select
  digitalLow(WE);                                                                                           //enable write
  NOP();                                                                                                    //let databus stabilise
  digitalHigh(WE);                                                                                          //disable write
  digitalHigh(CE);                                                                                          //disable RAM chip select
  databus_input();                                                                                          //set databus to (default) input state 
} 

//disable TI I/O, enable Arduino shift registers and control bus
//INLINE: need for speed
inline void TIstop() __attribute__((always_inline));
void TIstop( void ) {   
  IAQ();                                                                                                    //wait for TI to finish IAQ cycle
  digitalLow(TI_READY);                                                                                     //halts TI and enables 74HC595 shift registers
  digitalHigh(TI_BUFFERS);                                                                                  //disables 74HCT541 address buffers and GAL outputs
  enable_controlbus();                                                                                      //Arduino in control of RAM
  NOP();                                                                                                    //wait for bus to settle
}

//enable TI I/O, disable Arduino shift registers and control bus
//INLINE: need for speed
inline void TIgo() __attribute__((always_inline));
void TIgo( void ) {
  disable_controlbus();                                                                                     //cease Arduino RAM control  
  NOP();                                                                                                    //wait for bus to settle
  digitalLow(TI_BUFFERS);                                                                                   //enables 74HCT541 address buffers and GAL outputs  
  digitalHigh(TI_READY);                                                                                    //disables 74HC595 shift registers and wakes up TI                                                                     
} 

//reset Arduino properly via watchdog timer
void APEDSK99reset ( void ) {
  wdt_disable();                                                                                      //disable Watchdog (just to be sure)                              
  wdt_enable(WDTO_15MS);                                                                              //enable it and set delay to 15ms
  while( true ) {}                                                                                    //self-destruct in 15ms
}

//unrecoverable errors: flash error code until reset (TI still usable)
//  flash             : SPI, SD Card fault / not ready
//  flash-flash       : no valid DSR binary image
//  flash-flash-flash : no valid DSR header (0xAA) at DSR RAM 0x0000)
void Flasher( byte errorcode ) {                                                                      //error routine: stuck in code flashing loop until reset
  //"no APEDSK99 for you"
  TIstop();                                                                                           //stop TI ...
  digitalHigh(TI_READY);                                                                              //... but let user enjoy vanilla console
  
  while ( true ) { 
    for ( byte flash = 0; flash < errorcode; flash++ ) {
      digitalLow(CE);                                                                                 //set RAM CE* LOW, turns on LED
      delay(LED_ON);                                                                                  //LED is on for a bit
      
      digitalHigh(CE);                                                                                //turn it off
      delay(LED_OFF);                                                                                 //LED is off for a bit
    }
    delay(LED_REPEAT);                                                                                //allow human interpretation
  }
}
