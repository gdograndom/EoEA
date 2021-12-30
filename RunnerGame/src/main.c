/* Includes */
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//Connections information
// PA8 ---> RS
// PA9 ---> E
// PC6 ---> D4
// PC7 ---> D5
// PC8 ---> D6
// PC9 ---> D7

//Button A ---> PA1 (Jump)
//Button B ---> PA0 (Crouch)

//LCD module information
#define lcd_LineOne 0x00 // start of line 1
#define lcd_LineTwo 0x40 // start of line 2

//LCD instructions
#define lcd_Clear 0b00000001 // replace all characters with ASCII 'space'
#define lcd_Home 0b00000010 // return cursor to first position on first line
#define lcd_EntryMode 0b00000110 // shift cursor from left to right on read/write
#define lcd_DisplayOff 0b00001000 // turn display off
#define lcd_DisplayOn 0b00001100 // display on, cursor off, don't blink character
#define lcd_FunctionReset 0b00110000 // reset the LCD
#define lcd_FunctionSet4bit 0b00101000 // 4-bit data, 2-line display, 5 x 7 font
#define lcd_SetCursor 0b10000000 // set cursor position

//-----------------------------VARIABLES FOR GAME LOGIC--------------------------------------

//flag for button press interrupt
volatile char cpress;
volatile char bpress;

//other flags used for game logic
volatile char isUp;
volatile char isDown;
volatile int airborne;
volatile int sliding;
volatile char dontprint;
volatile char birds;
volatile char gameOver;
volatile char newHS;
volatile char trick;
volatile int overBird;

//ints to track scores and highscores
int score;
int highscore;
int numTricks;
int highnumTricks;

//array that holds characters at players position so they can move past
char hold[1];

//ints to track distance between cars, how close they can be and current game speed
int distance;
int carLimit;
int gameSpeed;

//holds the current custom char for the player's run animation
uint8_t character;

//int i for for loops
int i;

//makes run animation change every other frame
int t;

//----------------------------CUSTOM CHARACTERS FOR LCD DISPLAY-------------------------------
//Some of these are unused but may be implemented in further revisions of this project

//String arrs to hold chars in top and bottom rows of screen
//both initialized to be empty - 16 spaces for 16x2 LCD module
uint8_t sky[] = "                "; //16 spaces
uint8_t ground[] = "                "; //16 spaces

//Messages for current score, score in past round and high score
uint8_t scoremsg[] = "S: ";
uint8_t gameovertop[] = "Score: ";
uint8_t gameoverbottom[] = "Best: ";
uint8_t gameovertrick[] = "Tricks: ";

//Custom characters for animation of player and obstacles
uint8_t stand[] = {0x00, 0x00, 0x04, 0x0E, 0x0E, 0x04, 0x0A, 0x0A};
uint8_t orun1[] = {0x00,0x00,0x04,0x0E,0x0C,0x04,0x0A,0x02};
uint8_t orun2[] = {0x00,0x00,0x04,0x0E,0x06,0x04,0x0A,0x08};
uint8_t run1[] = {0x00,0x00,0x04,0x06,0x0C,0x06,0x09,0x10};
uint8_t run2[] = {0x00,0x00,0x04,0x0C,0x06,0x04,0x1A,0x02};
uint8_t jump[] = {0x00,0x00,0x05,0x0E,0x14,0x04,0x0B,0x10};
uint8_t jump2[] = {0x04,0x0E,0x15,0x04,0x0A,0x0A,0x00,0x00};
uint8_t jump3bottom[] = {0x14,0x04,0x0A,0x11,0x00,0x00,0x00,0x00};
uint8_t jump3top[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0E};
uint8_t fall[] = {0x00,0x00,0x04,0x0E,0x15,0x04,0x0A,0x11};
uint8_t car[] = {0x00,0x00,0x00,0x00,0x07,0x1F,0x1F,0x0A};
uint8_t trickCar[] = {0x00,0x00,0x0A,0x15,0x00,0x07,0x1F,0x0A};
uint8_t crash[] = {0x00,0x00,0x00,0x00,0x04,0x12,0x15,0x1F};
uint8_t bird[] = {0x00,0x00,0x06,0x0D,0x1F,0x0C,0x06,0x00};
uint8_t slide[] = {0x00,0x00,0x00,0x00,0x02,0x1C,0x0A,0x12};
uint8_t celebrate[] = {0x00, 0x00, 0x15, 0x0E, 0x04, 0x04, 0x0A, 0x0A};
uint8_t celebrateJump[] = {0x00, 0x0A, 0x0E, 0x0E, 0x04, 0x04, 0x0A, 0x11};

//------------------------------FUNCTION DEFINITIONS------------------------------------------

//systick
extern __IO uint32_t Timingdelay;

//systick initialization
void systick_Init(void)
{
	SysTick_Config((SystemCoreClock)/1000000); // generate an interrupt every 1 us

}

//Delay method using systick
void delay_us(__IO uint32_t time)
{
	Timingdelay = time;
	while(Timingdelay !=0);
}

//Function to write bytes to data pins of LCD
void LCD_Write(uint8_t byte) {
	//assume data for each bit is 0, then make it 1 if necessary
	//D7
	GPIO_WriteBit(GPIOC, GPIO_Pin_9, Bit_RESET);
	if (byte & 1<<7) GPIO_WriteBit(GPIOC, GPIO_Pin_9, Bit_SET);
	//D6
	GPIO_WriteBit(GPIOC, GPIO_Pin_8, Bit_RESET);
	if (byte & 1<<6) GPIO_WriteBit(GPIOC, GPIO_Pin_8, Bit_SET);
	//D5
	GPIO_WriteBit(GPIOC, GPIO_Pin_7, Bit_RESET);
	if (byte & 1<<5) GPIO_WriteBit(GPIOC, GPIO_Pin_7, Bit_SET);
	//D4
	GPIO_WriteBit(GPIOC, GPIO_Pin_6, Bit_RESET);
	if (byte & 1<<4) GPIO_WriteBit(GPIOC, GPIO_Pin_6, Bit_SET);

	//now write data to LCD
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_SET); //Set E high
	delay_us(1000); //min delay of 310ns
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET); //Set enable back to low
	delay_us(1000); //min delay of 510ns
}

//Function to specifically write an instruction to LCD using LCD_Write
void LCD_Write_Instruction(uint8_t inst) {

	//Set Instruction Register (RS low)
	GPIO_WriteBit(GPIOA, GPIO_Pin_8, Bit_RESET);

	//Ensure E is initially low
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);

	//write upper 4-bits of the data
	LCD_Write(inst);
	//write lower 4-bits
	LCD_Write(inst << 4);
}

//Function to write a single character to LCD using LCD_Write
void LCD_Write_Char(uint8_t byte) {

	//Set Data Register (RS high)
	GPIO_WriteBit(GPIOA, GPIO_Pin_8, Bit_SET);

	//ensure E is initially low
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);

	//write upper 4-bits of the data
	LCD_Write(byte);
	//write lower 4-bits
	LCD_Write(byte << 4);
}

//Function to write an array of chars (string) to LCD using calls to LCD_Write_Char
void LCD_Write_String(uint8_t string[]) {
	volatile int i = 0;
	while (string[i] != 0) {
		LCD_Write_Char(string[i]);
		i++;
		delay_us(1000); //min delay of 40us
	}
}

//The below functions handle settup for LCD and various microcontroller ports
//Functions are called in main upon program being flashed onto board

//LCD screen initialization
void LCD_Init(void) {
	//power up delay, min 40ms from datasheet
	delay_us(100000);

	//set RS and E to low for LCD_Write
	GPIO_WriteBit(GPIOA, GPIO_Pin_8, Bit_RESET);
	GPIO_WriteBit(GPIOA, GPIO_Pin_9, Bit_RESET);

	//Reset of LCD controller
	LCD_Write(lcd_FunctionReset);
	delay_us(10000); //min delay of 4.1ms

	LCD_Write(lcd_FunctionReset);
	delay_us(1000); //min delay of 100us

	LCD_Write(lcd_FunctionReset);
	delay_us(1000); //delay omitted from datasheet but included just in case

	//set LCD to 4-bit mode for less wiring
	LCD_Write(lcd_FunctionSet4bit);
	delay_us(1000); //min delay of 40us
	//function set instruction
	LCD_Write_Instruction(lcd_FunctionSet4bit);
	delay_us(1000); //min delay of 40us

	//Continuing initialization
	//turn display OFF
	LCD_Write_Instruction(lcd_DisplayOff);
	delay_us(1000); //min delay of 40us

	//clear display RAM
	LCD_Write_Instruction(lcd_Clear);
	delay_us(3000); //min delay of 1.64ms

	//Set shift characteristics desired for game
	LCD_Write_Instruction(lcd_EntryMode);
	delay_us(1000); //min delay of 40us

	//finally, turn screen back on
	LCD_Write_Instruction(lcd_DisplayOn);
	delay_us(1000); //min delay of 40us

	//Store custom characters into CGRAM of LCD

	//store custom character to display's CGRAM @address 0x04
	LCD_Write_Instruction(0x60);
	for(i=0; i<8; i++) LCD_Write_Char(run2[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//same process for obstacles @CGRAM 0x01
	LCD_Write_Instruction(0x48);
	for(i=0; i<8; i++) LCD_Write_Char(car[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits
}

//GPIO output initialization for LCD
void LCD_GPIO_Init(void)
{
	//define struct for pins
	GPIO_InitTypeDef Struct_GPIO_PC6;
	GPIO_InitTypeDef Struct_GPIO_PC7;
	GPIO_InitTypeDef Struct_GPIO_PC8;
	GPIO_InitTypeDef Struct_GPIO_PC9;

	GPIO_InitTypeDef Struct_GPIO_PA8;
	GPIO_InitTypeDef Struct_GPIO_PA9;

	//enable clock to PORT C, has already been enabled to PORT A
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	//set parameters for GPIOC
	Struct_GPIO_PC6.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PC6.GPIO_Pin = GPIO_Pin_6;
	Struct_GPIO_PC6.GPIO_OType = GPIO_OType_PP;

	Struct_GPIO_PC7.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PC7.GPIO_Pin = GPIO_Pin_7;
	Struct_GPIO_PC7.GPIO_OType = GPIO_OType_PP;

	Struct_GPIO_PC8.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PC8.GPIO_Pin = GPIO_Pin_8;
	Struct_GPIO_PC8.GPIO_OType = GPIO_OType_PP;

	Struct_GPIO_PC9.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PC9.GPIO_Pin = GPIO_Pin_9;
	Struct_GPIO_PC9.GPIO_OType = GPIO_OType_PP;

	//set parameters for GPIOA
	Struct_GPIO_PA8.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PA8.GPIO_Pin = GPIO_Pin_8;
	Struct_GPIO_PA8.GPIO_OType = GPIO_OType_PP;

	Struct_GPIO_PA9.GPIO_Mode = GPIO_Mode_OUT;
	Struct_GPIO_PA9.GPIO_Pin = GPIO_Pin_9;
	Struct_GPIO_PA9.GPIO_OType = GPIO_OType_PP;

	GPIO_Init(GPIOC, &Struct_GPIO_PC6);
	GPIO_Init(GPIOC, &Struct_GPIO_PC7);
	GPIO_Init(GPIOC, &Struct_GPIO_PC8);
	GPIO_Init(GPIOC, &Struct_GPIO_PC9);

	GPIO_Init(GPIOA, &Struct_GPIO_PA8);
	GPIO_Init(GPIOA, &Struct_GPIO_PA9);
}

//GeneralPurpose button A initialization (jump)
void GPA_BUTTON_Init(void)
{
	//define struct for button
	GPIO_InitTypeDef Struct_GPIO_BUTTON1;

	//enable clock to PORT A
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	//set parameters for GPIOA
	Struct_GPIO_BUTTON1.GPIO_Mode = GPIO_Mode_IN;
	Struct_GPIO_BUTTON1.GPIO_Pin = GPIO_Pin_1;
	//Unless otherwise specified by a note, all I/Os are set as floating inputs during and after reset
	//Having embedded pull-ups and pull-downs removes the need to add external resistors - section 38.4.3 in ref manual
	Struct_GPIO_BUTTON1.GPIO_PuPd = GPIO_PuPd_DOWN;
	Struct_GPIO_BUTTON1.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOA, &Struct_GPIO_BUTTON1);
}

//GPIO button A interrupt initialization (jump)
void EXTI_Init_GPAButton(void)
{
	EXTI_InitTypeDef Struct_EXTI_Button;
	NVIC_InitTypeDef Struct_NVIC;

	//Configures the priority grouping: pre-emption priority and subpriority.
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	////SYSCFG APB clock must be enabled to get write access to SYSCFG_EXTICRx registers:
	//Interrupt configuration registers and  selects the source of interrupt
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//Tell system to use PA0 for EXTI_Line0
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource1);

	//set parameters for EXTI
	Struct_EXTI_Button.EXTI_Line = EXTI_Line1;
	Struct_EXTI_Button.EXTI_LineCmd = ENABLE;
	Struct_EXTI_Button.EXTI_Mode = EXTI_Mode_Interrupt;
	Struct_EXTI_Button.EXTI_Trigger = EXTI_Trigger_Rising_Falling;

	EXTI_Init(&Struct_EXTI_Button);

	//set parameters for NVIC for PA1 interrupt
	//PA1 is connected to EXTI_Line1, which has EXTI1_IRQn vector
	Struct_NVIC.NVIC_IRQChannel = EXTI1_IRQn;
	Struct_NVIC.NVIC_IRQChannelCmd = ENABLE;
	Struct_NVIC.NVIC_IRQChannelPreemptionPriority = 0x0A;
	Struct_NVIC.NVIC_IRQChannelSubPriority = 0x0A;

	NVIC_Init(&Struct_NVIC);
}

//GeneralPurpose button B initialization (slide)

void GPB_BUTTON_Init(void)
{
	//define struct for button
	GPIO_InitTypeDef Struct_GPIO_BUTTON2;

	//enable clock to PORT A
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	//set parameters for GPIOA
	Struct_GPIO_BUTTON2.GPIO_Mode = GPIO_Mode_IN;
	Struct_GPIO_BUTTON2.GPIO_Pin = GPIO_Pin_2;
	//Unless otherwise specified by a note, all I/Os are set as floating inputs during and after reset
	//Having embedded pull-ups and pull-downs removes the need to add external resistors - section 38.4.3 in ref manual
	Struct_GPIO_BUTTON2.GPIO_PuPd = GPIO_PuPd_DOWN;
	Struct_GPIO_BUTTON2.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOB, &Struct_GPIO_BUTTON2);
}

//GPIO button interrupt initialization
void EXTI_Init_GPBButton(void)
{
	EXTI_InitTypeDef Struct_EXTI_Button;
	NVIC_InitTypeDef Struct_NVIC;

	//Configures the priority grouping: pre-emption priority and subpriority.
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	////SYSCFG APB clock must be enabled to get write access to SYSCFG_EXTICRx registers:
	//Interrupt configuration registers and  selects the source of interrupt
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//Tell system to use PA0 for EXTI_Line0
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource2);

	//set parameters for EXTI
	Struct_EXTI_Button.EXTI_Line = EXTI_Line2;
	Struct_EXTI_Button.EXTI_LineCmd = ENABLE;
	Struct_EXTI_Button.EXTI_Mode = EXTI_Mode_Interrupt;
	Struct_EXTI_Button.EXTI_Trigger = EXTI_Trigger_Rising_Falling;

	EXTI_Init(&Struct_EXTI_Button);

	//set parameters for NVIC for PA0 interrupt
	//PA0 is connected to EXTI_Line0, which has EXTI0_IRQn vector
	Struct_NVIC.NVIC_IRQChannel = EXTI2_IRQn;
	Struct_NVIC.NVIC_IRQChannelCmd = ENABLE;
	Struct_NVIC.NVIC_IRQChannelPreemptionPriority = 0x0F;
	Struct_NVIC.NVIC_IRQChannelSubPriority = 0x0F;

	NVIC_Init(&Struct_NVIC);
}

//interrupt handler for GeneralPurpose button A (jump)
void EXTI1_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line1) != RESET)
	{
		uint8_t buttonvalue;
		//Make sure that interrupt flag is set
		if(EXTI_GetITStatus(EXTI_Line1)!=RESET)
		{
			delay_us(20000); //20ms debounce
			buttonvalue = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);
			if(buttonvalue==0)
			{
				//sets flag high upon legal button press
				cpress = 1;

				//clear interrupt flag
				EXTI_ClearITPendingBit(EXTI_Line1);
			}
		}
	}
}

//interrupt handler for GeneralPurpose button B (slide)
void EXTI2_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line2) != RESET)
	{
		uint8_t buttonvalue2;
		//Make sure that interrupt flag is set
		if(EXTI_GetITStatus(EXTI_Line2)!=RESET)
		{
			delay_us(20000); //20ms debounce
			buttonvalue2 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2);
			if(buttonvalue2==0)
			{
				//sets flag high upon legal button press
				bpress = 1;

				//clear interrupt flag
				EXTI_ClearITPendingBit(EXTI_Line2);
			}
		}
	}
}

//Displays game's start screen and initializes vars upon game start or restart
void gameSetup(void) {

	//Versatile custom character definitions
	//These are defined here as opposed to LCD_Init as these addresses are used
	//to hold multiple custom characters defined above and are changed within game code

	//store custom character to display's CGRAM @address 0x07
	LCD_Write_Instruction(0x78);
	for(i=0; i<8; i++) LCD_Write_Char(jump[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//store custom character to display's CGRAM @address 0x06
	LCD_Write_Instruction(0x70);
	for(i=0; i<8; i++) LCD_Write_Char(jump3bottom[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//store custom character to display's CGRAM @address 0x05
	LCD_Write_Instruction(0x68);
	for(i=0; i<8; i++) LCD_Write_Char(trickCar[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//store custom character to display's CGRAM @address 0x03
	LCD_Write_Instruction(0x58);
	for(i=0; i<8; i++) LCD_Write_Char(run1[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//store custom character to display's CGRAM @address 0x02
	LCD_Write_Instruction(0x50);
	for(i=0; i<8; i++) LCD_Write_Char(stand[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//same process for obstacles @CGRAM 0x00
	LCD_Write_Instruction(0x40);
	for(i=0; i<8; i++) LCD_Write_Char(bird[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits

	//Display game's start screen

	//Set cursor to line 1
	LCD_Write_Instruction(lcd_SetCursor | lcd_LineOne);
	delay_us(1000); //min 40us delay

	uint8_t intro[] = "Ready to Run?";
	LCD_Write_String(intro);

	//set cursor to line 2
	LCD_Write_Instruction(lcd_SetCursor | lcd_LineTwo);
	delay_us(1000); //min 40us delay

	uint8_t intro2[] = "     Dodge cars!";
	intro2[3] = 0x02;
	LCD_Write_String(intro2);

	delay_us(1000000);

	//ensure that ground and sky are both empty (filled with 16 spaces)
	for(i = 0; i < 17; i++) ground[i] = ' ';
	for(i = 0; i < 17; i++) sky[i] = ' ';

	//use ground and sky to clear the screen
	LCD_Write_Instruction(lcd_Home);
	delay_us(1000); //min 40us

	LCD_Write_String(ground);

	LCD_Write_Instruction(lcd_SetCursor | lcd_LineOne);
	delay_us(1000); //min 40us delay

	LCD_Write_String(sky);

	delay_us(20000); //20ms

	//Initialize game logic vars
	cpress = 0;
	bpress = 0;
	isUp = 0;
	dontprint = 0;
	birds = 0;
	distance = 0;
	carLimit = 7;
	score = 1;
	numTricks = 0;
	trick = 0;
	overBird = 0;
	gameOver = 0;
	newHS = 0;
	character = 0x03;
	gameSpeed = 55;
	t = 0;

	//store custom character to display's CGRAM @address 0x02
	LCD_Write_Instruction(0x50);
	for(i=0; i<8; i++) LCD_Write_Char(jump3top[i]);
	LCD_Write_Instruction(0x80); //end receiving char bits
}

int main(void)
{
	//calls to initialization functions
	systick_Init();
	GPA_BUTTON_Init();
	EXTI_Init_GPAButton();
	GPB_BUTTON_Init();
	EXTI_Init_GPBButton();
	LCD_GPIO_Init();
	LCD_Init();

	//Re-set priority of systick to highest
	NVIC_SetPriority(SysTick_IRQn, 0);

	//call to gameSetup
	gameSetup();


	/* Infinite loop - Game continues to run until game over */
	while (1)
	{
		//Shifts everything in bottom row to the left
		//(makes cars drive towards player)
		for(i = 0; i < 16; i++) ground[i] = ground[i + 1];

		if((rand() % 100) > (rand() % 100) && !dontprint) { //handles car spawning

			if (birds) { //if player is far enough for birds to appear
				if((rand() % 100) > (rand() % 100)) { //random chance for obstacle to be bird instead of car
					ground[15] = '^';
				} else ground[15] = 0x01;
			} else ground[15] = 0x01; //0x01 represents the cars

			dontprint = 1; //acts as boolean to ensure car separation
			distance = 0; //reset distance between cars
		}
		else ground[15] = ' '; //if game does not spawn car, insert a space char

		//handles tracking char as it moves past player
		hold[0] = hold[1];
		hold[1] = ground[3];

		//if player is running and button is pressed
		//make player jump and set according flags
		if (!isUp && !isDown && cpress) {
			ground[3] = 0x06; //ground player pos shows player's feet
			sky[3] = 0x02; //sky player pos shows player's upper body
			ground[2] = hold[0]; //ground behind player becomes char that was in front of player
			isUp = 1; //flag set to true to indicate player is in the air
			cpress = 0; //reset button press flag
			airborne++; //increment airborne
		}

		//if player is running and slide button pressed
		//make player slide and set according flags
		else if (!isUp && !isDown && bpress) {
			//store custom character to display's CGRAM @address 0x03
			LCD_Write_Instruction(0x58);
			for(i=0; i<8; i++) LCD_Write_Char(slide[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			ground[3] = 0x03;
			ground[2] = hold[0];
			isDown = 1; //flag set to true to indicate player is sliding
			bpress = 0; //reset button press flag
			sliding++;
		}

		//if player is in the air and has been for less than 4 cycles
		else if (isUp && !overBird && airborne < 4) {

			//if player has been in air for one cycle
			if(airborne == 1) {
				sky[3] = 0x07; //sky player pos shows player's jumping animation
				ground[3] = ' '; //clear ground at players pos
			}

			//if player has been in air for three cycles, they are about to land
			if(airborne == 3) {
				ground[3] = 0x06; //ground player pos shows player's feet
				sky[3] = 0x02; //sky player pos shows player's upper body
			}

			//otherwise, player is still in their jumping animation so no change needed

			//regardless,
			sky[2] = ' '; //sky behind player pos becomes blank
			ground[2] = hold[0]; //ground behind player becomes char that was in front of player
			airborne++; //increment cycles player has been airborne
		}

		//if player is in the air and has been for 4 cycles or longer
		//make player land
		else if (isUp && !overBird && airborne >= 4) {
			ground[3] = 0x06; //ground player pos shows player's feet
			sky[3] = 0x02; //sky player pos shows player's upper body
			ground[2] = hold[0]; //ground behind player becomes char that was in front of player
			isUp = 0; //set flag to false to indicate player is on the ground
			trick = 0; //set flag false to indicate player did not perform a trick
			airborne = 0; //reset airborne counter for next jump
		}

		//if player is sliding and has been for less than 3 cycles
		else if (isDown && sliding < 3) {
			ground[3] = 0x03;
			ground[2] = hold[0];
			sliding++;
		}

		//if player is sliding and has been for 3 cycles or longer
		//make player get back up
		else if (isDown && sliding >= 3) {

			//store custom character to display's CGRAM @address 0x03
			LCD_Write_Instruction(0x58);
			for(i=0; i<8; i++) LCD_Write_Char(run1[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			ground[3] = character; //return player to running
			ground[2] = hold[0]; //ground behind player becomes char that was in front of player
			isDown = 0; //set flag to false to indicate player is running
			sliding = 0; //reset sliding counter for next slide
		}

		//if none of the above applied, player is running forward on the ground
		else {
			sky[3] = ' '; //sky player pos becomes blank as player is not in air
			sky[2] = ' '; //sky behind player becomes blank
			ground[2] = ' '; //ground behind player becomes blank instead of what was in front of player
							 //(As player is running on the ground, simply need to remove past frame)
			if (!overBird) ground[3] = character; //update player's animation frame if not special case
		}

		//if player is about to land and the char underneath is a car, make player do a trick!
		if (hold[1] == 0x01 && airborne == 4) {
			sky[3] = 0x06; //sky player pos shows player's feet as they have jumped higher
			sky[2] = ' '; //sky behind player becomes blank
			ground[2] = hold[0]; //ground behind player becomes char that was in front of player
			ground[3] = 0x05; //car underneath player is updated to show it has been tricked off of
			airborne = 2; //airborne counter is reset to 2 so that player gains extra airtime
			trick = 1; //flag updated to show player has performed a trick
			numTricks++; //num of tricks counter incremented
		}

		//if distance between cars is over the limit
		if (distance > carLimit) {
			dontprint = 0; //don't spawn car flag set to low to indicate a new car can spawn
		}

		//running animation for player - alternates frame each cycle

		if (t == 1) {
			if (character == 0x03){
				character = 0x04;
			}
			else {
				character = 0x03;
			}
			t=0;
		} else t++;


		//Prints the current score during the game
		uint8_t cnt = 13; //starting at index 13 of sky
		for(i = 100; i > 0; i /= 10){
			sky[cnt] = ((score / i) % 10) + '0';
			cnt++;
		}

		//Prints the number of tricks during the game if num is > 0. Otherwise hidden.
		//Chosen to be like this as right now its functionality is more of an easter egg
		if(numTricks > 0) {
			cnt = 10; //starting at index 10 of sky
			for(i=10; i > 0; i /= 10) {
				sky[cnt] = ((numTricks/i) % 10) + '0';
				cnt++;
			}
		}


		//OverBird animation handling

		//if player is in the air and the char underneath is a bird, GAME OVER - start animation
		if (hold[1] == '^' && isUp) {
			//sky[3] = '*'; //player gets hurt, symbolized by asterisk
			//ground[3] = ' '; //underneath player is updated to blank as bird has flown up
			sky[2] = ' '; //sky behind player becomes blank

			overBird = 1;
		}

		//if player has game overed from attempting to jump over a bird - delayed by one frame for animation
		else if (overBird == 1) {
			delay_us(500000); //500ms dramatic effect
			overBird++;
		}

		else if (overBird == 2) {
			sky[3] = '^'; //bird flies up, attacking player
			ground[3] = ' '; //bird leaves ground
			overBird++;
		}

		else if (overBird == 3) {
			sky[3] = '*'; //indicates bird hurt player

			//update custom character on CGRAM @address 0x02 to be crash custom character
			LCD_Write_Instruction(0x50);
			for(i=0; i<8; i++) LCD_Write_Char(crash[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			overBird++;
		}

		else if (overBird == 4) {
			sky[3] = 0x02;
			overBird++;
		}

		else if (overBird == 5) {
			sky[3] = ' ';
			ground[3] = 0x02;
			overBird++;
		}

		else if (overBird == 6) {

			delay_us(500000); //500ms dramatic effect

			gameOver = 1; //set gameOver flag to true
			LCD_Write_Instruction(lcd_Clear); //clear the display
			delay_us(3000); //min delay of 1.64ms

			for(i = 0; i < 17; i++) ground[i] = ' '; //clear ground
			for(i = 0; i < 17; i++) sky[i] = ' '; //clear sky

			//update custom character on CGRAM @address 0x02 to be standing frame of player
			LCD_Write_Instruction(0x50);
			for(i=0; i<8; i++) LCD_Write_Char(stand[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			//show player standing on index 14 of ground
			ground[14] = 0x02;
		}




		//If a bird is within the player's position and they are running on the ground, GAME OVER
		if (hold[1] == '^' && !isUp && !isDown) {
			gameOver = 1; //set gameOver flag to true
			LCD_Write_Instruction(lcd_Clear); //clear the display
			delay_us(3000); //min delay of 1.64ms

			for(i = 0; i < 17; i++) ground[i] = ' '; //clear ground
			for(i = 0; i < 17; i++) sky[i] = ' '; //clear sky

			//update custom character on CGRAM @address 0x02 to be standing frame of player
			LCD_Write_Instruction(0x50);
			for(i=0; i<8; i++) LCD_Write_Char(stand[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			//show player standing on index 14 of ground
			ground[14] = 0x02;
		}

		//If a car is within the player's position and they are on the ground, GAME OVER
		if(hold[1] == 0x01 && !isUp && !trick) {
			gameOver = 1; //set gameOver flag to true
			LCD_Write_Instruction(lcd_Clear); //clear the display
			delay_us(3000); //min delay of 1.64ms

			for(i = 0; i < 17; i++) ground[i] = ' '; //clear ground
			for(i = 0; i < 17; i++) sky[i] = ' '; //clear sky

			//update custom character on CGRAM @address 0x02 to be standing frame of player
			LCD_Write_Instruction(0x50);
			for(i=0; i<8; i++) LCD_Write_Char(stand[i]);
			LCD_Write_Instruction(0x80); //end receiving char bits

			//show player standing on index 14 of ground
			ground[14] = 0x02;
		}

		//update screen on each cycle
		LCD_Write_Instruction(lcd_SetCursor | lcd_LineOne);
		delay_us(100); //min 40us delay
		LCD_Write_String(sky);

		LCD_Write_Instruction(lcd_SetCursor | lcd_LineTwo);
		delay_us(100); //min 40us delay
		LCD_Write_String(ground);


		//logic for game's increasing difficulty
		if (score % 20 == 0) gameSpeed -= 5; //increase game speed every 5 score
		if (score % 150 == 0) carLimit--; //decrease min distance between cars every 150 score
		if (score > 50) birds = 1; //add birds to dodge at certain score limit
		if (carLimit < 4) carLimit = 4; //absolute min of car distance is 4

		//if game speed is > 0, delay by the amount on each cycle
		if (gameSpeed > 0) delay_us(gameSpeed);

		//increase distance and score on each cycle
		distance++;
		score++;

		//If player game overed:
		if(gameOver) {

			//update high score to be the achieved score if achieved score is higher
			if(score > highscore) {
				highscore = score;
				newHS = 1; //set flag to indicate a new high score has been achieved.

				//Update stored custom characters to hold player victory animation frames

				//update custom character on CGRAM @address 0x06 to be celebrationJump
				LCD_Write_Instruction(0x70);
				for(i=0; i<8; i++) LCD_Write_Char(celebrateJump[i]);
				LCD_Write_Instruction(0x80); //end receiving char bits

				//update custom character on CGRAM @address 0x05 to be celebrationStand
				LCD_Write_Instruction(0x68);
				for(i=0; i<8; i++) LCD_Write_Char(celebrate[i]);
				LCD_Write_Instruction(0x80); //end receiving char bits
			}

			else {
				//defeat animation
				//update custom character on CGRAM @address 0x05 to be crash frame
				LCD_Write_Instruction(0x68);
				for(i=0; i<8; i++) LCD_Write_Char(crash[i]);
				LCD_Write_Instruction(0x80); //end receiving char bits
			}

			//update high score for number of tricks to be the achieved score if achieved score is higher
			if(numTricks > highnumTricks) {
				highnumTricks = numTricks;
			}

			//set timers for gameover animations to 0
			cnt = 0;
			int scoreTimer = 0;
			uint8_t jumpTimer = 0;

			//car for game over and not high score animation pos initialized to 15
			int carPos = 15;

			while(1) { //waits for user to press button to play again
				if(cpress) {
					gameOver = 0; //reset game over flag
					gameSetup(); //call to gameSetup() again to initialize game for another playthrough
					break; //exit this while loop
				}

				//displays score and high score for 15 cycles
				if(scoreTimer == 0) {
					for(i = 0; i < sizeof(gameovertop); i++) sky[i] = gameovertop[i];
					cnt = sizeof(gameovertop) - 1;
					for(i = 100; i > 0; i /= 10){
						sky[cnt] = ((score / i) % 10) + '0';
						cnt++;
					}

					for(i = 0; i < sizeof(gameoverbottom); i++) ground[i] = gameoverbottom[i];
					cnt = sizeof(gameoverbottom) - 1;
					for(i = 100; i > 0; i /= 10){
						ground[cnt] = ((highscore / i) % 10) + '0';
						cnt++;
					}
				}

				//15 cycles after score/highscore display,
				//if player performed a trick, display numTricks and highNumTricks for 15 cycles
				if(scoreTimer > 15 && numTricks > 0) {
					for(i = 0; i < sizeof(gameovertrick); i++) sky[i] = gameovertrick[i];
					cnt = sizeof(gameovertrick) - 1;
					for(i = 10; i > 0; i /= 10){
						sky[cnt] = ((numTricks / i) % 10) + '0';
						cnt++;
					}

					cnt = sizeof(gameoverbottom);
					for(i = 10; i > 0; i /= 10){
						ground[cnt] = ((highnumTricks / i) % 10) + '0';
						cnt++;
					}

					ground[6] = ' '; //set ground pos 6 to blank to hide extra digit of highscore

					scoreTimer = -15; //set score timer to -15 so score/highscore will be
									  //displayed again 15 cycles after these scores
				}

				//celebration animation
				//player jumps in celebration every 8 cycles if highscore achieved
				if (newHS && jumpTimer > 8) {
					if (sky[14] == 0x06) {
						sky[14] = ' ';
						ground[14] = 0x05;
					}
					else {
						ground[14] = ' ';
						sky[14] = 0x06;
					}
					jumpTimer = 0; //timer reset upon jump
				}
				//if player did not achieve highscore, jump timer will increment past 8
				//after 13 cycles, car will drive over player
				//this statement continues to drive the car left each cycle that car's pos > 8
				else if (jumpTimer > 13 && carPos > 8) {
					ground[carPos] = 0x01;
					ground[carPos+1] = ' '; //last pos of car reset to blank (also deletes player)
					carPos--; //decrement carPos
				}
				else {
					//after car gets to pos 8, frame updated to rubble as it has crashed
					ground[carPos+1] = 0x05;
				}

				//update screen on each cyle
				LCD_Write_Instruction(lcd_SetCursor | lcd_LineOne);
				delay_us(100); //min 40us delay
				LCD_Write_String(sky);

				LCD_Write_Instruction(lcd_SetCursor | lcd_LineTwo);
				delay_us(100); //min 40us delay
				LCD_Write_String(ground);

				//increment timers on each cyle
				jumpTimer++;
				scoreTimer++;
			}
		}
	}
}


/*
 * Callback used by stm32f4_discovery_audio_codec.c.
 * Refer to stm32f4_discovery_audio_codec.h for more info.
 */
void EVAL_AUDIO_TransferComplete_CallBack(uint32_t pBuffer, uint32_t Size){
  /* TODO, implement your code here */
  return;
}

/*
 * Callback used by stm324xg_eval_audio_codec.c.
 * Refer to stm324xg_eval_audio_codec.h for more info.
 */
uint16_t EVAL_AUDIO_GetSampleCallBack(void){
  /* TODO, implement your code here */
  return -1;
}
