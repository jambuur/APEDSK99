//*****Interrupt routine; puts TI on hold and sets flag

//INT0 on pin 2
ISR( INT0_vect ) {

  TIstop();                                   //TI, hold your horses
  FD1771 = true;                              //set interrupt flag
 
}
