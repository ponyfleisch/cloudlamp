#include "application.h"
#include "SparkWebSocketServer.h"
extern char* itoa(int a, char* buffer, unsigned char radix);

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

unsigned char r, g, b, w;

int current_mode;

int strobe_on_delay, strobe_off_delay, pulse_period, pulse_direction, pulse_value, sweep_delay;

int set(String);
int mode(String);
int variable(String);
void reset_all();
void set_all();
void hsvtorgb(unsigned char*, unsigned char*, unsigned char*, unsigned char, unsigned char, unsigned char);
void handleWS(String&, String&);


TCPServer server = TCPServer(2525);

SparkWebSocketServer ws(server);


void setup()
{
	current_mode = MANUAL;
	strobe_on_delay = 10;
	strobe_off_delay = 100;
	pulse_period = 1;
	pulse_direction = 1;
	pulse_value = 0;
	sweep_delay = 100;

	pinMode(rOut, OUTPUT);
	pinMode(gOut, OUTPUT);
	pinMode(bOut, OUTPUT);
	pinMode(wOut, OUTPUT);

	Spark.function("set", set);
	Spark.function("variable", variable);
	Spark.function("mode", mode);

	// ip
	IPAddress ip = WiFi.localIP();
	static char ipAddress[15] = "";
	char octet[5];

	netapp_ipconfig(&ip_config);

	itoa(ip[0], octet, 10); strcat(ipAddress, octet); strcat(ipAddress, ".");
	itoa(ip[1], octet, 10); strcat(ipAddress, octet); strcat(ipAddress, ".");
	itoa(ip[2], octet, 10); strcat(ipAddress, octet); strcat(ipAddress, ".");
	itoa(ip[3], octet, 10); strcat(ipAddress, octet);
	Spark.variable("endpoint", ipAddress, STRING);

	Serial.begin(9600);
	CallBack cb = &handleWS;

	ws.setCallBack(cb);


	server.begin();
	Serial.println("Done setup");
	Serial.flush();
}

void loop()
{
	static int strobe_state = 1;
	unsigned char sweep_state = 0;

	ws.doIt();

	// delay(500);

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
	}else if(current_mode == SWEEP_COLOR){
		hsvtorgb(&r, &g, &b, sweep_state, 255, 255);
		sweep_state = (sweep_state+1)%256;
		set_all();
		delay(sweep_delay);
	}
}

int set(String command) {
	Serial.write("SET CALLED\n");
	Serial.flush();

	uint32_t val = command.toInt();
	w = val & 0x000000ff;
	r = (val & 0x0000ff00) >> 8;
	g = (val & 0x00ff0000) >> 16;
	b = (val & 0xff000000) >> 24;
	set_all();
	return 1;
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
		reset_all();
	}else if(command == "pulse_white"){
		current_mode = PULSE_WHITE;
		reset_all();
	}else if(command == "pulse_current"){
		current_mode = PULSE_CURRENT;
	}else if(command == "sweep_color"){
		current_mode = SWEEP_COLOR;
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

// http://web.mit.edu/storborg/Public/hsvtorgb.c
void hsvtorgb(unsigned char *r, unsigned char *g, unsigned char *b, unsigned char h, unsigned char s, unsigned char v)
{
	unsigned char region, fpart, p, q, t;

	if(s == 0) {
		/* color is grayscale */
		*r = *g = *b = v;
		return;
	}

	/* make hue 0-5 */
	region = h / 43;
	/* find remainder part, make it from 0-255 */
	fpart = (h - (region * 43)) * 6;

	/* calculate temp vars, doing integer multiplication */
	p = (v * (255 - s)) >> 8;
	q = (v * (255 - ((s * fpart) >> 8))) >> 8;
	t = (v * (255 - ((s * (255 - fpart)) >> 8))) >> 8;

	/* assign temp vars based on color cone region */
	switch(region) {
		case 0:
			*r = v; *g = t; *b = p; break;
		case 1:
			*r = q; *g = v; *b = p; break;
		case 2:
			*r = p; *g = v; *b = t; break;
		case 3:
			*r = p; *g = q; *b = v; break;
		case 4:
			*r = t; *g = p; *b = v; break;
		default:
			*r = v; *g = p; *b = q; break;
	}

	return;
}

void handleWS(String &cmd, String &result){
	Serial.println("YO!"); Serial.flush();
	result = "Yo!";
}