//---------------------------------------------------------------------------------------------------- Interrupt routine; puts TI on hold and sets flag
//Interrupt Service Routine (INT0 on pin 2)
ISR(INT0_vect) {

  TIstop();
   
  //set interrupt flag
  FD1771 = true;
}
