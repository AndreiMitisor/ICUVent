/*
 Name:		ArduinoSlave.ino
 Created:	3/25/2020 9:00:42 PM
 Author:	Alex Gradea
*/
#include <Arduino.h>
#include <ArduinoJson.hpp>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <Wire.h> 
#include "TeensyStep.h"

#include <ADC.h>
#include <ADC_util.h>


#define HWSERIAL Serial1

const int readPin = A4; // ADC0
const int readPin2 = A5; // ADC1

ADC* adc = new ADC(); // adc object

// stepper and  controller
constexpr int stpPin = 21, dirPin = 22;
StepControl controller;    // Use default settings 
Stepper motor(stpPin, dirPin);

const int LED = 13;
volatile int readPressure = 1;
int ledState = LOW;
volatile unsigned long blinkCount = 0;

int millisbetweenSteps = 1;

#define ENdrv 23
#define ZERO_POS 20 // Endstop

long counter = 0;
int State;
int LastState;

int menu_page = 0;

int maxBreathsPM = 40;
int minBreathsPM = 3;

int maxAirVol = 600;
int minAirVol = 200;

int maxTimeBB = 100;
int minTimeBB = 0;

int maxInhaleRatio = 3;
int minInhaleRatio = 1;

long minMotionLength = 10;
long maxMotionLength = 11000; // hard limiting the movement of the mechanics

int homePosition = 100;
int offSwitchPos = 1500;
int zeroed = 0;
int ventPos = 0;

int minStepperSpeed = 2000;
int maxStepperSpeed = 30000;
int acceleration = 100000;

unsigned long last_watchdog_update = 0;
unsigned long last_sensorWatchdog_update = 0;

bool needInhale = true;
bool needExhale = false;

bool inhaling = false;
bool exhaling = false;
bool motorShouldRun = false;

String input;

float adcMultiplier = 0.1875;
int16_t readVal1, readVal2;


unsigned long lastDebug = 0;
unsigned long debugInhaleTime = 0;
unsigned long debugExhaleTime = 0;

struct config
{
	int breathsPerMinute;
	int airVol;
	int inhaleExhaleRatio;
	long motionLength;
	int stepperSpeed;
};

typedef struct config Config;
Config conf;

void setup() {

	motor.setMaxSpeed(maxStepperSpeed);
	motor.setAcceleration(acceleration);

	pinMode(readPin, INPUT);
	pinMode(readPin2, INPUT);

	// Setting ADC0
	adc->adc0->setAveraging(32); // set number of averages
	adc->adc0->setResolution(16); // set bits of resolution
	adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS); // change the conversion speed
	adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); // change the sampling speed

	pinMode(LED, OUTPUT);
	pinMode(ENdrv, OUTPUT);
	digitalWrite(ENdrv, HIGH); // Stepper driver ENABLE ! 

	//Define the pins as inputs
	pinMode(ZERO_POS, INPUT_PULLUP);
	Serial.begin(9600);
	HWSERIAL.begin(115200); //Start serial com so we could print the value on the serial monitor

	loadOrwriteDefaults();

}

void loop() {

	if (!zeroed) {
		delay(100);
		zero_position();
		motorShouldRun = true;
		needInhale = true;
	}



	serialCom();
	
	unsigned long current_millis = millis();
	unsigned long lastPrint;

	
	float timeToInhale = getInhaleTime(conf.breathsPerMinute);
	float timeToExhale = getExhaleTime(conf.breathsPerMinute);




	sensorsWatchdog();

	if (needInhale || inhaling) {

		if (needInhale) {
			Serial.print("This should be inhale time: ");
			Serial.println(timeToInhale);

			debugInhaleTime = millis();

			setMotorSpeed(timeToInhale);
			setAirVolume();
			ventWatchdog(conf.motionLength, "Inhale");
			needInhale = false;
		}
		
		if (inhaling) {
			if (motor.getPosition() == conf.motionLength) {

				needExhale = true;
				inhaling = false;
				Serial.print("Time to inhale: ");
				Serial.println(current_millis - debugInhaleTime);

				Serial.print("At stepper Speed: ");
				Serial.println(conf.stepperSpeed);

			}

		}

	}


	if (needExhale || exhaling) {

		//Serial.println("I am getting here 1");

		if (needExhale) {
			debugExhaleTime = millis();
			Serial.print("This should be exhale time: ");
			Serial.println(timeToExhale);

			setMotorSpeed(timeToExhale);
			ventWatchdog(homePosition, "Exhale");
			needExhale = false;
		}

		if (exhaling) {
			//Serial.println("I am getting here 3");

			//Serial.print("This is motor Position: ");
			//Serial.println(motor.getPosition());

			if (motor.getPosition() == homePosition) {
				needInhale = true;
				exhaling = false;

				Serial.println("Time to exhale: ");
				Serial.println(current_millis - debugExhaleTime);

				Serial.println("At stepper Speed: ");

				Serial.println(conf.stepperSpeed);
				Serial.println("");
				Serial.println("");

			}

		}

	}

	if (motorShouldRun) {
		//ventMotor.run();
		homeSwitchCheck();
	}

	if (!motorShouldRun) {
		//ventMotor.stop();
	}
	


}
