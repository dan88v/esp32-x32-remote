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
};

enum state actualState = HOME;

enum type {
  PARENT,
  CALLBACK,
  ON_OFF,
  VALUE_EDIT,
  IP_EDIT,
  CHANNEL_SELECTOR
};


//Menu Struct
struct MenuItem{
   char* name;      
   enum type itemType;
   int parent;            
   int value1;            
   int value2;            
   int value3;            
   int value4;            
   int min;               
   int max;               
   const char* callback;  
};


#define MENU_ELEMENTS 3
const int totalMenuItems = MENU_ELEMENTS - 1;
int selectedMenuItem = 0;

MenuItem menu[MENU_ELEMENTS] {                     
/*  0   */    {"Select Channel",       CHANNEL_SELECTOR,    0,  1,  0,   0, 0, 1, 32,  "selectChannel"},
/*  2   */    {"Set Local IP",             IP_EDIT,       1,  10,  0,  1, 2, 1, 254,   "setLocalIp"},
/*  3   */    {"Set Mixer IP",             IP_EDIT,       1,  10,  0,  1, 1, 1, 254,   "setMixerIp"},
};

void C_selectChannel(MenuItem);
void C_setLocalIp(MenuItem);
void C_setMixerIp(MenuItem);

void (* callback_[])(MenuItem) = { &C_selectChannel, &C_setLocalIp, &C_setMixerIp };

byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress localIp   (10, 0, 1, 2);
IPAddress mixerIp   (10, 0, 1, 1);
bool ethUp = false;
bool linkUp = false;

const unsigned int localPort = 10023;
const unsigned int mixerPort = 10023;

unsigned long lastTime_home = 0UL;
unsigned long timerDelay_home = 500UL;

unsigned long lastTime_xremote = 0UL;
unsigned long timerDelay_xremote = 5000UL;

int batchNumber = 0;

OSCErrorCode error;

byte meter0[] = {B00000,B00000,B00000,B00000,B00000,B00000,B11111,B11111};
byte meter1[] = {B00000,B00000,B00000,B00000,B11111,B11111,B11111,B11111};
byte meter2[] = {B00000,B00000,B11111,B11111,B11111,B11111,B11111,B11111};
byte meter3[] = {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};
byte meterClip[] = {B11111,B11111,B11111,B11111,B11111,B00000,B11111,B11111};

//Channel Struct
struct Channel{
   char* name;      
   char* type;
   int number;
   float faderValue;
   int on;
};

#define TOTAL_CHANNELS 71

const int totalChannels = TOTAL_CHANNELS - 1;
int selectedChannel = 1;

Channel x32Channel[TOTAL_CHANNELS] {                     
/*  0   */    {"", "", 0},
/*  1   */    {"Ch01", "/ch/", 1},
/*  2   */    {"Ch02", "/ch/", 2},
/*  3   */    {"Ch03", "/ch/", 3},
/*  4   */    {"Ch04", "/ch/", 4},
/*  5   */    {"Ch05", "/ch/", 5},
/*  6   */    {"Ch06", "/ch/", 6},
/*  7   */    {"Ch07", "/ch/", 7},
/*  8   */    {"Ch08", "/ch/", 8},
/*  9   */    {"Ch09", "/ch/", 9},
/*  10  */    {"Ch10", "/ch/", 10},
/*  11  */    {"Ch11", "/ch/", 11},
/*  12  */    {"Ch12", "/ch/", 12},
/*  13  */    {"Ch13", "/ch/", 13},
/*  14  */    {"Ch14", "/ch/", 14},
/*  15  */    {"Ch15", "/ch/", 15},
/*  16  */    {"Ch16", "/ch/", 16},
/*  17  */    {"Ch17", "/ch/", 17},
/*  18  */    {"Ch18", "/ch/", 18},
/*  19  */    {"Ch19", "/ch/", 19},
/*  20  */    {"Ch20", "/ch/", 20},
/*  21  */    {"Ch21", "/ch/", 21},
/*  22  */    {"Ch22", "/ch/", 22},
/*  23  */    {"Ch23", "/ch/", 23},
/*  24  */    {"Ch24", "/ch/", 24},
/*  25  */    {"Ch25", "/ch/", 25},
/*  26  */    {"Ch26", "/ch/", 26},
/*  27  */    {"Ch27", "/ch/", 27},
/*  28  */    {"Ch28", "/ch/", 28},
/*  29  */    {"Ch29", "/ch/", 29},
/*  30  */    {"Ch30", "/ch/", 30},
/*  31  */    {"Ch31", "/ch/", 31},
/*  32  */    {"Ch32", "/ch/", 32},
/*  33  */    {"Aux01", "/auxin/", 1},
/*  34  */    {"Aux02", "/auxin/", 2},
/*  35  */    {"Aux03", "/auxin/", 3},
/*  36  */    {"Aux04", "/auxin/", 4},
/*  37  */    {"Aux05", "/auxin/", 5},
/*  38  */    {"Aux06", "/auxin/", 6},
/*  39  */    {"Aux07", "/auxin/", 7},
/*  40  */    {"Aux08", "/auxin/", 8},
/*  41  */    {"FX01", "/fxrtn/", 1},
/*  42  */    {"FX02", "/fxrtn/", 2},
/*  43  */    {"FX03", "/fxrtn/", 3},
/*  44  */    {"FX04", "/fxrtn/", 4},
/*  45  */    {"FX05", "/fxrtn/", 5},
/*  46  */    {"FX06", "/fxrtn/", 6},
/*  47  */    {"FX07", "/fxrtn/", 7},
/*  48  */    {"FX08", "/fxrtn/", 8},
/*  49  */    {"Bus01", "/bus/", 1},
/*  50  */    {"Bus02", "/bus/", 2},
/*  51  */    {"Bus03", "/bus/", 3},
/*  52  */    {"Bus04", "/bus/", 4},
/*  53  */    {"Bus05", "/bus/", 5},
/*  54  */    {"Bus06", "/bus/", 6},
/*  55  */    {"Bus07", "/bus/", 7},
/*  56  */    {"Bus08", "/bus/", 8},
/*  57  */    {"Bus09", "/bus/", 9},
/*  58  */    {"Bus10", "/bus/", 10},
/*  59  */    {"Bus11", "/bus/", 11},
/*  60  */    {"Bus12", "/bus/", 12},
/*  61  */    {"Bus13", "/bus/", 13},
/*  62  */    {"Bus14", "/bus/", 14},
/*  63  */    {"Bus15", "/bus/", 15},
/*  64  */    {"Bus16", "/bus/", 16},
/*  65  */    {"Mtx01", "/mtx/", 1},
/*  66  */    {"Mtx02", "/mtx/", 2},
/*  67  */    {"Mtx03", "/mtx/", 3},
/*  68  */    {"Mtx04", "/mtx/", 4},
/*  69  */    {"Mtx05", "/mtx/", 5},
/*  70  */    {"Mtx06", "/mtx/", 6},
};

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
      if (selectedChannel < 1) {selectedChannel = 1;}
      displayChannelSelectMenu(selectedChannel, direction);
    }
    
    if (direction == "left") {
      selectedChannel++;
      if (selectedChannel > totalChannels) {selectedChannel = totalChannels;}
      displayChannelSelectMenu(selectedChannel, direction);
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
    }
  }
  
  else if (actualState == CHANNEL_SELECT_MENU) {
    callback_[selectedMenuItem](menu[selectedMenuItem]);
    actualState = HOME;
    batchNumber = 0;
    lcd.clear();
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
  msg.add("/meters/6");
  //msg.add(48);
  msg.add(selectedChannel-1);
  msg.add(0);
  msg.add(10);
  msg.send(Serial);
  Serial.println(" ");
  Udp.beginPacket(mixerIp, mixerPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
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
  byte blobBuffer[5];
  msg.getBlob(0, blobBuffer);
  //Serial.println(((float *)blobBuffer)[4]);
  float meter = ((float *)blobBuffer)[1];
  
  if (actualState == HOME) {
    //lcd.setCursor(12, 0);
    //lcd.print(meter);
    echoMeter(meter);
  }

  //Serial.print(" ");
  //Serial.print(float(blobBuffer[4]));
  //Serial.print(" ");
  //Serial.println(float(blobBuffer[5]));
  //Serial.print(" ");
  //Serial.print(blobBuffer[3]);
  //Serial.print(" ");
  //Serial.println(blobBuffer[4]);
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
        //lcd.setCursor(sizeof(x32Channel[selectedChannel].name)+1, 0);
        //lcd.print("         ");
        //lcd.setCursor(sizeof(x32Channel[selectedChannel].name)+1, 0);
        //lcd.print(name);
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
  //Ethernet init
  Serial.println("Initialising Ethernet: "); 
  Ethernet.begin(mac, localIp);

  // print local IP address:
  Serial.println(Ethernet.localIP());
  Udp.begin(mixerPort);
  linkUp = true;
  batchNumber = 0;

}

void homeRoutine() {
  
  if (!ethUp) {
    lcd.clear();
    printCenterString("No cable", 0);
  }

  else if (ethUp) {

    if (linkUp) {
      if (batchNumber < 3) { get_initial_data(batchNumber); }
      get_meters();
    }

    else if (!linkUp) {
      connectMixer();
      lcd.clear();
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


char * composeOscCommand (char prefix[], char suffix[], int channel_number) {
  
  static char buff [40];
  char channel [3];

  String temp_str = String(channel_number);
  if (channel_number < 10) {
    temp_str = 0 + temp_str;
  }  
 
  temp_str.toCharArray(channel, 3);

  strcpy (buff, prefix);
  strcat (buff, channel);
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

void clearMeter() {
  lcd.setCursor(15,0);
  lcd.print(" ");
  lcd.setCursor(15,1);
  lcd.print(" ");
}

void echoMeter(float value) {
  if (value >= 0.002 && value < 0.03) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(0);
  }
  else if (value >= 0.03 && value < 0.13) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(1);
  }
  else if (value >= 0.13 && value < 0.25) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(2);
  }
  else if (value >= 0.25 && value < 0.50) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(3);
  }
  else if (value >= 0.50 && value < 0.95) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(3);
    lcd.setCursor(15,0);
    lcd.write(0);
  }
  else if (value >= 0.95) {
    clearMeter();
    lcd.setCursor(15,1);
    lcd.write(3);
    lcd.setCursor(15,0);
    lcd.write(4);
  }
  else {
    clearMeter();
  }
}

void C_selectChannel(MenuItem item) {
  lcd.clear();
  printCenterString(x32Channel[selectedChannel].name, 0);
  printCenterString("Selected", 1);
  delay(1500);
}

void C_setLocalIp(MenuItem item) {
  lcd.clear();
  delay(1500);
}

void C_setMixerIp(MenuItem item) {
  lcd.clear();
  delay(1500);
}
