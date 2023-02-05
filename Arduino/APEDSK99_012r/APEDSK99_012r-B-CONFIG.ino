//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
//Short delay function to let bus/signals settle. 
//Probably not necessary in most cases but conservative timing ensures most consoles work with APEDSK99 "out of the box"
inline void NOP() __attribute__((always_inline));
void NOP(){
  delayMicroseconds(6);  
}

//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
//Short delay function to let TI finish the Instruction Acquisition (IAQ) cycle before READY is made inactive
inline void IAQ() __attribute__((always_inline));
void IAQ(){
  delayMicroseconds(4);  
}
