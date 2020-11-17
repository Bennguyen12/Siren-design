#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define S1 0x10
#define S2 0x20
#define S3 0x40
#define S4 0x80

#define LED1 0x10
#define LED2 0x20
#define LED3 0x40
#define LED4 0x80

#define LEDS_OFF 0x0F
// Student number n10275355;
// 5: wave form: Sawtooth, falling
// 3: f_min: 2400 Hz
// 5: f_max: 5100 Hz
// 5: f_inc: 100 Hz
enum
{

	STATE1,
	STATE2,
	STATE3,
	STATE4,
	STATE5,
};

//Use for ISR
// intermediate register used for debouncing, keep track of changes 2 more time steps.
volatile uint8_t pushbuttons_db = 0;
volatile uint64_t numbers_ticks;
volatile uint8_t reg1, reg2, cycle;
volatile uint16_t f_max, f_min, value_top_step, dev_f;
void init()
{
	cli(); //Disable interrupts

	//Set sysclk to 16 MHz
	CLKPR = 0x80; // Prescaler change enable
	CLKPR = 0x00; // Set prescaler to zero

	DDRF &= 0x0F;  // Set PORTF bit 7-4 as input (PBS)
	DDRD |= 0xF0;  // Set PORTD bits 7-4 as outputs (LEDs)
	PORTD &= 0x0F; // initalise LEDs turn off

	DDRB |= 0x40; // Set PORTB bit 6 as output

	// Initialise Timer1
	// Using OCB1, connected to B6

	TCCR1A = 0x23; // fast PWM, clear on CMP, TOP in OCR1A
	TCCR1B = 0x19; // fast PWM. TOP in OCR1A, prescale 1
	numbers_ticks = 0;
	TIMSK1 = 0x02;

	f_max = 5100;
	f_min = 2400;
	value_top_step = 16e6 / (f_max);

	cycle = 0b00000001;
	// initialise to 50% duty cycle

	TCCR0A = 0x02;
	TCCR0B = 0x04; //prescale 256
	OCR0A = 0x7D;  // 2ms
	TIMSK0 = 0x02; // interup on OCR0A
	sei();		   //Enable interrupts
}

int main(void)
{
	uint8_t pb_now = 0, pb_prev = 0, pb_pressed;
	uint8_t state = STATE1;
	cycle = 0b00000000;
	// Initialisation
	init();
	while (1)
	{

		pb_now = pushbuttons_db; // BUuttons pressed
		pb_pressed = pb_now & (pb_now ^ pb_prev);
		pb_prev = pb_now;

		if (pb_pressed & S4)
		{
			PORTD = pb_pressed & LED4;
			state = STATE5;
			cycle = 0b0010000;
		}

		switch (state)
		{
		case STATE1:
			PORTD &= LEDS_OFF;
			PORTD |= LED1;
			cycle = 0b00000001;
			if (pb_pressed & S1)
			{
				state = STATE2;
			}

			break;
		case STATE2:		   // F max range from 5k6 Hz to 4k6Hz
			PORTD &= LEDS_OFF; // ADjust F max
			PORTD |= LED2;
			cycle = 0b00000010;
			if (pb_pressed & S2)
			{
				f_max += 100;

				if (f_max > 5600)
				{
					f_max = 5600;
				}
			}
			if (pb_pressed & S3)
			{
				f_max -= 100;

				if (f_max < 4600)
				{
					f_max = 4600;
				}
			}

			// initialise to 50% duty cycl
			if (pb_pressed & S1)
			{
				state = STATE3;
			}
			break;
		case STATE3:		   // f_min range from 1k9 to 2k9 Hz
			PORTD &= LEDS_OFF; // ADjust f min
			PORTD |= LED3;
			cycle = 0b00000100;

			if (pb_pressed & S2)
			{
				f_min += 100;

				if (f_min > 2900)
				{
					f_min = 2900;
				}
			}
			if (pb_pressed & S3)
			{
				f_min -= 100;

				if (f_min < 1900)
				{
					f_min = 1900;
				}
			}
			if (pb_pressed & S1)
			{
				cycle = 0b10000000;
				state = STATE4;
			}
			break;
		case STATE4:
			PORTD &= LEDS_OFF;
			PORTD |= LED1;
			cycle = 0b00001000;
			if (cycle == 0b10000000)
			{
				numbers_ticks = 0;
				value_top_step = 16e6 / (f_max);
			}
			if (pb_pressed & S1)
			{
				state = STATE2;
			}
			break;

		case STATE5: // Reset
			PORTD &= LEDS_OFF;
			PORTD |= LED1;
			if (cycle == 0b0010000)
			{
				f_max = 5100;
				f_min = 2400;
				numbers_ticks = 0;
				value_top_step = 16e6 / (f_max);
				cycle = 0b0100000;
			}
			if (pb_pressed & S1)
			{
				state = STATE2;
			}
		default:
			state = STATE1;
		}

		// Create sawtooth falling
	}
}

ISR(TIMER0_COMPA_vect)
{
	uint8_t pb0;				  // new value read from pushbuttons
	uint8_t delta;				  // difference betwwen new value and present stable state
	pb0 = ~PINF;				  //  read new value from pushbuttons
	delta = pb0 ^ pushbuttons_db; // find differece betwwen new value and present stable state
	// pushbutton_db only updated if difeerence persist through 2 stages (reg1 and reg2)
	pushbuttons_db ^= (reg2 & delta);

	reg2 = (reg1 & delta); // if diferences still persist they are propagate to reg2
	reg1 = delta;		   //diferences stored in reg1
}

ISR(TIMER1_COMPA_vect)
{
	if (cycle == 0b00000001)
	{
		OCR1A = value_top_step;
		OCR1B = value_top_step / 2;
		numbers_ticks += value_top_step;
		if (numbers_ticks > 16e6)
		{
			numbers_ticks = 0;
		}
		dev_f = ((f_max - f_min) * numbers_ticks) / (16e6);
		value_top_step = (16e6) / (f_max - dev_f);
	}
	if (cycle == 0b00000010)
	{
		OCR1A = 16e6 / (f_max);
		OCR1B = 16e6 / (f_max) / 2;
	}
	if (cycle == 0b00000100)
	{
		OCR1A = 16e6 / (f_min);
		OCR1B = 16e6 / (f_min) / 2;
	}
	if (cycle == 0b00001000)
	{
		OCR1A = value_top_step;
		OCR1B = value_top_step / 2;
		numbers_ticks += value_top_step;
		if (numbers_ticks > 16e6)
		{
			numbers_ticks = 0;
		}
		dev_f = ((f_max - f_min) * numbers_ticks) / (16e6);
		value_top_step = (16e6) / (f_max - dev_f);
	}
}