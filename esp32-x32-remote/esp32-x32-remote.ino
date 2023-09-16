#include <LiquidCrystal_I2C.h>
#include <EthernetENC.h>
#include <EthernetUdp.h>
#include <SPI.h>    
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <Button2.h>
#include <ESPRotary.h>
#include <Preferences.h>

//LCD Display Parameter
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

//Rotary Encoder Pins
#define ROTARY_PIN1 27
#define ROTARY_PIN2	26
#define BUTTON_PIN	25

//Rotary Encoder Parameters
#define CLICKS_PER_STEP   4

//Initilize Preferences
Preferences prefs;

//Initialize Udp
EthernetUDP Udp;

//Display object
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

//Rotary and Button objects
ESPRotary r;
Button2 b;

enum state {
  HOME,
  MAIN_MENU,
  SUB_MENU,
  CHANNEL_SELECT_MENU,
  IP_EDIT_MENU,
  LOCK_MENU
};

enum state actualState = HOME;

enum type {
  PARENT,
  CALLBACK,
  ON_OFF,
  VALUE_EDIT,
  IP_EDITOR,
  CHANNEL_SELECTOR,
  PIN_EDITOR
};


//Menu Struct
struct MenuItem{
   char* name;      
   enum type itemType;          
   int value;            
   int ip[4];                        
   char* callback;  
};


#define MENU_ELEMENTS 4
const int totalMenuItems = MENU_ELEMENTS - 1;
int selectedMenuItem = 0;

MenuItem menu[MENU_ELEMENTS] {                     
/*  0   */    {"Select Channel",       CHANNEL_SELECTOR,   0,  {0,0,0,0},    "selectChannel"},
/*  1   */    {"Set Local IP",             IP_EDITOR,      0,  {10,0,1,2},   "setLocalIp"},
/*  2   */    {"Set Mixer IP",             IP_EDITOR,      0,  {10,0,1,1},   "setMixerIp"},
/*  3   */    {"Lock",                     PIN_EDITOR,     0,  {0,0,0,0},     "lock"},
};

void C_selectChannel(MenuItem);
void C_setLocalIp(MenuItem);
void C_setMixerIp(MenuItem);
void C_lock(MenuItem);

void (* callback_[])(MenuItem) = { &C_selectChannel, &C_setLocalIp, &C_setMixerIp, &C_lock };

byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };


IPAddress localIp   (10, 0, 1, 2);
IPAddress mixerIp   (10, 0, 1, 1);
int octet = 0;

bool ethUp = false;
bool linkUp = false;

int timeoutCount = 0;

const unsigned int localPort = 10023;
const unsigned int mixerPort = 10023;

unsigned long lastTime_home = 0UL;
unsigned long timerDelay_home = 600UL;

unsigned long lastTime_xremote = 0UL;
unsigned long timerDelay_xremote = 5000UL;

int batchNumber = 0;

OSCErrorCode error;

byte meter0[]     = {B00000,B00000,B00000,B00000,B00000,B00000,B11111,B11111};
byte meter1[]     = {B00000,B00000,B00000,B00000,B11111,B11111,B11111,B11111};
byte meter2[]     = {B00000,B00000,B11111,B11111,B11111,B11111,B11111,B11111};
byte meter3[]     = {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};
byte meterClip[]  = {B00000,B00110,B01000,B01000,B00110,B00000,B11111,B11111};

//Channel Struct
struct Channel{
   char* name;      
   char* type;
   int number;
   float faderValue;
   int on;
};

#define TOTAL_CHANNELS 72

const int totalChannels = TOTAL_CHANNELS - 1;
int selectedChannel = 0;

Channel x32Channel[TOTAL_CHANNELS] {                     
/*  0   */    {"Ch01", "/ch/", 1},
/*  1   */    {"Ch02", "/ch/", 2},
/*  2   */    {"Ch03", "/ch/", 3},
/*  3   */    {"Ch04", "/ch/", 4},
/*  4   */    {"Ch05", "/ch/", 5},
/*  5   */    {"Ch06", "/ch/", 6},
/*  6   */    {"Ch07", "/ch/", 7},
/*  7   */    {"Ch08", "/ch/", 8},
/*  8   */    {"Ch09", "/ch/", 9},
/*  9   */    {"Ch10", "/ch/", 10},
/*  10  */    {"Ch11", "/ch/", 11},
/*  11  */    {"Ch12", "/ch/", 12},
/*  12  */    {"Ch13", "/ch/", 13},
/*  13  */    {"Ch14", "/ch/", 14},
/*  14  */    {"Ch15", "/ch/", 15},
/*  15  */    {"Ch16", "/ch/", 16},
/*  16  */    {"Ch17", "/ch/", 17},
/*  17  */    {"Ch18", "/ch/", 18},
/*  18  */    {"Ch19", "/ch/", 19},
/*  19  */    {"Ch20", "/ch/", 20},
/*  20  */    {"Ch21", "/ch/", 21},
/*  21  */    {"Ch22", "/ch/", 22},
/*  22  */    {"Ch23", "/ch/", 23},
/*  23  */    {"Ch24", "/ch/", 24},
/*  24  */    {"Ch25", "/ch/", 25},
/*  25  */    {"Ch26", "/ch/", 26},
/*  26  */    {"Ch27", "/ch/", 27},
/*  27  */    {"Ch28", "/ch/", 28},
/*  28  */    {"Ch29", "/ch/", 29},
/*  29  */    {"Ch30", "/ch/", 30},
/*  30  */    {"Ch31", "/ch/", 31},
/*  31  */    {"Ch32", "/ch/", 32},
/*  32  */    {"Aux01", "/auxin/", 1},
/*  33  */    {"Aux02", "/auxin/", 2},
/*  34  */    {"Aux03", "/auxin/", 3},
/*  35  */    {"Aux04", "/auxin/", 4},
/*  36  */    {"Aux05", "/auxin/", 5},
/*  37  */    {"Aux06", "/auxin/", 6},
/*  38  */    {"Aux07", "/auxin/", 7},
/*  39  */    {"Aux08", "/auxin/", 8},
/*  40  */    {"FX01", "/fxrtn/", 1},
/*  41  */    {"FX02", "/fxrtn/", 2},
/*  42  */    {"FX03", "/fxrtn/", 3},
/*  43  */    {"FX04", "/fxrtn/", 4},
/*  44  */    {"FX05", "/fxrtn/", 5},
/*  45  */    {"FX06", "/fxrtn/", 6},
/*  46  */    {"FX07", "/fxrtn/", 7},
/*  47  */    {"FX08", "/fxrtn/", 8},
/*  48  */    {"Bus01", "/bus/", 1},
/*  49  */    {"Bus02", "/bus/", 2},
/*  50  */    {"Bus03", "/bus/", 3},
/*  51  */    {"Bus04", "/bus/", 4},
/*  52  */    {"Bus05", "/bus/", 5},
/*  53  */    {"Bus06", "/bus/", 6},
/*  54  */    {"Bus07", "/bus/", 7},
/*  55  */    {"Bus08", "/bus/", 8},
/*  56  */    {"Bus09", "/bus/", 9},
/*  57  */    {"Bus10", "/bus/", 10},
/*  58  */    {"Bus11", "/bus/", 11},
/*  59  */    {"Bus12", "/bus/", 12},
/*  60  */    {"Bus13", "/bus/", 13},
/*  61  */    {"Bus14", "/bus/", 14},
/*  62  */    {"Bus15", "/bus/", 15},
/*  63  */    {"Bus16", "/bus/", 16},
/*  64  */    {"Mtx01", "/mtx/", 1},
/*  65  */    {"Mtx02", "/mtx/", 2},
/*  66  */    {"Mtx03", "/mtx/", 3},
/*  67  */    {"Mtx04", "/mtx/", 4},
/*  68  */    {"Mtx05", "/mtx/", 5},
/*  69  */    {"Mtx06", "/mtx/", 6},
/*  70  */    {"Main",  "/main/st", 99},
/*  71  */    {"Mono",  "/main/m", 99},
};

short pin[4] = {0,0,0,0};
short currentPinDigit = 0;
bool isLocked = false;

void setup() {

  Serial.begin(9600);

  //Init LCD Display
  lcd.init();
  lcd.backlight();
  lcd.clear();

  //Loading preferences
  prefs.begin("x32-control");

  //Rotary Encoder
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);
  r.setLeftRotationHandler(showDirection);
  r.setRightRotationHandler(showDirection);

  //Push Button
  b.begin(BUTTON_PIN);
  b.setClickHandler(click);
  b.setLongClickTime(500);
  b.setLongClickHandler(longClick);
  
  lcd.createChar(0, meter0);
  lcd.createChar(1, meter1);
  lcd.createChar(2, meter2);
  lcd.createChar(3, meter3);
  lcd.createChar(4, meterClip);

  //Init Ethernet shield
  Ethernet.init(5);

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    printCenterString("No eth hardware!", 0);
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }

}

void loop() {

  //Home loop
  if ((millis() - lastTime_home) > timerDelay_home) {

    if (Ethernet.linkStatus() == LinkOFF) {
      ethUp = false;
      linkUp = false;
    }
    else if (Ethernet.linkStatus() == LinkON) {
      ethUp = true;
    }

    if (actualState == HOME) {
      homeRoutine();
    }
    lastTime_home = millis();
  }

  //Xremote loop
  if ((millis() - lastTime_xremote) > timerDelay_xremote) {
    if (actualState == HOME) {
      get_xremote();
      get_meters();
      //Serial.println("get meters");
    }
    lastTime_xremote = millis();
  }

  OSCMsgReceive();
  r.loop();
  b.loop();

}

// on change
void rotate(ESPRotary& r) {
}

// on left or right rotation
void showDirection(ESPRotary& r) {
  String direction = r.directionToString(r.getDirection());

  if (actualState == HOME) {
   
    if (direction == "right") {
      x32Channel[selectedChannel].faderValue = x32Channel[selectedChannel].faderValue - 0.01;
    }
    
    else if (direction == "left") {
      x32Channel[selectedChannel].faderValue = x32Channel[selectedChannel].faderValue + 0.01;
    }
    
    OSCMessage msg(composeOscCommand(x32Channel[selectedChannel].type, "/mix/fader", x32Channel[selectedChannel].number));
    msg.add(x32Channel[selectedChannel].faderValue);
    msg.send(Serial);
    Serial.println(" ");
    Udp.beginPacket(mixerIp, mixerPort);
    msg.send(Udp);
    Udp.endPacket();
    msg.empty();

    OSCMessage msg1(composeOscCommand(x32Channel[selectedChannel].type, "/mix/fader", x32Channel[selectedChannel].number));
    Udp.beginPacket(mixerIp, mixerPort);
    msg1.send(Udp);
    Udp.endPacket();
    msg1.empty();
  
  }
  
  //Cycle through Main Menu Items
  else if (actualState == MAIN_MENU) {
   
    if (direction == "right") {
      selectedMenuItem--;
      if (selectedMenuItem < 0) {selectedMenuItem = 0;}
      displayMainMenu(selectedMenuItem, direction);
    }
    
    if (direction == "left") {
      selectedMenuItem++;
      if (selectedMenuItem > totalMenuItems) {selectedMenuItem = totalMenuItems;}
      displayMainMenu(selectedMenuItem, direction);
    }
  
  }
  
  else if (actualState == CHANNEL_SELECT_MENU) {
   
    if (direction == "right") {
      selectedChannel--;
      if (selectedChannel < 0) {selectedChannel = totalChannels;}
      displayChannelSelectMenu(selectedChannel, direction);
    }
    
    if (direction == "left") {
      selectedChannel++;
      if (selectedChannel > totalChannels) {selectedChannel = 0;}
      displayChannelSelectMenu(selectedChannel, direction);
    }
  
  }

  else if (actualState == IP_EDIT_MENU) {

    if (direction == "right") {
      menu[selectedMenuItem].ip[octet]--;
      if (menu[selectedMenuItem].ip[octet] < 0) {menu[selectedMenuItem].ip[octet] = 0;}
      displayIpEditMenu();
    }
    
    if (direction == "left") {
      menu[selectedMenuItem].ip[octet]++;
      if (menu[selectedMenuItem].ip[octet] > 254) {menu[selectedMenuItem].ip[octet] = 254;}
      displayIpEditMenu();
    }
  
  }

  else if (actualState == LOCK_MENU) {

    if (direction == "right") {
      pin[currentPinDigit]--;
      if (pin[currentPinDigit] < 0) {pin[currentPinDigit] = 0;}
      displayLockMenu();
    }
    
    if (direction == "left") {
      pin[currentPinDigit]++;
      if (pin[currentPinDigit] > 9) {pin[currentPinDigit] = 9;}
      displayLockMenu();
    }
  
  }

}

// single click
void click(Button2& btn) {
  
  //Open Main Menu if pressed on Home Screen
  if (actualState == HOME) {
    selectedMenuItem = 0;
    actualState = MAIN_MENU;
    displayMainMenu(selectedMenuItem, "right");
  }

  //Open channel selector in menu if item type is channel selector
  else if (actualState == MAIN_MENU) {
    switch (menu[selectedMenuItem].itemType) {
      
      case CHANNEL_SELECTOR:
        actualState = CHANNEL_SELECT_MENU;
        displayChannelSelectMenu(selectedChannel, "right");
      break;

      case IP_EDITOR:
        actualState = IP_EDIT_MENU;
        displayIpEditMenu();
      break;

      case PIN_EDITOR:
        actualState = LOCK_MENU;
        displayLockMenu();
      break;

    }
  }
  
  else if (actualState == CHANNEL_SELECT_MENU) {
    callback_[selectedMenuItem](menu[selectedMenuItem]);
    actualState = HOME;
    batchNumber = 0;
    lcd.clear();
  }

  else if (actualState == IP_EDIT_MENU) {
    if (octet < 3) {
      octet++;
      displayIpEditMenu();
    }
    else {
      callback_[selectedMenuItem](menu[selectedMenuItem]);
      actualState = HOME;
      batchNumber = 0;
      lcd.clear();
    }
  }
  
  else if (actualState == LOCK_MENU) {
    if (currentPinDigit < 3) {
      currentPinDigit++;
      displayLockMenu();
    }
    else {
      callback_[selectedMenuItem](menu[selectedMenuItem]);
      actualState = HOME;
      batchNumber = 0;
      lcd.clear();
    }
  }

}

// long click
void longClick(Button2& btn) {

  if (actualState == HOME) {
    if (x32Channel[selectedChannel].on == 0) {
      x32Channel[selectedChannel].on = 1;
    }
    
    else if (x32Channel[selectedChannel].on == 1)  {
      x32Channel[selectedChannel].on = 0;
    }

    OSCMessage msg(composeOscCommand(x32Channel[selectedChannel].type, "/mix/on", x32Channel[selectedChannel].number));
    msg.add(x32Channel[selectedChannel].on);
    msg.send(Serial);
    Serial.println(" ");
    Udp.beginPacket(mixerIp, mixerPort);
    msg.send(Udp);
    Udp.endPacket();
    msg.empty();

    OSCMessage msg1(composeOscCommand(x32Channel[selectedChannel].type, "/mix/on", x32Channel[selectedChannel].number));
    Udp.beginPacket(mixerIp, mixerPort);
    msg1.send(Udp);
    Udp.endPacket();
    msg1.empty();

  }
    
  else if (actualState >= MAIN_MENU) {
    actualState = HOME;
    batchNumber = 0;
    //Back to Home
    lcd.clear();
  }
}

void get_xremote() {
    
  OSCMessage msg("/xremote");
  msg.send(Serial);
  Serial.println(" ");
  Udp.beginPacket(mixerIp, mixerPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

}

void get_meters() {
  OSCMessage msg("/meters");
  
  //Main LR
  if (selectedChannel == 70) { 
    msg.add("/meters/5");
    msg.add(3);
    msg.add(3);
    msg.add(10);
  }
  //All other channels
  else {
    msg.add("/meters/6");
    msg.add(selectedChannel);
    msg.add(0);
    msg.add(10);
  }

  msg.send(Serial);

  Udp.beginPacket(mixerIp, mixerPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  timeoutCount++;
}

void get_initial_data(int num) {

  lcd.setCursor(0, 0);
  lcd.print("     ");
  lcd.setCursor(0, 0);
  lcd.print(x32Channel[selectedChannel].name);
  
  char * message;

  switch (num) {
    case 0:
      message = composeOscCommand(x32Channel[selectedChannel].type, "/config/name", x32Channel[selectedChannel].number);
    break;
    case 1:
      message = composeOscCommand(x32Channel[selectedChannel].type, "/mix/fader", x32Channel[selectedChannel].number);
    break;
    case 2:
      message = composeOscCommand(x32Channel[selectedChannel].type, "/mix/on", x32Channel[selectedChannel].number);
    break;
  }

  OSCMessage msg(message);

  msg.send(Serial);
  Serial.println(" ");
  Udp.beginPacket(mixerIp, mixerPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  batchNumber++;

}

void info(OSCMessage &msg) {

  //msg.getString(2, mixer_model);
  //msg.getString(3, mixer_version);

  //lcd.setCursor(0, 0);
  //lcd.print(mixer_model);
  //lcd.setCursor(0, 1);
  //lcd.print(mixer_version);
    lcd.setCursor(15, 0);
    lcd.write(byte(3));

}

void channelMeter(OSCMessage &msg) {
  
  timeoutCount = 0;

  float meter1;
  float meter2;

  if (selectedChannel == 70) {
    byte blobBuffer[133];
    msg.getBlob(0, blobBuffer);

    meter1 = ((float *)blobBuffer)[25];
    meter2 = ((float *)blobBuffer)[26];
    //Serial.println(meter1);
    //Serial.println(meter2);
  }
  else {
    byte blobBuffer[5];
    msg.getBlob(0, blobBuffer);
    meter1 = 0;
    meter2 = ((float *)blobBuffer)[1];
  }
  
  if (actualState == HOME) {
    echoMeter(meter1, 0);
    echoMeter(meter2, 1);
  }

}

void inputOutputChannel(OSCMessage &msg, int addrOffset) {

  msg.send(Serial);
  Serial.println(" ");

  if (msg.match(composeOscCommand(x32Channel[selectedChannel].type, "/mix/fader", x32Channel[selectedChannel].number), 0) > 0) {
    
    if (msg.isFloat(0)) {
      float faderPosition = msg.getFloat(0);
      x32Channel[selectedChannel].faderValue = faderPosition;
      if (actualState == HOME) {
        
        lcd.setCursor(0, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(faderValueToDb(faderPosition));
      }
    }
  }
  else if (msg.match(composeOscCommand(x32Channel[selectedChannel].type, "/mix/on", x32Channel[selectedChannel].number), 0) > 0) {
    
    if (msg.isInt(0)) {
      int on = msg.getInt(0);
      x32Channel[selectedChannel].on = on;
      if (actualState == HOME) {
        lcd.setCursor(10, 1);
        lcd.print("    ");
        lcd.setCursor(10, 1);
        if (on == 0) { lcd.print("MUTE"); }
        if (on == 1) { lcd.print("    "); }
      }
    
    }
  }
  else if (msg.match(composeOscCommand(x32Channel[selectedChannel].type, "/config/name", x32Channel[selectedChannel].number), 0) > 0) {
    
    if (msg.isString(0)) {
      char name[10];
      msg.getString(0, name);
      if (actualState == HOME) {

        printRightString("         ", 13, 0);
        printRightString(String(name), 13, 0);

      }
    
    }
  }
}

void OSCMsgReceive() {
  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {

    while (size--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      //msg.send(Serial);
      msg.dispatch("/info", info);
      msg.route("/ch", inputOutputChannel);
      msg.route("/auxin", inputOutputChannel);
      msg.route("/fxrtn", inputOutputChannel);
      msg.route("/bus", inputOutputChannel);
      msg.route("/mtx", inputOutputChannel);
      msg.route("/main", inputOutputChannel);
      msg.dispatch("/meters/6", channelMeter);
      msg.dispatch("/meters/5", channelMeter);

    } else {
      int error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }
}


void printCenterString(String header, int line) {
  int len = header.length();
  lcd.setCursor((LCD_COLUMNS - len) / 2, line);
  lcd.print(header);
}
void printRightString(String header, int rightestChar, int line) {
  int len = header.length();
  lcd.setCursor(rightestChar-len+1, line);
  lcd.print(header);
}

void connectMixer() {
  linkUp = true;
  //Ethernet init
  Serial.println("Initialising Ethernet: "); 
  Ethernet.begin(mac, localIp);

  // print local IP address:
  Serial.println(Ethernet.localIP());
  Udp.begin(mixerPort);
  batchNumber = 0;

}

void homeRoutine() {
  
  if (!ethUp) {
    lcd.clear();
    printCenterString("No cable", 0);
  }

  else if (ethUp) {

    if (linkUp) {
      if (timeoutCount >= 2) { 
        lcd.clear();
        //linkUp = false;
        printCenterString("No connection!", 0);
        batchNumber = 0;
      }
      else if (batchNumber < 3) { 
        get_initial_data(batchNumber); 
      }
      //get_meters();
    }

    else if (!linkUp) {
      connectMixer();
    }

  }

}

void displayMainMenu(int selectedMenuItem, String direction) {
  lcd.clear();
  printCenterString("<MAIN MENU>", 0);
  printCenterString(menu[selectedMenuItem].name, 1);
}

void displayChannelSelectMenu(int selectedChannel, String direction) {
  lcd.clear();
  printCenterString("<Select Channel>", 0);
  printCenterString(x32Channel[selectedChannel].name, 1);
}

void displayIpEditMenu(void) {
  lcd.clear();
  printCenterString(menu[selectedMenuItem].name, 0);
  
  String ipString;
  for (int i = 0; i < 4; i++) {
    if (i == octet) { ipString = ipString + "<"; }
    ipString = ipString + String(menu[selectedMenuItem].ip[i]);
    if (i == octet) { ipString = ipString + ">"; }
    if (i < 3) { ipString = ipString + "."; }
  }
  printCenterString(ipString, 1);
}

void displayLockMenu(void) {
  lcd.clear();
  printCenterString("Choose PIN",0);
  //6
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(6+i,1);
    lcd.print(pin[i]);
  }
  lcd.setCursor(6+currentPinDigit,1);
  lcd.cursor();
}


char * composeOscCommand (char prefix[], char suffix[], int channel_number) {
  
  static char buff [40];
  char channel [3];

  String temp_str = String(channel_number);
  if (channel_number < 10) {
    temp_str = 0 + temp_str;
  }  
 
  temp_str.toCharArray(channel, 3);

  strcpy (buff, prefix);
  if (channel_number != 99) { strcat (buff, channel); }
  strcat (buff, suffix);

  return buff;

}

String faderValueToDb(float value) {

  const double stepConstant = 0.00625;
  float result;
  String resultString;

  if (value >= 0.5000 && value <= 1.0000) {
    result = (((0.7500 - value) / stepConstant) * 0.25)*(-1);
  }
  else if (value < 0.5000 && value >= 0.2500) {
    result = (((0.5000 - value) / stepConstant) * 0.50)*(-1)-10;
  }
  else if (value < 0.2500 && value >= 0.0625) {
    result = (((0.2500 - value) / stepConstant) * 1)*(-1)-30;
  }
  else if (value < 0.0625 && value >= 0) {
    result = (((0.0625 - value) / stepConstant) * 3)*(-1)-60;
  }
  
  char buff[5];
  dtostrf(result,3,1,buff);

  if (result == -90.0000) {
    resultString = "-Inf";
  }
  else {
    if (result > 0){
      resultString = "+" + String(buff);
    }
    else {
      resultString = String(buff);
    }
  }

  return resultString;

}

void clearMeter(int pos) {
  lcd.setCursor(14+pos,0);
  lcd.print(" ");
  lcd.setCursor(14+pos,1);
  lcd.print(" ");
}

void echoMeter(float value, int pos) {
  if (value >= 0.002 && value < 0.03) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(0);
  }
  else if (value >= 0.03 && value < 0.13) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(1);
  }
  else if (value >= 0.13 && value < 0.25) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(2);
  }
  else if (value >= 0.25 && value < 0.50) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(3);
  }
  else if (value >= 0.50 && value < 0.95) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(3);
    lcd.setCursor(14+pos,0);
    lcd.write(0);
  }
  else if (value >= 0.95) {
    clearMeter(pos);
    lcd.setCursor(14+pos,1);
    lcd.write(3);
    lcd.setCursor(14+pos,0);
    lcd.write(4);
  }
  else {
    clearMeter(pos);
  }
}

void C_selectChannel(MenuItem item) {
  lcd.clear();
  printCenterString(x32Channel[selectedChannel].name, 0);
  printCenterString("Selected", 1);
  delay(1500);
}

void C_setLocalIp(MenuItem item) {
  octet = 0;
  for (int i = 0; i < 4; i++) {
    localIp[i] = item.ip[i];
  }
  linkUp = false;
  lcd.clear();
  printCenterString("Local Ip Set", 0);
  delay(1500);
}

void C_setMixerIp(MenuItem item) {
  octet = 0;
  for (int i = 0; i < 4; i++) {
    mixerIp[i] = item.ip[i];
  }
  linkUp = false;
  lcd.clear();
  printCenterString("Mixer Ip Set", 0);
  delay(1500);
}

void C_lock(MenuItem item) {
  currentPinDigit = 0;
  lcd.clear();
  lcd.noCursor();
  printCenterString("Pin Set!", 0);
  delay(1500);
}
