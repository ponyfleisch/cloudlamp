#include "application.h"

#define MANUAL 1
#define STROBE_WHITE 2
#define STROBE_COLOR 3
#define STROBE_CURRENT 4
#define PULSE_WHITE 5
#define PULSE_CURRENT 6
#define SWEEP_COLOR 7

int rOut = A1;
int gOut = A6;
int bOut = A0;

int wOut = A5;

int r, g, b, w;

int current_mode;

int strobe_on_delay, strobe_off_delay, pulse_period, pulse_direction, pulse_value;

int set(String);
int mode(String);
int variable(String);
void reset_all();
void set_all();

void setup()
{
	current_mode = MANUAL;

	strobe_on_delay = 10;
	strobe_off_delay = 100;
	pulse_period = 1;
	pulse_direction = 1;
	pulse_value = 0;

	pinMode(rOut, OUTPUT);
	pinMode(gOut, OUTPUT);
	pinMode(bOut, OUTPUT);
	pinMode(wOut, OUTPUT);

	Spark.function("set", set);
	Spark.function("variable", variable);
	Spark.function("mode", mode);
}

void loop()
{
	static int strobe_state = 1;

	if(current_mode == MANUAL) return;

	if(current_mode == STROBE_WHITE){
		if(strobe_state == 1){
			strobe_state = 0;
			analogWrite(wOut, 255);
			delay(strobe_on_delay);
		}else{
			strobe_state = 1;
			analogWrite(wOut, 0);
			delay(strobe_off_delay);
		}
	}else if(current_mode == STROBE_COLOR){
		if(strobe_state == 1){
			strobe_state = 0;
			analogWrite(rOut, rand()*255);
			analogWrite(gOut, rand()*255);
			analogWrite(bOut, rand()*255);
			delay(strobe_on_delay);
		}else{
			strobe_state = 1;
			analogWrite(rOut, 0);
			analogWrite(gOut, 0);
			analogWrite(bOut, 0);
			delay(strobe_off_delay);
		}
	}else if(current_mode == STROBE_CURRENT){
		if(strobe_state == 1){
			strobe_state = 0;
			set_all();
			delay(strobe_on_delay);
		}else{
			strobe_state = 1;
			reset_all();
			delay(strobe_off_delay);
		}
	}else if(current_mode == PULSE_WHITE){
		pulse_value += pulse_direction;
		if(pulse_value == 0 || pulse_value == 255){
			pulse_direction = -pulse_direction;
		}
		delay((pulse_period*1000)/510);
		analogWrite(wOut, pulse_value);
	}else if(current_mode == PULSE_CURRENT){
		pulse_value += pulse_direction;
		if(pulse_value == 0 || pulse_value == 255){
			pulse_direction = -pulse_direction;
		}
		delay((pulse_period*1000)/510);
		analogWrite(wOut, (w*pulse_value)/255);
		analogWrite(rOut, (r*pulse_value)/255);
		analogWrite(gOut, (g*pulse_value)/255);
		analogWrite(bOut, (b*pulse_value)/255);
	}
}

int set(String command) {
	uint32_t val = command.toInt();
	w = val & 0x000000ff;
	r = (val & 0x0000ff00) >> 8;
	g = (val & 0x00ff0000) >> 16;
	b = (val & 0xff000000) >> 24;
	set_all();
}

int mode(String command)
{
	if(command == "manual"){
		current_mode = MANUAL;
		set_all();
	}else if(command == "strobe_white"){
		current_mode = STROBE_WHITE;
		reset_all();
	}else if(command == "strobe_color"){
		current_mode = STROBE_COLOR;
		reset_all();
	}else if(command == "strobe_current"){
		current_mode = STROBE_CURRENT;
	}else if(command == "pulse_white"){
		current_mode = PULSE_WHITE;
	}else if(command == "pulse_current"){
		current_mode = PULSE_CURRENT;
	}

	return 1;
}

int variable(String command)
{
	String var = command.substring(0, command.indexOf(":"));
	String value = command.substring(command.indexOf(":")+1);

	if(var == "pulse_period"){
		pulse_period = value.toInt();
	}else if(var == "strobe_on_delay"){
		strobe_on_delay = value.toInt();
	}else if(var == "strobe_off_delay"){
		strobe_off_delay = value.toInt();
	}

	return 1;
}

void reset_all(){
	analogWrite(rOut, 0);
	analogWrite(bOut, 0);
	analogWrite(gOut, 0);
	analogWrite(wOut, 0);
}

void set_all(){
	analogWrite(rOut, r);
	analogWrite(bOut, b);
	analogWrite(gOut, g);
	analogWrite(wOut, w);
}