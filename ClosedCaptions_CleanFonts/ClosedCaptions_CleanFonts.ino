#include <TVout.h>
#include <fontALL.h>
#include <pollserial.h>
#define W 128
#define H 96
#define BITWIDTH 5
#define THRESHOLD 3
TVout tv;
pollserial pserial;
unsigned char x, y;
char s[32];
int start = 40;
unsigned char ccdata[16];
// TiVo, DVD player
byte bpos[][8] = {{26, 32, 38, 45, 51, 58, 64, 70}, {78, 83, 89, 96, 102, 109, 115, 121}};
boolean wroteOutput = false;
boolean newline = false;
char c[2];
//char compare[] = {"T"}; //trigger letter
char lastControlCode[2];
int line = 22;
int dataCaptureStart = 310;
unsigned int loopCount = 0;
float compVolt = 0;
int wordCount = 0;
unsigned long startTime;

char *dictionary[] = {
  "Non-visible data area",
  "Machine laerning",
  "Vote Trump!",
  "Human beings transmitted as data",
  "Intelligently artificial",
  "Is this art?",
  "Ephemeral Objects",
  "BuR5T S1GNaL",
  "The loudest sound in the universe"
};


void setup()  {
  tv.begin(_NTSC, W, H);
  tv.set_hbi_hook(pserial.begin(57600));
  initOverlay();
  initInputProcessing();

  tv.setDataCapture(line, dataCaptureStart, ccdata);
  tv.select_font(font6x8);
  tv.fill(0);

  randomSeed(1);
  startTime = millis();
}


void initOverlay() {
  TCCR1A = 0;
  // Enable timer1.  ICES0 is set to 0 for falling edge detection on input capture pin.
  TCCR1B = _BV(CS10);

  // Enable input capture interrupt
  TIMSK1 |= _BV(ICIE1);

  // Enable external interrupt INT0 on pin 2 with falling edge.
  EIMSK = _BV(INT0);
  EICRA = _BV(ISC01);
}

void initInputProcessing() {
  // Analog Comparator setup
  ADCSRA &= ~_BV(ADEN); // disable ADC
  ADCSRB |= _BV(ACME); // enable ADC multiplexer
  ADMUX &= ~_BV(MUX0); // select A2 for use as AIN1 (negative voltage of comparator)
  ADMUX |= _BV(MUX1);
  ADMUX &= ~_BV(MUX2);
  ACSR &= ~_BV(ACIE);  // disable analog comparator interrupts
  ACSR &= ~_BV(ACIC);  // disable analog comparator input capture
}

ISR(INT0_vect) {
  display.scanLine = 0;
  wroteOutput = false;
  for (x = 0; x < display.hres; x++) {
    ccdata[x] = 0;
  }
}


// display the captured data line on the screen
void displayccdata() {
  y = 0;
  for (x = 0; x < display.hres; x++) {
    display.screen[(y * display.hres) + x] = ccdata[x];
  }
}

int insertFlag = 0;

void loop() {
  byte pxsum;
  byte i;
  byte parityCount;
  loopCount++;

  if ( millis() - startTime < 20000) {
    for (int k = 9; k > 0; k--)
    {
      ccdata[random(0, 16)] = 1;
      ccdata[random(0, 16)] = 0;
      ccdata[k] = ccdata[k - 1];
    }
    if ( insertFlag ) {
      delay(100);
      tv.print("   ");
      tv.print(dictionary[random(0, 10)]);
      tv.print("   ");
      insertFlag = 0;
      delay(100);
    }
  }

  if ( millis() - startTime > 60000 ) {
    startTime = millis();
  }


  // Display the captured data line to the screen so we can see it.
  displayccdata();

  if ((ccdata[0] > 0) && (!wroteOutput) ) {
    // we have new data to decode
    for (byte bytenum = 0; bytenum < 2; bytenum++) {
      c[bytenum] = 0;
      parityCount = 0;
      for (int bit = 0; bit < 8; bit++) {
        pxsum = 0;
        for (int w = 0; w < BITWIDTH; w++) {
          i = bpos[bytenum][bit] + w;
          if (((ccdata[i / 8] >> (7 - (i % 8))) & 1) == 1) {
            pxsum++;
          }
        }
        if (pxsum >= THRESHOLD) {
          // consider the bit to be "on"
          c[bytenum] |= (1 << bit);
          parityCount++;
        }
      }

      if ((parityCount % 2) == 1) {
        // parity check matches
        // strip off the MSB because it's the parity bit
        c[bytenum] &= 0x7F;
      } else {
        // parity check failed
        c[bytenum] = 0;
      }
    }

    // output the data
    if ((c[0] > 0) && (c[0] < ' ')) {
      // control character
      if ((c[0] != lastControlCode[0]) && (c[1] != lastControlCode[1])) {
        if (!newline) {
          pserial.write('\n');
          newline = true;
        }
        lastControlCode[0] = c[0];
        lastControlCode[1] = c[1];
      }
    } else {
      if (c[0] > 0) {
        newline = false;
        lastControlCode[0] = 0;
        pserial.write(c[0]);
        tv.print(c[0]);
      }
      if (c[1] > 0) {
        newline = false;
        lastControlCode[0] = 0;
        pserial.write(c[1]);
        tv.print(c[1]);
      }
    }
    wroteOutput = true;
  }
  if ( (millis() - startTime > 5000) && (millis() - startTime < 5050) ) {
    tv.select_font(font8x8ext);
    insertFlag = 1;
  }
  else if ( (millis() - startTime > 40600) && (millis() - startTime < 40650) || (millis() - startTime > 40000) && (millis() - startTime < 40050)  ) {
    tv.select_font(font6x8);
    insertFlag = 1;
  }
}

int getValue() {
  int value;
  ADCSRA |= _BV(ADEN); // enable ADC
  value = analogRead(5);
  initInputProcessing();
  return value;
}

void displayBitPositions() {
  y = 1;
  tv.draw_line(0, y, W - 1, y, 0);
  for (byte bytenum = 0; bytenum < 2; bytenum++) {
    for (byte bit = 0; bit < 8; bit++) {
      tv.set_pixel(bpos[bytenum][bit], y, 1);
    }
  }
}
