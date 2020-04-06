//---------------------------------------------------------------------------------------------------- Interrupt routine; puts TI on hold and sets flag
//Interrupt Service Routine (INT0 on pin 2)
ISR(INT0_vect) {

  TIstop();
   
  //set interrupt flag
  FD1771 = true;
}

/* DEBUG snippets
 *  
            for ( byte ii = 0; ii < 3; ii++ ) {
              Wbyte(aDEBUG + ii, aDSK[ii+1]);
              Wbyte(aDEBUG + 3 + ii, pDSK[ii+1]);
            }
 *  
 *  for ( byte ii = 0; ii < 20; ii++ ) {
              Wbyte(aDEBUG + ii, mDOAD[ii]);
            }
 */
