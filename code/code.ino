#include <EEPROM.h>

/**
 * Test Ultrasound sensor
 */

// pins

const int TriggerPin = 2;
const int EchoPin = 3;
const int ButtonUpPin = 12;
const int ButtonDnPin = 11;
const int LedUpPin = 7;
const int LedDnPin = 6;
const int LedOkPin = 5;

// consts

const int buttonStatePk = 0; // Parking
const int buttonStateUp = 1; // Up
const int buttonStateDn = 2; // Down
const int buttonStateBt = 3; // Both
const int buttonStateCl = 4; // Clear

const long timeButtonClick = 1000; // 1/2 s
const long timeButtonHold = 3000; // 3s
const long timeButtonClear = 5000; // 5s

const long positionTolerance = 20; // 2cm

// vars

int buttonUpState = LOW;
int buttonUpStateLast = LOW;
int buttonDnState = LOW;
int buttonDnStateLast = LOW;
int buttonBtState = LOW;
int buttonBtStateLast = LOW;

unsigned long timeButtonUp = 0;
unsigned long timeButtonDn = 0;
unsigned long timeButtonBt = 0;
unsigned long timeButtonUpRelease = 0;
unsigned long timeButtonDnRelease = 0;
unsigned long timeButtonBtRelease = 0;

int buttonState;
int buttonStateLast;

int tablePosition = -1;
long positions[10];

// setup

void setup() {

    buttonState = buttonStatePk;
    buttonStateLast = buttonStatePk;

    pinMode(TriggerPin,OUTPUT);  // Trigger is an output pin
    pinMode(EchoPin,INPUT);      // Echo is an input pin
    pinMode(ButtonUpPin, INPUT);
    pinMode(ButtonDnPin, INPUT);
    pinMode(LedUpPin, OUTPUT);
    pinMode(LedDnPin, OUTPUT);
    pinMode(LedOkPin, OUTPUT);
    Serial.begin(9600);          // Serial Output
    Serial.println("SETUP");

    digitalWrite(LedUpPin, HIGH);
    digitalWrite(LedDnPin, HIGH);
    digitalWrite(LedOkPin, HIGH);

    delay(200);

    digitalWrite(LedUpPin, LOW);
    digitalWrite(LedDnPin, LOW);
    digitalWrite(LedOkPin, LOW);

    timeButtonUp = millis();
    timeButtonDn = millis();

    // init positions from eeprom
    int numberOfStoredValues = EEPROM.read(0);
    if(numberOfStoredValues < 0 || numberOfStoredValues > 10) {
        for (int i = 0 ; i < EEPROM.length() ; i++) {
            EEPROM.write(i, 0);
        }
        numberOfStoredValues = 0;
    }
    
    // debug positions
    Serial.print("stored positions ");
    Serial.println(numberOfStoredValues);
    
    GetPositions(numberOfStoredValues);
    
    for(int i=0;i<numberOfStoredValues;i++) {
        long pos = positions[i];
        Serial.print("pos ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(pos);
    }
    
    long currentHeight = GetDistance();
    tablePosition = GetTablePosition(currentHeight);

    Serial.print("table position ");
    Serial.println(tablePosition);
}

int ReadButtonState() {

    // read buttons, remember state changes and state change time

    buttonUpState = digitalRead(ButtonUpPin);
    buttonDnState = digitalRead(ButtonDnPin);

    if(buttonUpState == HIGH && buttonDnState == HIGH) {
        buttonBtState = HIGH;
    } else {
        buttonBtState = LOW;
    }

    // flanks button up

    if(buttonUpState == HIGH && buttonUpStateLast == LOW) {
        timeButtonUp = millis();
        Serial.println("   up");
    }
    if(buttonUpState == LOW && buttonUpStateLast == HIGH) {
        timeButtonUpRelease = millis();
        Serial.print("   up-released: ");
        Serial.println(timeButtonUpRelease-timeButtonUp);
    }

    // flanks button dn

    if(buttonDnState == HIGH && buttonDnStateLast == LOW) {
        timeButtonDn = millis();
        Serial.println("   down");
    }
    if(buttonDnState == LOW && buttonDnStateLast == HIGH) {
        timeButtonDnRelease = millis();
        Serial.print("   down-released: ");
        Serial.println(timeButtonDnRelease-timeButtonDn);
    }

    // flanks button both

    if(buttonBtState == HIGH && buttonBtStateLast == LOW) {
        timeButtonBt = millis();
        Serial.println("   both");
    }
    if(buttonBtState == LOW && buttonBtStateLast == HIGH) {
        timeButtonBtRelease = millis();
        Serial.print("   both-released: ");
        Serial.println(timeButtonBtRelease-timeButtonBt);
    }

    long int now = millis();

    // both release help

    if(buttonBtState == HIGH && now-timeButtonBt > timeButtonHold) {
        confirmSignal();
    }

    // button state

    if(buttonBtState == LOW && timeButtonBtRelease-timeButtonBt > timeButtonClear) {
        buttonState = buttonStateCl;
        resetButtons();
    } else if(buttonBtState == LOW && timeButtonBtRelease-timeButtonBt > timeButtonHold) {
        buttonState = buttonStateBt;
        resetButtons();
    } else if(buttonUpState == LOW && timeButtonUpRelease-timeButtonUp < timeButtonClick && timeButtonUpRelease > 0) {
        buttonState = buttonStateUp;
        confirmSignal();
        resetButtons();
    } else if(buttonDnState == LOW && timeButtonDnRelease-timeButtonDn < timeButtonClick && timeButtonDnRelease > 0) {
        buttonState = buttonStateDn;
        confirmSignal();
        resetButtons();
    } else {
        buttonState = buttonStatePk;
    }

    buttonUpStateLast = buttonUpState;
    buttonDnStateLast = buttonDnState;
    buttonBtStateLast = buttonBtState;
}

void ClearPosition(int slot) {
  
    Serial.print("delete position at slot: ");
    Serial.println(slot);

    // copy values from lastSlot to slot

    int lastSlot = EEPROM.read(0);    
    lastSlot--;
    
    if(slot < lastSlot) {
        int addr = lastSlot*2;
        addr++;
        int byte0 = EEPROM.read(addr);
        addr++;
        int byte1 = EEPROM.read(addr);

        Serial.print("read slot ");
        Serial.println(lastSlot);
        
        addr = slot*2;
        addr++;
        EEPROM.update(addr, byte0);
        addr++;
        EEPROM.update(addr, byte1);

        Serial.print("store slot ");
        Serial.println(slot);
    }
    
    EEPROM.update(0, lastSlot);
}

int StorePosition() {
    
    long dist = 0;
    for(int i=0;i<5;i++) {
        long currentHeight = GetDistance();
        Serial.print("read height(");
        Serial.print(i);
        Serial.print("): ");
        Serial.println(currentHeight);
        dist = dist + currentHeight;
        delay(50);
    }
    dist = dist / 5;

    Serial.print("store distance: ");
    Serial.println(dist);

    int byte0 = dist/10;
    long tmp = byte0*10;
    tmp = dist - tmp;
    int byte1 = (int)tmp;
    
    int slot = EEPROM.read(0);
    int addr = 0;
    
    addr = slot*2;    
    addr++;
    EEPROM.update(addr, byte0);
    addr++;
    EEPROM.update(addr, byte1);

    slot++;
    EEPROM.update(0, slot);

    return slot;
}

void GetPositions(int numberOfStoredValues) {
    for(int i=0;i<numberOfStoredValues;i++) {
        int addr = i*2;
        addr++;
        
        int byte0 = 0;
        int byte1 = 0;
        byte0 = EEPROM.read(addr);
        addr++;
        byte1 = EEPROM.read(addr);
        
        long pos = byte0*10;
        pos = pos + byte1;

        positions[i] = pos;
    }
}

void loop() {

    ReadButtonState();

    if(buttonState != buttonStateLast) {
        
        Serial.print("buttonState: ");
        Serial.println(buttonState);
        
        switch(buttonState) {          
            case buttonStateUp :
                tableMove(1);
                break;
            case buttonStateDn :
                tableMove(-1);
                break;
            case buttonStateBt :
                HandlePos();
                break;
            case buttonStateCl :
                ResetPositions();
                break;
        }
    }

    buttonStateLast = buttonState;
}

void ResetPositions() {
    Serial.println("reset positions");
    for(int i=0;i<EEPROM.length();i++) {
        EEPROM.write(i, 0);
    }
    tablePosition = -1;
    // reset positions
    for(int i=0;i<10;i++) {
        positions[i] = 0;
    }
}

void HandlePos() {
    if(tablePosition < 0) {
        tablePosition = StorePosition();
        tablePosition--;
        Serial.print("store at new position nr. ");
        Serial.println(tablePosition);
    } else {
        ClearPosition(tablePosition);
    }
    int numberOfStoredValues = EEPROM.read(0);
    GetPositions(numberOfStoredValues);
}

// 200ms
void confirmSignal() {
    for(int i=0;i<5;i++) {
        digitalWrite(LedOkPin, HIGH);
        delay(20);
        digitalWrite(LedOkPin, LOW);
        delay(20);
    }
}

void resetButtons() {
    timeButtonBt = 0;
    timeButtonBtRelease = 0;
    timeButtonUp = 0;
    timeButtonUpRelease = 0;
    timeButtonDn = 0;
    timeButtonDnRelease = 0;
}

long GetDistance()
{
    digitalWrite(TriggerPin, LOW);
    delayMicroseconds(2);
    digitalWrite(TriggerPin, HIGH);          // Trigger pin to HIGH
    delayMicroseconds(10);                   // 10us high 
    digitalWrite(TriggerPin, LOW);           // Trigger pin to HIGH

    long Duration = pulseIn(EchoPin,HIGH);
    long Distance = ((Duration / 2.9) / 2);

    return Distance;
}

int GetTablePosition(long currentHeight) {    
    int numberOfStoredValues = EEPROM.read(0);
    for(int i=0;i<numberOfStoredValues;i++) {
        long pos = positions[i];
        long diff = currentHeight-pos;
        diff = abs(diff);
        if(diff < positionTolerance) {
            return i;
        }
    }
    return -1;
}

// -1=dn, 1=up
void tableMove(int direction) {

    int ledDirPin;
    
    if(direction > 0) {
        ledDirPin = LedUpPin;
        Serial.println("table up");
    } else {
        ledDirPin = LedDnPin;
        Serial.println("table down");
    }

    digitalWrite(ledDirPin, HIGH);

    // current table pos
    Serial.print("table at pos ");
    Serial.println(tablePosition);

    int drive = 1;
    long distLast = GetDistance();
    int zeroDistCounter = 0;
    
    long distAvg[] = {distLast, distLast, distLast, distLast, distLast};
    long distAvgSlot = 0;
    long distAvgResult = 0;
    
    while(drive == 1) {

        delay(50);

        // take 5 readings, average
        
        long dist = GetDistance();        
        
        distAvg[distAvgSlot] = dist;
        distAvgResult = distAvg[0] + distAvg[1] + distAvg[2] + distAvg[3] + distAvg[4];
        distAvgResult = distAvgResult / 5;

        //Serial.print(distAvgResult);
        //Serial.print(" mm avg (");
        //Serial.print(dist);
        //Serial.println(" mm height current read)");

        int drove = distAvgResult-distLast;
        drove = drove*direction;
        //Serial.print(drove);
        //Serial.println(" mm driven");

        distLast = distAvgResult;
        
        if(drove <= 0) {
            zeroDistCounter++;
        } else {
            zeroDistCounter--;
            zeroDistCounter = max(0, zeroDistCounter);
        }
        if(zeroDistCounter == 10) {
            drive = 2;
        }
        //Serial.print("zdc ");
        //Serial.println(zeroDistCounter);

        ReadButtonState();
        if(buttonState == buttonStateUp || buttonState == buttonStateDn) {
            Serial.println("button clicked");
            buttonState = 0;
            drive = 3;
        }

        int currentPos = GetTablePosition(distAvgResult);
        if(currentPos != tablePosition && currentPos > -1) {
            drive = 4;
            tablePosition = currentPos;
            Serial.print("table reached position ");
            Serial.println(tablePosition);
        }
        
        distAvgSlot++;
        distAvgSlot = distAvgSlot % 5;
    }
    
    // stopped on button press(1) or end position(2)
    // determine current position
    if(drive == 2 || drive == 3) {
        tablePosition = GetTablePosition(distAvgResult);
    }

    Serial.println("table stop");
    digitalWrite(ledDirPin, LOW);
}
