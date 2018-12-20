//RFID Tag Unlocker
//RFID reader + screen + relay (not many IO, but can use D1,D0 and A5, should be OK if outputs. TX/RX leds are driven by 16u2, so should be ignored)
//relay on A5 so it doens't chatter. D0 for RFID RST, as it idles high anyway
//master user is first user and only has settings access (can't unlock)
//save settings to eeprom 8bytes card, 8 bytes pin, 14 bytes username, 2 byte permissions (allow access on card, allow access on pin) gives 32 users on Uno

#include "XC4630d.c"
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
char hex[]="0123456789ABCDEF";

#define RST_PIN 0       
#define SS_PIN 10       
#define USERCOUNT 32
#define RELAYTIME 5000

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

int bx[] ={25,95,165,25,95,165,25,95,165,25,95,165};
int by[] ={10,10,10,70,70,70,130,130,130,190,190,190};
char bb[]="123456789#0*";
char kb[]="1234567890QWERTYUIOPASDFGHJKL'ZXCVBNM .<";   //soft keyboard
byte tstate[12]={0,0,0,0,0,0,0,0,0,0,0,0};
byte ltstate[12]={0,0,0,0,0,0,0,0,0,0,0,0};
char pin[10]="";
byte cardbytes[8];                                  //for reading routine
byte mcardbytes[8];                                 //for setting routine

void setup() {
  SPI.begin();              //start SPI
  mfrc522.PCD_Init();       //start RC522 module
  pinMode(0,INPUT);         //use serial pullup to hold high
  XC4630_init();
  XC4630_rotate(1);
  XC4630_clear(BLACK);          //Blank screen
  int e=0;
  for(int i=0;i<16;i++){e=e+EEPROM.read(i);}       //check EEPROM contents (255=blank)
  if(e==16*255){dosetup();XC4630_clear(BLACK);}    //first time setup if no master user set  
  for(int i=0;i<12;i++){dobutton(i,WHITE,GREY,50);}
  XC4630_chara(0,260,"ENTER PIN:",GREY,BLACK);
  XC4630_chara(0,280,"OR SWIPE CARD.",GREY,BLACK);  
}

void loop() {
  char a;
  int s;
  a=checktouch();
  s=strlen(pin);
  if(a=='#'){if(s){pin[s-1]=0;s--;a=0;}}                //erase last character of pin
  if(a=='*'){dopin();a=0;s=0;pin[0]=0;}                 //pin entered, process, clear
  if(a){pin[s]=a;pin[s+1]=0;s++;}
  for(int i=0;i<8;i++){XC4630_char(120+i*12,260,((i<s)?'*':' '),GREY,BLACK);}
  if(checkcard()){
    XC4630_chara(108,280,"CARD",BLACK,WHITE);
    docard();                                   //process card
    delay(100);
  }else{
    XC4630_chara(108,280,"CARD",GREY,BLACK);
  }
}

void dopin(){
  int umatch=-1;                //user to be matched
  byte match;                   //flag for mismatch
  for(int i=0;i<USERCOUNT;i++){
    match=1;
    for(int n=0;n<8;n++){
      if(EEPROM.read(i*32+8+n)!=pin[n]){match=0;}   //mismatch found, clear
    }
    if(EEPROM.read(i*32+31)==0){match=0;}           //pin not allowed for this user
    if(match){umatch=i;}                            //flag matched user
  }
  if(umatch==0){domaster();return;}              //master user matched, do master routine  
  if(umatch>0){  
    dounlock(umatch);                               //do unlock routine for user
  }else{
    XC4630_chara(0,300,"PIN ERROR",RED,BLACK);      //error message
    delay(1000);  
    XC4630_chara(0,300,"PIN ERROR",BLACK,BLACK);
  }
}

void dounlock(int u){             //unlock and display welcome message
  char uname[16]="";
  pinMode(A5,OUTPUT);
  digitalWrite(A5,HIGH);
  for(int i=0;i<14;i++){uname[i]=EEPROM.read(u*32+16+i);}
  uname[14]=0;
  XC4630_box(0,250,239,319,BLACK);
  XC4630_chara(0,260,"UNLOCK",GREEN,BLACK);
  XC4630_chara(0,280,uname,GREEN,BLACK);  
  delay(RELAYTIME);
  digitalWrite(A5,LOW);
  pinMode(A5,INPUT);
  XC4630_box(0,250,239,319,BLACK);
  XC4630_chara(0,260,"ENTER PIN:",GREY,BLACK);
  XC4630_chara(0,280,"OR SWIPE CARD.",GREY,BLACK);  
}

void docard(){
  int umatch=-1;                //user to be matched
  byte match;                   //flag for mismatch
  for(int i=0;i<USERCOUNT;i++){
    match=1;
    for(int n=0;n<8;n++){
      if(EEPROM.read(i*32+n)!=cardbytes[n]){match=0;}   //mismatch found, clear
    }
    if(EEPROM.read(i*32+30)==0){match=0;}           //pin not allowed for this user
    if(match){umatch=i;}                            //flag matched user
  }
  if(umatch==0){domaster();return;}              //master user matched, do master routine  
  if(umatch>0){  
    dounlock(umatch);                               //do unlock routine for user
  }else{
    XC4630_chara(0,300,"CARD ERROR",RED,BLACK);      //error message
    delay(1000);  
    XC4630_chara(0,300,"CARD ERROR",BLACK,BLACK);
  }
}

void domaster(){                                  //for master user to setup other users
  int u=1;            //user to start editing
  byte done=0;        //flag to say we've finished
  XC4630_clear(BLACK);          //Blank screen
  drawuserinfo(u);
  while(!done){
    if(XC4630_istouch(165,65,235,95)){editusername(u);drawuserinfo(u);delay(100);}                //edit username
    if(XC4630_istouch(5,145,75,175)){EEPROM.write(u*32+30,0);drawuserinfo(u);delay(100);}         //disable card
    if(XC4630_istouch(85,145,155,175)){EEPROM.write(u*32+30,1);drawuserinfo(u);delay(100);}       //enable card
    if(XC4630_istouch(165,145,235,175)){editcard(u);drawuserinfo(u);delay(100);}                //edit card #
    if(XC4630_istouch(5,225,75,255)){EEPROM.write(u*32+31,0);drawuserinfo(u);delay(100);}         //disable pin
    if(XC4630_istouch(85,225,155,255)){EEPROM.write(u*32+31,1);drawuserinfo(u);delay(100);}       //enable pin
    if(XC4630_istouch(165,225,235,255)){editpin(u);drawuserinfo(u);delay(100);}                   //edit PIN
    if(XC4630_istouch(5,265,75,305)){u=u-1;if(u<0){u=USERCOUNT-1;}XC4630_clear(BLACK);drawuserinfo(u);delay(100);}    //previous
    if(XC4630_istouch(85,265,155,305)){u=u+1;if(u>USERCOUNT-1){u=0;}XC4630_clear(BLACK);drawuserinfo(u);delay(100);}  //next
    if(XC4630_istouch(165,265,235,305)){done=1;}                                                  //exit
  }
  XC4630_clear(BLACK);          //Blank screen
  for(int i=0;i<12;i++){dobutton(i,WHITE,GREY,50);}
  XC4630_chara(0,260,"ENTER PIN:",GREY,BLACK);
  XC4630_chara(0,280,"OR SWIPE CARD.",GREY,BLACK);  
}

void editusername(int u){
  int done=0;
  char uname[16]="";
  char c;
  int s;
  for(int i=0;i<14;i++){uname[i]=EEPROM.read(u*32+16+i);if(uname[i]<0){uname[i]=0;}}   //load current name, change 0xFF to null
  uname[14]=0;
  XC4630_clear(BLACK);          //Blank screen
  XC4630_chara(0,0,"TYPE USERNAME:",WHITE,BLACK);
  XC4630_tbox(5,145,115,175,"CANCEL",WHITE,GREY,2);
  XC4630_tbox(125,145,235,175,"ACCEPT",WHITE,GREY,2);
  drawkeyboard();
  while(!done){
    s=strlen(uname);
    XC4630_chara(0,20,uname,GREY,BLACK);    
    if((millis()/300)&1){XC4630_chara(s*12,20,"_  ",GREY,BLACK);}else{XC4630_chara(s*12,20,"   ",GREY,BLACK);} //flashing cursor
    c=checkkeyboard();
    if(c=='<'){if(s){uname[s-1]=0;s--;}c=0;}     //backspace
    if(c){uname[s]=c;s++;uname[s]=0;}           //add character
    uname[14]=0;                                //limit length
    if(XC4630_istouch(5,145,115,175)){XC4630_clear(BLACK);return;}   //no change on cancel
    if(XC4630_istouch(125,145,235,175)){for(int i=0;i<14;i++){EEPROM.write(u*32+16+i,uname[i]);}XC4630_clear(BLACK);return;}
  }
}

void editcard(int u){   //swipe new card- need to check if it matches an existing one before validating
  byte cardset=0;
  byte match;                   //flag for mismatch
  int umatch=-1;
  XC4630_clear(BLACK);          //Blank screen
  cardset=getcard();                                                              //get a card, returns 0 if no card selected
  for(int i=0;i<USERCOUNT;i++){
    match=1;
    for(int n=0;n<8;n++){
      if(EEPROM.read(i*32+n)!=mcardbytes[n]){match=0;}   //mismatch found, clear
    }
    if(match){umatch=i;}                            //flag matched user
  }
  if(umatch>=0){cardset=0;XC4630_clear(BLACK);XC4630_chara(0,150,"CARD ALREADY IN USE!",RED,BLACK);delay(2000);XC4630_clear(BLACK);}  //card already used
  if(cardset){for(int i=0;i<8;i++){EEPROM.write(i+u*32, mcardbytes[i]);}}              //copy to EEPROM 
  XC4630_clear(BLACK);          //Blank screen
}

void editpin(int u){    //enter new PIN- need to check if it matches an existing one before validating
  byte pinset=0;
  byte match;                   //flag for mismatch
  int umatch=-1;
  XC4630_clear(BLACK);          //Blank screen
  pinset=getpin();                                                                //get a pin, returns 0 if no pin entered/cancelled
    for(int i=0;i<USERCOUNT;i++){
    match=1;
    for(int n=0;n<8;n++){
      if(EEPROM.read(i*32+n+8)!=pin[n]){match=0;}   //mismatch found, clear
    }
    if(match){umatch=i;}                            //flag matched user
  }
  if(umatch>=0){pinset=0;XC4630_clear(BLACK);XC4630_chara(8,150,"PIN ALREADY IN USE!",RED,BLACK);delay(2000);XC4630_clear(BLACK);}  //PIN already used
  if(pinset){for(int i=0;i<8;i++){EEPROM.write(i+8+u*32, pin[i]);}}           //copy to EEPROM
  for(int i=0;i<8;i++){pin[i]=0;}                                             // clear array for main program
  XC4630_clear(BLACK);          //Blank screen
}

void drawuserinfo(int u){
  int e;
  char uname[16]="";
  for(int i=0;i<14;i++){uname[i]=EEPROM.read(u*32+16+i);}
  uname[14]=0;
  XC4630_chara(0,0,"USER INFO:",WHITE,BLACK);
  XC4630_char(120,0,(u/10)%10+'0',WHITE,BLACK);
  XC4630_char(132,0,(u)%10+'0',WHITE,BLACK);
  XC4630_chara(0,20,"Name:",WHITE,BLACK);
  XC4630_chara(0,40,uname,WHITE,BLACK);
  e=0;
  for(int i=0;i<8;i++){e=e+EEPROM.read(u*32+i);}      //check if card set or all 0xFF
  XC4630_chara(0,100,"Card:",WHITE,BLACK);
  if(e==8*0xFF){XC4630_chara(0,120,"NOT SET",RED,BLACK);}else{XC4630_chara(0,120,"SET    ",GREEN,BLACK);}
  if(EEPROM.read(u*32+30)){XC4630_chara(120,120,"ACTIVE  ",GREEN,BLACK);}else{XC4630_chara(120,120,"DISABLED",RED,BLACK);}
  e=0;
  for(int i=0;i<8;i++){e=e+EEPROM.read(u*32+i+8);}      //check if pin set or all 0xFF
  XC4630_chara(0,180,"PIN:",WHITE,BLACK);
  if(e==8*0xFF){XC4630_chara(0,200,"NOT SET",RED,BLACK);}else{XC4630_chara(0,200,"SET    ",GREEN,BLACK);}
  if(EEPROM.read(u*32+31)){XC4630_chara(120,200,"ACTIVE  ",GREEN,BLACK);}else{XC4630_chara(120,200,"DISABLED",RED,BLACK);}
  XC4630_tbox(165,65,235,95,"EDIT",WHITE,GREY,2);     //edit username
  XC4630_tbox(5,145,75,175,"DISABLE",WHITE,GREY,1);      //edit Card
  XC4630_tbox(85,145,155,175,"ENABLE",WHITE,GREY,1);      //edit Card
  XC4630_tbox(165,145,235,175,"EDIT",WHITE,GREY,2);      //edit Card
  XC4630_tbox(5,225,75,255,"DISABLE",WHITE,GREY,1);      //edit PIN
  XC4630_tbox(85,225,155,255,"ENABLE",WHITE,GREY,1);      //edit PIN
  XC4630_tbox(165,225,235,255,"EDIT",WHITE,GREY,2);      //edit PIN
  XC4630_tbox(5,265,75,305,"PREVIOUS",WHITE,GREY,1);      //previous user
  XC4630_tbox(85,265,155,305,"NEXT",WHITE,GREY,1);      //next user
  XC4630_tbox(165,265,235,305,"EXIT",WHITE,GREY,2);      //done
}

void dosetup(){
  byte cardset=0;
  byte pinset=0;
  XC4630_chara(0,0," MASTER USER SETUP  ",WHITE,RED_1*8);                         //warning for master setup
  cardset=getcard();                                                              //get a card, returns 0 if no card selected
  XC4630_clear(BLACK);          //Blank screen
  XC4630_chara(0,0," MASTER USER SETUP  ",WHITE,RED_1*8);                         //warning for master setup
  pinset=getpin();                                                                //get a pin, returns 0 if no pin entered/cancelled
  if(cardset){for(int i=0;i<8;i++){EEPROM.write(i, mcardbytes[i]);}}              //copy to EEPROM
  if(pinset){for(int i=0;i<8;i++){EEPROM.write(i+8, pin[i]);pin[i]=0;}}           //copy to EEPROM, clear array for main program
  EEPROM.write(30,cardset);         //write card permission
  EEPROM.write(31,pinset);          //write card permission  
}

byte getcard(){       //get a swiped card for setup
  byte done=0;
  byte cardset=0;
  for(int i=0;i<8;i++){mcardbytes[i]=0;}
  XC4630_chara(8,30,"Swipe a card to set",GREY,BLACK);
  XC4630_tbox(125,90,235,120,"CANCEL",RED,GREY,2);
  while(!done){
    if(checkcard()){          //valid card swiped
      for(int i=0;i<8;i++){
        mcardbytes[i]=cardbytes[i];
        XC4630_char(i*30+4,60,hex[mcardbytes[i]>>4],WHITE,BLACK);
        XC4630_char(i*30+16,60,hex[mcardbytes[i]&15],WHITE,BLACK);
      }
      cardset=1;
      XC4630_tbox(5,90,115,120,"USE CARD",WHITE,GREY,2);
    }
    if(XC4630_istouch(5,90,115,120)&&cardset){done=1;}                                  //keep card value
    if(XC4630_istouch(125,90,235,120)){for(int i=0;i<8;i++){mcardbytes[i]=0;}done=1;cardset=0;}   //clear card value    
  }
  return cardset;
}

byte getpin(){        //get a typed pin for setup
  byte done=0;
  byte pinset=0;
  for(int i=0;i<12;i++){dobutton(i,WHITE,GREY,50);}   //draw buttons
  XC4630_chara(0,260,"ENTER PIN:",WHITE,BLACK);
  XC4630_tbox(125,289,235,319,"CANCEL",RED,GREY,2);
  for(int i=0;i<8;i++){pin[i]=0;}
  while(!done){
    char a;
    int s;
    a=checktouch();
    s=strlen(pin);
    if(a=='#'){if(s){pin[s-1]=0;s--;a=0;}}                //erase last character of pin
    if(a=='*'){a=0;}                                      //ignore (use button for OK)
    if(a){pin[s]=a;pin[s+1]=0;s++;}
    for(int i=0;i<8;i++){XC4630_char(120+i*12,260,pin[i],GREY,BLACK);}
    if(s<4){pinset=0;}else{pinset=1;}                                 //minimum PIN length
    XC4630_tbox(5,289,115,319,"USE PIN",pinset*WHITE,pinset*GREY,2);
    if(XC4630_istouch(5,289,115,319)&&pinset){done=1;}                //keep pin
    if(XC4630_istouch(125,289,235,319)){for(int i=0;i<8;i++){pin[i]=0;}done=1;pinset=0;}   //clear pin value
  }
  return pinset;
}

void drawkeyboard(){
  char t[]=" ";
  for(int i=0;i<40;i++){
    t[0]=kb[i];
    XC4630_tbox((i%10)*24,200+(i/10)*30,(i%10)*24+22,228+(i/10)*30,t,BLACK,GREY,3);
  }
}

char checkkeyboard(){
  int x,y;
  static char lastn=0;
  char n;
  n=0;
  x=XC4630_touchx();
  y=XC4630_touchy();
  if((x>0)&&(y>200)){
    n=kb[x/24+((y-200)/30)*10];
  }
  if((lastn==0)&&(n)){lastn=n;return n;}
  lastn=n;
  return 0;
}

int checkcard(){
  for(byte i=0;i<8;i++){cardbytes[i]=0;}
  if (!mfrc522.PICC_IsNewCardPresent()){return 0;}      //no card found
  if (!mfrc522.PICC_ReadCardSerial()){return 0;}        //no card found
  for(byte i = 0; i < mfrc522.uid.size; i++){
    if(i<8){cardbytes[i]=mfrc522.uid.uidByte[i];}
  }
  mfrc522.PICC_HaltA();
  return 1;                                             //card ID read
}

void dobutton(int n,unsigned int f,unsigned int b, int s){
  char t[]=" ";   //single char array
  t[0]=bb[n];
  XC4630_tbox(bx[n],by[n],bx[n]+s-1,by[n]+s-1,t,f,b,6);
}

char checktouch(){                     //returns touched button
  char key=0;
  for(int i=0;i<12;i++){
    tstate[i]=XC4630_istouch(bx[i],by[i],bx[i]+59,by[i]+59);
    delay(5);
    tstate[i]=XC4630_istouch(bx[i],by[i],bx[i]+59,by[i]+59)&tstate[i];    //debounce
    if(tstate[i]!=ltstate[i]){
      if(tstate[i]){
        dobutton(i,BLACK,WHITE,50);
        key=bb[i];                                                         //key pressed
      }else{
        dobutton(i,WHITE,GREY,50);
      }
    }
    ltstate[i]=tstate[i];
  }
  return key;
}

