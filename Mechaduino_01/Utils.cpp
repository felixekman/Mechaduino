//Contains the definitions of the functions used by the firmware.


#include <SPI.h>
#include <Wire.h>

#include <Arduino.h>
#include "Parameters.h"
#include "Controller.h"
#include "Utils.h"
#include "State.h"
#include "analogFastWrite.h"

void setupPins() {
  pinMode(VREF_2, OUTPUT);
  pinMode(VREF_1, OUTPUT);
  pinMode(IN_4, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_1, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(step_pin, INPUT);
  pinMode(dir_pin, INPUT);
  pinMode(ena_pin, INPUT_PULLUP);
  pinMode(chipSelectPin, OUTPUT); // CSn -- has to toggle high and low to signal chip to start data transfer

  attachInterrupt(step_pin, stepInterrupt, RISING);
  attachInterrupt(ena_pin, enaInterrupt, CHANGE);
  attachInterrupt(dir_pin, dirInterrupt, CHANGE);

  REG_PORT_OUTSET0 = PORT_PA20;
  //digitalWrite(IN_4, HIGH);
  REG_PORT_OUTCLR0 = PORT_PA15;
  //digitalWrite(IN_3, LOW);
  REG_PORT_OUTSET0 = PORT_PA21;
  //digitalWrite(IN_2, HIGH);
  REG_PORT_OUTCLR0 = PORT_PA06;
  //digitalWrite(IN_1, LOW);

}

void setupSPI() {
  SPISettings settingsA(10000000, MSBFIRST, SPI_MODE1);             ///400000, MSBFIRST, SPI_MODE1);

  SPI.begin();    //AS5047D SPI uses mode=1 (CPOL=0, CPHA=1)
  delay(1000);
  SPI.beginTransaction(settingsA);

}


void stepInterrupt() {
  if (enabled) {
    if (dir) {
      r = r + stepangle;
    }
    else {
      r = r - stepangle;
    }
  }
}

void dirInterrupt() {
  if (digitalRead(dir_pin)) {
    dir = false;
  }
  else {
    dir = true;
  }
}


void enaInterrupt() {
  if (digitalRead(ena_pin) == 1) {
    enabled = false;
  }
  else {
    enabled = true;
  }
}


void output(float theta, int effort) {
  static float angle;
  static float floatangle;

  angle = (10000 * theta * 0.87266 );

  floatangle = angle + 23562;
  //floatangle = angle + 7854;

  val1 = effort * lookup_sine((int)floatangle);

  analogFastWrite(VREF_2, abs(val1));

  if (val1 >= 0)  {
    REG_PORT_OUTSET0 = PORT_PA20;
    //digitalWrite(IN_4, HIGH);

    REG_PORT_OUTCLR0 = PORT_PA15;
    //digitalWrite(IN_3, LOW);
  }
  else  {
    REG_PORT_OUTCLR0 = PORT_PA20;
    //digitalWrite(IN_4, LOW);

    REG_PORT_OUTSET0 = PORT_PA15;
    //digitalWrite(IN_3, HIGH);
  }

  floatangle = angle + 7854;
  //floatangle = angle + 23562;

  val2 = effort * lookup_sine((int)floatangle);

  analogFastWrite(VREF_1, abs(val2));

  if (val2 >= 0)  {
    REG_PORT_OUTSET0 = PORT_PA21;
    //digitalWrite(IN_2, HIGH);

    REG_PORT_OUTCLR0 = PORT_PA06;
    //igitalWrite(IN_1, LOW);
  }
  else  {
    REG_PORT_OUTCLR0 = PORT_PA21;
    //digitalWrite(IN_2, LOW);

    REG_PORT_OUTSET0 = PORT_PA06;
    // digitalWrite(IN_1, HIGH);
  }
}

void commandW() {

  int encoderReading = 0;     //or float?  not sure if we can average for more res?
  int lastencoderReading = 0;
  int avg = 10;         //how many readings to average

  int iStart = 0;
  int jStart = 0;
  int stepNo = 0;

  int fullStepReadings[steps_per_revolution];
  int fullStep = 0;
  //  float newLookup[counts_per_revolution];
  int ticks = 0;

  float lookupAngle = 0.0;

  encoderReading = readEncoder();
  dir = 1;
  oneStep();
  delay(500);

  if ((readEncoder() - encoderReading) < 0)
  {
    //dir = 0;
    SerialUSB.println("Wired backwards");
    return;
  }

  while (stepNumber != 0) {
    if (stepNumber > 0) {
      dir = 1;
    }
    else
    {
      dir = 0;
    }
    oneStep();
    delay(100);
  }
  dir = 1;
  for (int x = 0; x < steps_per_revolution; x++) {

    encoderReading = 0;
    delay(100);

    for (int reading = 0; reading < avg; reading++) {
      encoderReading += readEncoder();
      delay(10);
    }

    encoderReading = encoderReading / avg;

    anglefloat = encoderReading * 0.02197265625;
    fullStepReadings[x] = encoderReading;
    SerialUSB.println(fullStepReadings[x], DEC);
    oneStep();
  }
  SerialUSB.println(" ");
  SerialUSB.println("ticks:");
  SerialUSB.println(" ");
  for (int i = 0; i < steps_per_revolution; i++) {
    ticks = fullStepReadings[mod((i + 1), steps_per_revolution)] - fullStepReadings[mod((i), steps_per_revolution)];
    if (ticks < -15000) {
      ticks += counts_per_revolution;
    }
    else if (ticks > 15000) {
      ticks -= counts_per_revolution;
    }

    SerialUSB.println(ticks);

    if (ticks > 1) {
      for (int j = 0; j < ticks; j++) {
        stepNo = (mod(fullStepReadings[i] + j, counts_per_revolution));
        // SerialUSB.println(stepNo);
        if (stepNo == 0) {
          iStart = i;
          jStart = j;
        }
      }
    }

    if (ticks < 1) {
      for (int j = -ticks; j > 0; j--) {
        stepNo = (mod(fullStepReadings[steps_per_revolution - 1 - i] + j, counts_per_revolution));
        // SerialUSB.println(stepNo);
        if (stepNo == 0) {
          iStart = i;
          jStart = j;
        }

      }
    }
  }




  SerialUSB.println(" ");
  SerialUSB.println("newLookup:");
  SerialUSB.println(" ");

  for (int i = iStart; i < (iStart + steps_per_revolution + 1); i++) {
    ticks = fullStepReadings[mod((i + 1), steps_per_revolution)] - fullStepReadings[mod((i), steps_per_revolution)];

    if (ticks < -15000) {
      ticks += counts_per_revolution;

    }
    else if (ticks > 15000) {
      ticks -= counts_per_revolution;
    }
    //SerialUSB.println(ticks);

    if (ticks > 1) {

      if (i == iStart) {
        for (int j = jStart; j < ticks; j++) {
          lookupAngle = 0.001 * mod(1000 * ((angle_per_step * i) + ((angle_per_step * j ) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }

      else if (i == (iStart + steps_per_revolution)) {
        for (int j = 0; j < jStart; j++) {
          lookupAngle = 0.001 * mod(1000 * ((angle_per_step * i) + ((angle_per_step * j ) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }
      else {
        for (int j = 0; j < ticks; j++) {
          lookupAngle = 0.001 * mod(1000 * ((angle_per_step * i) + ((angle_per_step * j ) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }



    }

    else if (ticks < 1) {
      if (i == iStart) {
        for (int j = - ticks; j > (jStart); j--) {
          lookupAngle = 0.001 * mod(1000 * (angle_per_step * (i) + (angle_per_step * ((ticks + j)) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }
      else if (i == iStart + steps_per_revolution) {
        for (int j = jStart; j > 0; j--) {
          lookupAngle = 0.001 * mod(1000 * (angle_per_step * (i) + (angle_per_step * ((ticks + j)) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }
      else {
        for (int j = - ticks; j > 0; j--) {
          lookupAngle = 0.001 * mod(1000 * (angle_per_step * (i) + (angle_per_step * ((ticks + j)) / float(ticks))), 360000.0);
          SerialUSB.print(lookupAngle);
          SerialUSB.print(" , ");
        }
      }

    }


  }
  SerialUSB.println(" ");


}


void serialCheck() {
  if (SerialUSB.peek() != -1) {

    char inChar = (char)SerialUSB.read();

    switch (inChar) {
      case 'c':
        commandW();           //cal routine
        break;

      case 's':             //new setpoint
        setpoint();
        break;

      case 'p':
        parameterQuery();     // prints copy-able parameters
        break;
        
      case 'e':
        parameterEdit();
        break;

      case 'a':             //anticogging
        antiCoggingCal();
        break;

      case 'j':
        step_response();
        break;

      default:
        break;
    }
  }

}
void Serial_menu(){
  SerialUSB.println("");
  SerialUSB.println("");
  SerialUSB.println("----- Mechaduino 0.1 -----");
  SerialUSB.print("Identifier: ");
  SerialUSB.println(identifier);
  SerialUSB.println("");
  SerialUSB.println("");
  SerialUSB.println("Main menu");
  SerialUSB.println("c  -  calibration");
  SerialUSB.println("s  -  setpoint");
  SerialUSB.println("p  -  print parameter");
  SerialUSB.println("e  -  edit parameter ");
  SerialUSB.println("a  -  anticogging");
  SerialUSB.println("j  -  setp response");  
  SerialUSB.println("");
}

void setpoint() {
  SerialUSB.print("current Setpoint: ");
  SerialUSB.println(yw);

  SerialUSB.println("Enter setpoint:");

  while (SerialUSB.peek() == -1)  {}
  r = SerialUSB.parseFloat();

  SerialUSB.print("new Setpoint: ");
  SerialUSB.println(r);
}

void parameterQuery() {
  SerialUSB.println(' ');
  SerialUSB.println("----Current Parameters-----");
  SerialUSB.println(' ');

  SerialUSB.print("volatile float pKp = ");
  SerialUSB.print(pKp, 4);
  SerialUSB.println(';');

  SerialUSB.print("volatile float pKi = ");
  SerialUSB.print(pKi, 4);
  SerialUSB.println(';');

  SerialUSB.print("volatile float pKd = ");
  SerialUSB.print(pKd, 4);
  SerialUSB.println(';');

}


float lookup_angle(int n)
{
  float a_out;
  a_out = pgm_read_float_near(lookup + n);
  return a_out;
}

void oneStep() {           /////////////////////////////////   oneStep    ///////////////////////////////

  if (dir == 0) {
    stepNumber += 1;
  }
  else {
    stepNumber -= 1;
  }

  output(1.8 * stepNumber, 64); //1.8 = 90/50

  delay(10);
}

int readEncoder()           //////////////////////////////////////////////////////   READENCODER   ////////////////////////////
{
  long angleTemp;
  digitalWrite(chipSelectPin, LOW);

  byte b1 = SPI.transfer(0xFF);
  byte b2 = SPI.transfer(0xFF);

  angleTemp = (((b1 << 8) | b2) & 0B0011111111111111);

  digitalWrite(chipSelectPin, HIGH);

  return angleTemp;
}


void receiveEvent(int howMany)
{
  while (1 < Wire.available()) // loop through all but the last
  {
    char c = Wire.read(); // receive byte as a character
    SerialUSB.print(c);         // print the character
  }
  int x = Wire.read();    // receive byte as an integer
  SerialUSB.println(x);         // print the integer
  r = 0.1 * ((float)x);
}

int mod(int xMod, int mMod) {
  return (xMod % mMod + mMod) % mMod;
}


float lookup_sine(int m)        /////////////////////////////////////////////////  LOOKUP_SINE   /////////////////////////////
{
  float b_out;

  m = (0.01 * (((m % 62832) + 62832) % 62832)) + 0.5; //+0.5 for rounding

  //SerialUSB.println(m);

  if (m > 314) {
    m = m - 314;
    b_out = -pgm_read_float_near(sine_lookup + m);

  }
  else
  {
    b_out = pgm_read_float_near(sine_lookup + m);
  }

  return b_out;
}


void setupTCInterrupts() {
  // Enable GCLK for TC4 and TC5 (timer counter input clock)
  GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while (GCLK->STATUS.bit.SYNCBUSY);

  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;   // Disable TCx
  WAIT_TC16_REGS_SYNC(TC5)                      // wait for sync

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;   // Set Timer counter Mode to 16 bits
  WAIT_TC16_REGS_SYNC(TC5)

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ; // Set TC as normal Normal Frq
  WAIT_TC16_REGS_SYNC(TC5)

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1;   // Set perscaler
  WAIT_TC16_REGS_SYNC(TC5)


  TC5->COUNT16.CC[0].reg = overflow;//0x3E72;
  WAIT_TC16_REGS_SYNC(TC5)


  TC5->COUNT16.INTENSET.reg = 0;              // disable all interrupts
  TC5->COUNT16.INTENSET.bit.OVF = 1;          // enable overfollow
  TC5->COUNT16.INTENSET.bit.MC0 = 1;         // enable compare match to CC0


  NVIC_SetPriority(TC5_IRQn, 1);


  // Enable InterruptVector
  NVIC_EnableIRQ(TC5_IRQn);
}


void enableTCInterrupts() {
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;    //Enable TC5
  WAIT_TC16_REGS_SYNC(TC5)                      //wait for sync
}

void disableTCInterrupts() {
  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;   // Disable TC5
  WAIT_TC16_REGS_SYNC(TC5)                      // wait for sync
}


void antiCoggingCal() {
  SerialUSB.println(" -----------------BEGIN ANTICOGGING CALIBRATION!----------------");
  r = lookup_angle(1);
  enableTCInterrupts();
  delay(1000);


  for (int i = 1; i < 657; i++) {
    r = lookup_angle(i);
    SerialUSB.print(r, 4);
    SerialUSB.print(" , ");
    delay(100);
    SerialUSB.println(u, 4);
  }
  SerialUSB.println(" -----------------REVERSE!----------------");

  for (int i = 656; i > 0; i--) {
    r = lookup_angle(i);
    SerialUSB.print(r, 4);
    SerialUSB.print(" , ");
    delay(100);
    SerialUSB.println(u, 4);
  }
  SerialUSB.println(" -----------------DONE!----------------");
  disableTCInterrupts();
}


void parameterEdit() {
  int received_1 = 0;
  int received_2 = 0;

  SerialUSB.println();
  SerialUSB.println("---- Edit position loop gains: ----");
  SerialUSB.println();
  SerialUSB.print("p ----- pKp = ");
  SerialUSB.println(pKp, 4);

  SerialUSB.print("i ----- pKi = ");
  SerialUSB.println(pKi, 4);

  SerialUSB.print("d ----- pKd = ");
  SerialUSB.println(pKd, 4);

  SerialUSB.println("q ----- quit");
  SerialUSB.println();


  while (received_1 == 0)  {
    delay(100);
    char inChar2 = (char)SerialUSB.read();

    switch (inChar2) {
      case 'p':
        {
          SerialUSB.println("enter new pKp!");
          while (received_2 == 0) {
            delay(100);
            if (SerialUSB.peek() != -1) {
              pKp = SerialUSB.parseFloat();
              received_2 = 1;
            }
          }
          received_1 = 1;
        }
        break;
      case 'i':
        {
          SerialUSB.println("enter new pKi!");
          while (received_2 == 0) {
            delay(100);
            if (SerialUSB.peek() != -1) {
              pKi = SerialUSB.parseFloat();
              received_2 = 1;
            }
          }
          received_1 = 1;
        }
        break;
      case 'd':
        {
          SerialUSB.println("enter new pKd!");
          while (received_2 == 0) {
            delay(100);
            if (SerialUSB.peek() != -1) {
              pKd = SerialUSB.parseFloat();
              received_2 = 1;
            }
          }
          received_1 = 1;
        }
        break;
      case 'q':
        {
          SerialUSB.println("quit");
          received_1 = 1;
        }
        break;
    }
  }
  SerialUSB.println();
  SerialUSB.println();
  parameterQuery();
}


void step_response() {

  float current_position = yw;

  unsigned long start_millis = millis();

  r = current_position;
  SerialUSB.print(micros());
  SerialUSB.print(',');
  SerialUSB.println(jump);

  while (millis() < (start_millis + 1700)) { //half a second

    SerialUSB.print(micros());
    SerialUSB.print(',');
    SerialUSB.print(r - current_position); //print target position
    SerialUSB.print(",");
    SerialUSB.println(yw - current_position); // print current position

    if (millis() > start_millis + 300) {
      r = (current_position + jump);
    }

    if (millis() > start_millis + 1000) {
      r = current_position;
    }
  }
}
