/* -*- c++ -*- */

/*
    Reprap firmware based on Sprinter and grbl.
 Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 This firmware is a mashup between Sprinter and grbl.
  (https://github.com/kliment/Sprinter)
  (https://github.com/simen/grbl/tree)

 It has preliminary support for Matthew Roberts advance algorithm
    http://reprap.org/pipermail/reprap-dev/2011-May/003323.html
 */

#include "Marlin.h"

#include "ultralcd.h"
#include "planner.h"
#include "stepper.h"
#include "motion_control.h"
#include "cardreader.h"
#include "watchdog.h"
#include "ConfigurationStore.h"
#include "language.h"
#include "pins_arduino.h"
#include "Base64.h"

#if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
	#include <SPI.h>
#endif

#define VERSION_STRING  "1.0.0"

// look here for descriptions of gcodes: http://linuxcnc.org/handbook/gcode/g-code.html
// http://objects.reprap.org/wiki/Mendel_User_Manual:_RepRapGCodes

//Implemented Codes
//-------------------
// G0  -> G1
// G1  - Coordinated Movement X Y Z E
// G2  - CW ARC
// G3  - CCW ARC
// G4  - Dwell S<seconds> or P<milliseconds>
// G28 - Home all Axis
// G90 - Use Absolute Coordinates
// G91 - Use Relative Coordinates
// G92 - Set current position to cordinates given

// M Codes
// M0   - Unconditional stop - Wait for user to press a button on the LCD (Only if ULTRA_LCD is enabled)
// M1   - Same as M0
// M5	- Stop the laser firing
// M17  - Enable/Power all stepper motors
// M18  - Disable all stepper motors; same as M84
// M20  - List SD card
// M21  - Init SD card
// M22  - Release SD card
// M23  - Select SD file (M23 filename.g)
// M24  - Start/resume SD print
// M25  - Pause SD print
// M26  - Set SD position in bytes (M26 S12345)
// M27  - Report SD print status
// M28  - Start SD write (M28 filename.g)
// M29  - Stop SD write
// M30  - Delete file from SD (M30 filename.g)
// M31  - Output time since last M109 or SD card start to serial
// M32  - Select file and start SD print (Can be used when printing from SD card)
// M42  - Change pin status via gcode Use M42 Px Sy to set pin x to value y, when omitting Px the onboard led will be used.
// M82  - Set E codes absolute (default)
// M83  - Set E codes relative while in Absolute Coordinates (G90) mode
// M84  - Disable steppers until next move,
//        or use S<seconds> to specify an inactivity timeout, after which the steppers will be disabled.  S0 to disable the timeout.
// M85  - Set inactivity shutdown timer with parameter S<seconds>. To disable set zero (default)
// M92  - Set axis_steps_per_unit - same syntax as G92
// M106 - Fan on					(****REPURPOSE for extractor fan?****)
// M107 - Fan off					(****REPURPOSE for extractor fan?****)
// M114 - Output current position to serial port
// M115 - Capabilities string
// M117 - display message
// M119 - Output Endstop status to serial port
// M201 - Set max acceleration in units/s^2 for print moves (M201 X1000 Y1000)			(****REALLY?****)
// M202 - Set max acceleration in units/s^2 for travel moves (M202 X1000 Y1000) Unused in Marlin!!	(****REALLY?****)
// M203 - Set maximum feedrate that your machine can sustain (M203 X200 Y200 Z300 E10000) in mm/sec	(****DELETE****)
// M204 - Set default acceleration: S normal moves T filament only moves (M204 S3000 T7000) im mm/sec^2  also sets minimum segment time in ms (B20000) to prevent buffer underruns and M20 minimum feedrate (****REALLY?****)
// M205 -  advanced settings:  minimum travel speed S=while printing T=travel only,  B=minimum segment time X= maximum xy jerk, Z=maximum Z jerk, E=maximum E jerk		(****REALLY?****)
// M206 - set additional homeing offset
// M220 S<factor in percent>- set speed factor override percentage
// M221 S<factor in percent>- set extrude factor override percentage				(****REPURPOSE FOR LASER SCALE?****)
// M250 - Set LCD contrast C<contrast value> (value 0..63)					(####REALLY?####)
// M300 - Play beepsound S<frequency Hz> P<duration ms>
// M400 - Finish all moves
// M500 - stores paramters in EEPROM
// M501 - reads parameters from EEPROM (if you need reset them after you changed them temporarily).
// M502 - reverts to the default "factory settings".  You still need to store them in EEPROM afterwards if you want to.
// M503 - print the current settings (from memory not from eeprom)
// M540 - Use S[0|1] to enable or disable the stop SD card print on endstop hit (requires ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED)
// M649 -
// M650 -
// M666 - set delta endstop adjustemnt
// M907 - Set digital trimpot motor current using axis codes.		(####WHAT DOES THIS DO?####)
// M908 - Control digital trimpot directly.				(####WHAT DOES THIS DO?####)
// M350 - Set microstepping mode.
// M351 - Toggle MS1 MS2 pins directly.
// M928 - Start SD logging (M928 filename.g) - ended by M29
// M999 - Restart after being stopped by error

//Stepper Movement Variables

//===========================================================================
//=============================imported variables============================
//===========================================================================


//===========================================================================
//=============================public variables=============================
//===========================================================================
#ifdef SDSUPPORT
	CardReader card;
#endif
float homing_feedrate[] = HOMING_FEEDRATE;
bool axis_relative_modes[] = AXIS_RELATIVE_MODES;
int feedmultiply=100; //100->1 200->2
int saved_feedmultiply;
int extrudemultiply=100; //100->1 200->2
float current_position[NUM_AXIS] = { 0.0, 0.0, 0.0 };
bool has_axis_homed[NUM_AXIS] = {false, false, false };
float add_homeing[3]= {0,0,0};
float min_pos[3] = { X_MIN_POS, Y_MIN_POS, Z_MIN_POS };
float max_pos[3] = { X_MAX_POS, Y_MAX_POS, Z_MAX_POS };

// Extruder offset
int fanSpeed=0;

#ifdef ULTIPANEL
	bool powersupply = true;
#endif

//===========================================================================
//=============================private variables=============================
//===========================================================================
const char axis_codes[NUM_AXIS] = {'X', 'Y', 'Z'};
static float destination[NUM_AXIS] = {  0.0, 0.0, 0.0};
static float offset[3] = {0.0, 0.0, 0.0};
static bool home_all_axis = true;
static float feedrate = 5000.0, next_feedrate, saved_feedrate;
static long gcode_N, gcode_LastN, Stopped_gcode_LastN = 0;

static bool relative_mode = false;  //Determines Absolute or Relative Coordinates

static char cmdbuffer[BUFSIZE][MAX_CMD_SIZE];
static bool fromsd[BUFSIZE];
static int bufindr = 0;
static int bufindw = 0;
static int buflen = 0;
//static int i = 0;
static char serial_char;
static int serial_count = 0;
static boolean comment_mode = false;
static char* strchr_pointer; // just a pointer to find chars in the cmd string like X, Y, Z, E, etc

const int sensitive_pins[] = SENSITIVE_PINS; // Sensitive pin list for M42

//static float tt = 0;
//static float bt = 0;

//Inactivity shutdown variables
static unsigned long previous_millis_cmd = 0;
static unsigned long max_inactive_time = 0;
static unsigned long stepper_inactive_time = DEFAULT_STEPPER_DEACTIVE_TIME*1000l;

unsigned long starttime=0;
unsigned long stoptime=0;

static uint8_t tmp_extruder;


bool Stopped=false;

bool CooldownNoWait = true;
bool target_direction;

//===========================================================================
//=============================ROUTINES=============================
//===========================================================================

void get_arc_coordinates();
bool setTargetedHotend(int code);

void serial_echopair_P(const char* s_P, float v)
{ serialprintPGM(s_P); SERIAL_ECHO(v); }
void serial_echopair_P(const char* s_P, double v)
{ serialprintPGM(s_P); SERIAL_ECHO(v); }
void serial_echopair_P(const char* s_P, long v)
{ serialprintPGM(s_P); SERIAL_ECHO(v); }
void serial_echopair_P(const char* s_P, unsigned long v)
{ serialprintPGM(s_P); SERIAL_ECHO(v); }

extern "C"
{
	extern unsigned int __bss_end;
	extern unsigned int __heap_start;
	extern void* __brkval;

	int freeMemory()
	{
		int free_memory;

		if((int) __brkval == 0)
		{ free_memory = ((int) &free_memory) - ((int) &__bss_end); }
		else
		{ free_memory = ((int) &free_memory) - ((int) __brkval); }

		return free_memory;
	}
}

//adds an command to the main command buffer
//thats really done in a non-safe way.
//needs overworking someday
void enquecommand(const char* cmd)
{
	if(buflen < BUFSIZE)
	{
		//this is dangerous if a mixing of serial and this happsens
		strcpy(& (cmdbuffer[bufindw][0]),cmd);
		SERIAL_ECHO_START;
		SERIAL_ECHOPGM("enqueing \"");
		SERIAL_ECHO(cmdbuffer[bufindw]);
		SERIAL_ECHOLNPGM("\"");
		bufindw= (bufindw + 1) %BUFSIZE;
		buflen += 1;
	}
}

void enquecommand_P(const char* cmd)
{
	if(buflen < BUFSIZE)
	{
		//this is dangerous if a mixing of serial and this happsens
		strcpy_P(& (cmdbuffer[bufindw][0]),cmd);
		SERIAL_ECHO_START;
		SERIAL_ECHOPGM("enqueing \"");
		SERIAL_ECHO(cmdbuffer[bufindw]);
		SERIAL_ECHOLNPGM("\"");
		bufindw= (bufindw + 1) %BUFSIZE;
		buflen += 1;
	}
}

void setup_killpin()
{
#if defined(KILL_PIN) && KILL_PIN > -1
	pinMode(KILL_PIN,INPUT);
	WRITE(KILL_PIN,HIGH);
#endif
}

void setup_photpin()
{
#if defined(PHOTOGRAPH_PIN) && PHOTOGRAPH_PIN > -1
	SET_OUTPUT(PHOTOGRAPH_PIN);
	WRITE(PHOTOGRAPH_PIN, LOW);
#endif
}

void suicide()
{
#if defined(SUICIDE_PIN) && SUICIDE_PIN > -1
	SET_OUTPUT(SUICIDE_PIN);
	WRITE(SUICIDE_PIN, LOW);
#endif
}

void setup()
{
	setup_killpin();
	MYSERIAL.begin(BAUDRATE);
	SERIAL_PROTOCOLLNPGM("start");
	SERIAL_ECHO_START;

	// Check startup - does nothing if bootloader sets MCUSR to 0
	byte mcu = MCUSR;
	if(mcu & 1) { SERIAL_ECHOLNPGM(MSG_POWERUP); }
	if(mcu & 2) { SERIAL_ECHOLNPGM(MSG_EXTERNAL_RESET); }
	if(mcu & 4) { SERIAL_ECHOLNPGM(MSG_BROWNOUT_RESET); }
	if(mcu & 8) { SERIAL_ECHOLNPGM(MSG_WATCHDOG_RESET); }
	if(mcu & 32) { SERIAL_ECHOLNPGM(MSG_SOFTWARE_RESET); }
	MCUSR=0;

	SERIAL_ECHOPGM(MSG_MARLIN);
	SERIAL_ECHOLNPGM(VERSION_STRING);
#ifdef STRING_VERSION_CONFIG_H
#ifdef STRING_CONFIG_H_AUTHOR
	SERIAL_ECHO_START;
	SERIAL_ECHOPGM(MSG_CONFIGURATION_VER);
	SERIAL_ECHOPGM(STRING_VERSION_CONFIG_H);
	SERIAL_ECHOPGM(MSG_AUTHOR);
	SERIAL_ECHOLNPGM(STRING_CONFIG_H_AUTHOR);
	SERIAL_ECHOPGM("Compiled: ");
	SERIAL_ECHOLNPGM(__DATE__);
#endif
#endif
	SERIAL_ECHO_START;
	SERIAL_ECHOPGM(MSG_FREE_MEMORY);
	SERIAL_ECHO(freeMemory());
	SERIAL_ECHOPGM(MSG_PLANNER_BUFFER_BYTES);
	SERIAL_ECHOLN((int) sizeof(block_t) *BLOCK_BUFFER_SIZE);
	for(int8_t i = 0; i < BUFSIZE; i++)
	{
		fromsd[i] = false;
	}

	// loads data from EEPROM if available else uses defaults (and resets step acceleration rate)
	Config_RetrieveSettings();

	plan_init();  // Initialize planner;
	watchdog_init();
	st_init();    // Initialize stepper, this enables interrupts!
	setup_photpin();

	lcd_init();
	tone(BEEPER, 1500);
	_delay_ms(1000);	// wait 1sec to display the splash screen
	noTone(BEEPER);

#if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1
	SET_OUTPUT(CONTROLLERFAN_PIN);    //Set pin used for driver cooling fan
#endif

	laser_init();

	// Start up our lcd button update interrupt
	OCR0B = 128;
	TIMSK0 |= (1<<OCIE0B);
}

// This interrups used to do the temperature monitoring. It now only does button update.
// Maybe this could be repurposed for other tasks
ISR(TIMER0_COMPB_vect)
{
	lcd_buttons_update();
}

void loop()
{
	if(buflen < (BUFSIZE-1))
	{ get_command(); }
#ifdef SDSUPPORT
	card.checkautostart(false);
#endif
	if(buflen)
	{
#ifdef SDSUPPORT
		if(card.saving)
		{
			if(strstr_P(cmdbuffer[bufindr], PSTR("M29")) == NULL)
			{
				card.write_command(cmdbuffer[bufindr]);
				if(card.logging)
				{
					process_commands();
				}
				else
				{
					SERIAL_PROTOCOLLNPGM(MSG_OK);
				}
			}
			else
			{
				card.closefile();
				SERIAL_PROTOCOLLNPGM(MSG_FILE_SAVED);
			}
		}
		else
		{
			process_commands();
		}
#else
		process_commands();
#endif //SDSUPPORT
		buflen = (buflen-1);
		bufindr = (bufindr + 1) %BUFSIZE;
	}

	manage_inactivity();
	checkHitEndstops();
	lcd_update();
}

void get_command()
{
	while(MYSERIAL.available() > 0  && buflen < BUFSIZE)
	{
		serial_char = MYSERIAL.read();
		if(serial_char == '\n' ||
		        serial_char == '\r' ||
		        (serial_char == ':' && comment_mode == false) ||
		        serial_count >= (MAX_CMD_SIZE - 1))
		{
			if(!serial_count)    //if empty line
			{
				comment_mode = false; //for new command
				return;
			}
			cmdbuffer[bufindw][serial_count] = 0; //terminate string
			if(!comment_mode)
			{
				comment_mode = false; //for new command
				fromsd[bufindw] = false;

				//Turnkey - Changed <=6 to <=8 to allow up to 999999 lines of raster data to be transmitted per raster node
				//This area needs improving for stability.
				if((strstr_P(cmdbuffer[bufindw], PSTR("G7")) == NULL && strchr(cmdbuffer[bufindw], 'N') != NULL) ||               //For non G7 commands, run as normal.
				        ((strstr_P(cmdbuffer[bufindw], PSTR("G7")) != NULL) &&
				         ((strstr_P(cmdbuffer[bufindw], PSTR("G7")) - strchr(cmdbuffer[bufindw], 'N')  <=8) &&
				          (strstr_P(cmdbuffer[bufindw], PSTR("G7")) - strchr(cmdbuffer[bufindw], 'N')  >0)))
				  )
				{
					strchr_pointer = strchr(cmdbuffer[bufindw], 'N');
					gcode_N = (strtol(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL, 10));
					if(gcode_N != gcode_LastN+1 && (strstr_P(cmdbuffer[bufindw], PSTR("M110")) == NULL))
					{
						SERIAL_ERROR_START;
						SERIAL_ERRORPGM(MSG_ERR_LINE_NO);
						SERIAL_ERRORLN(gcode_LastN);
						//Serial.println(gcode_N);
						FlushSerialRequestResend();
						serial_count = 0;
						return;
					}

					if(strchr(cmdbuffer[bufindw], '*') != NULL)
					{
						byte checksum = 0;
						byte count = 0;
						while(cmdbuffer[bufindw][count] != '*') { checksum = checksum^cmdbuffer[bufindw][count++]; }
						strchr_pointer = strchr(cmdbuffer[bufindw], '*');

						if((int)(strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL)) != checksum)
						{
							SERIAL_ERROR_START;
							SERIAL_ERRORPGM(MSG_ERR_CHECKSUM_MISMATCH);
							SERIAL_ERRORLN(gcode_LastN);
							FlushSerialRequestResend();
							serial_count = 0;
							return;
						}
						//if no errors, continue parsing
					}
					else
					{
						SERIAL_ERROR_START;
						SERIAL_ERRORPGM(MSG_ERR_NO_CHECKSUM);
						SERIAL_ERRORLN(gcode_LastN);
						FlushSerialRequestResend();
						serial_count = 0;
						return;
					}

					gcode_LastN = gcode_N;
					//if no errors, continue parsing
				}
				else  // if we don't receive 'N' but still see '*'
				{
					if((strchr(cmdbuffer[bufindw], '*') != NULL))
					{
						SERIAL_ERROR_START;
						SERIAL_ERRORPGM(MSG_ERR_NO_LINENUMBER_WITH_CHECKSUM);
						SERIAL_ERRORLN(gcode_LastN);
						serial_count = 0;
						return;
					}
				}
				if((strchr(cmdbuffer[bufindw], 'G') != NULL))
				{
					strchr_pointer = strchr(cmdbuffer[bufindw], 'G');
					switch((int)((strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL))))
					{
					case 0:
					case 1:
					case 2:
					case 3:
						if(Stopped == false)    // If printer is stopped by an error the G[0-3] codes are ignored.
						{
#ifdef SDSUPPORT
							if(card.saving)
							{ break; }
#endif //SDSUPPORT
							SERIAL_PROTOCOLLNPGM(MSG_OK);
						}
						else
						{
							SERIAL_ERRORLNPGM(MSG_ERR_STOPPED);
							LCD_MESSAGEPGM(MSG_STOPPED);
						}
						break;
					default:
						break;
					}

				}
				bufindw = (bufindw + 1) %BUFSIZE;
				buflen += 1;
			}
			serial_count = 0; //clear buffer
		}
		else
		{
			if(serial_char == ';') { comment_mode = true; }
			if(!comment_mode) { cmdbuffer[bufindw][serial_count++] = serial_char; }
		}
	}
#ifdef SDSUPPORT
	if(!card.sdprinting || serial_count!=0)
	{
		return;
	}
	while(!card.eof()  && buflen < BUFSIZE)
	{
		int16_t n=card.get();
		serial_char = (char) n;
		if(serial_char == '\n' ||
		        serial_char == '\r' ||
		        (serial_char == ':' && comment_mode == false) ||
		        serial_count >= (MAX_CMD_SIZE - 1) ||n==-1)
		{
			if(card.eof())
			{
				SERIAL_PROTOCOLLNPGM(MSG_FILE_PRINTED);
				stoptime=millis();
				char time[30];
				unsigned long t= (stoptime-starttime) /1000;
				int hours, minutes;
				minutes= (t/60) %60;
				hours=t/60/60;
				sprintf_P(time, PSTR("%i hours %i minutes"),hours, minutes);
				SERIAL_ECHO_START;
				SERIAL_ECHOLN(time);
				lcd_setstatus(time);
				card.printingHasFinished();
				card.checkautostart(true);

			}
			if(!serial_count)
			{
				comment_mode = false; //for new command
				return; //if empty line
			}
			cmdbuffer[bufindw][serial_count] = 0; //terminate string
//      if(!comment_mode){
			fromsd[bufindw] = true;
			buflen += 1;
			bufindw = (bufindw + 1) %BUFSIZE;
//      }
			comment_mode = false; //for new command
			serial_count = 0; //clear buffer
		}
		else
		{
			if(serial_char == ';') { comment_mode = true; }
			if(!comment_mode) { cmdbuffer[bufindw][serial_count++] = serial_char; }
		}
	}

#endif //SDSUPPORT

}


float code_value()
{
	return (strtod(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL));
}

long code_value_long()
{
	return (strtol(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL, 10));
}

bool code_seen(char code)
{
	strchr_pointer = strchr(cmdbuffer[bufindr], code);
	return (strchr_pointer != NULL);   //Return True if a character was found
}

#define DEFINE_PGM_READ_ANY(type, reader)       \
	static inline type pgm_read_any(const type *p)  \
	{ return pgm_read_##reader##_near(p); }

DEFINE_PGM_READ_ANY(float,       float);
DEFINE_PGM_READ_ANY(signed char, byte);

#define XYZ_CONSTS_FROM_CONFIG(type, array, CONFIG) \
	static const PROGMEM type array##_P[3] =        \
	        { X_##CONFIG, Y_##CONFIG, Z_##CONFIG };     \
	static inline type array(int axis)          \
	{ return pgm_read_any(&array##_P[axis]); }

XYZ_CONSTS_FROM_CONFIG(float, base_min_pos,    MIN_POS);
XYZ_CONSTS_FROM_CONFIG(float, base_max_pos,    MAX_POS);
XYZ_CONSTS_FROM_CONFIG(float, base_home_pos,   HOME_POS);
XYZ_CONSTS_FROM_CONFIG(float, max_length,      MAX_LENGTH);
XYZ_CONSTS_FROM_CONFIG(float, home_retract_mm, HOME_RETRACT_MM);
XYZ_CONSTS_FROM_CONFIG(signed char, home_dir,  HOME_DIR);

static void axis_is_at_home(int axis)
{
	current_position[axis] = base_home_pos(axis) + add_homeing[axis];
	min_pos[axis] =          base_min_pos(axis) + add_homeing[axis];
	max_pos[axis] =          base_max_pos(axis) + add_homeing[axis];
}

static void homeaxis(int axis)
{
#define HOMEAXIS_DO(LETTER) \
	((LETTER##_MIN_PIN > -1 && LETTER##_HOME_DIR==-1) || (LETTER##_MAX_PIN > -1 && LETTER##_HOME_DIR==1))

	if(axis==X_AXIS ? HOMEAXIS_DO(X) :
	        axis==Y_AXIS ? HOMEAXIS_DO(Y) :
	        axis==Z_AXIS ? HOMEAXIS_DO(Z) :
	        0)
	{
		int axis_home_dir = home_dir(axis);

		// Engage Servo endstop if enabled
		has_axis_homed[axis] = true;
		current_position[axis] = 0;
		plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
		destination[axis] = 1.5 * max_length(axis) * axis_home_dir;
		feedrate = homing_feedrate[axis];
		plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);
		st_synchronize();

		current_position[axis] = 0;
		plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
		destination[axis] = -home_retract_mm(axis) * axis_home_dir;
		plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);
		st_synchronize();

		destination[axis] = 2*home_retract_mm(axis) * axis_home_dir;
		feedrate = homing_feedrate[axis]/2 ;

		plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);

		st_synchronize();
		axis_is_at_home(axis);
		destination[axis] = current_position[axis];
		feedrate = 0.0;
		endstops_hit_on_purpose();

		// Retract Servo endstop if enabled
	}
}
#define HOMEAXIS(LETTER) homeaxis(LETTER##_AXIS)

void process_commands()
{
	unsigned long codenum; //throw away variable
	char* starpos = NULL;

	if(code_seen('G'))
	{
		switch((int) code_value())
		{

		// G Code overview
		// G0  - Coordinated Movement X Y Z
		// G1  - Coordinated Movement X Y Z with laser params
		// G2  - CW ARC
		// G3  - CCW ARC
		// G4  - Dwell S<seconds> or P<milliseconds>
		// G28 - Home all Axis
		// G90 - Use Absolute Coordinates
		// G91 - Use Relative Coordinates
		// G92 - Set current position to cordinates given

		//////////////////////////////////////////////////////////////////////
		// G0 Move to X Y Z
		// X: XPos to move to
		// Y: YPos to move to
		// Z: ZPos to move to
		//////////////////////////////////////////////////////////////////////
		case 0:
			if(Stopped == false)
			{
				get_coordinates(); // For X Y Z E F
				prepare_move();
				//ClearToSend();
				return;
			}

		//////////////////////////////////////////////////////////////////////
		// G1 Move to X Y Z with laser firing
		// X: XPos to move to
		// Y: YPos to move to
		// Z: ZPos to move to
		// S: Laser intensity %
		// L: Laser duration ??????
		// P: Laser PPM ?????????
		// B: Laser mode ?????
		//////////////////////////////////////////////////////////////////////
		case 1:
			if(Stopped == false)
			{
				get_coordinates(); // For X Y Z E F

#ifdef LASER_FIRE_G1
				if(code_seen('S') && !IsStopped()) { laser.intensity = (float) code_value(); }
				if(code_seen('L') && !IsStopped()) { laser.duration = (unsigned long) labs(code_value()); }
				if(code_seen('P') && !IsStopped()) { laser.ppm = (float) code_value(); }
				if(code_seen('B') && !IsStopped()) { laser_set_mode((int) code_value()); }

				laser.status = LASER_ON;
				laser.fired = LASER_FIRE_G1;
#endif // LASER_FIRE_G1

				prepare_move();

#ifdef LASER_FIRE_G1
				laser.status = LASER_OFF;
#endif // LASER_FIRE_G1

				//ClearToSend();
				return;
			}

		//////////////////////////////////////////////////////////////////////
		// G2 CW Arc to X Y Z with laser firing
		// X: XPos to move to
		// Y: YPos to move to
		// Z: ZPos to move to
		// I: ??????
		// J: ??????
		// S: Laser intensity %
		// L: Laser duration ??????
		// P: Laser PPM ?????????
		// B: Laser mode ?????
		//////////////////////////////////////////////////////////////////////
		case 2: // G2  - CW ARC
			if(Stopped == false)
			{
				get_arc_coordinates();

#ifdef LASER_FIRE_G1
				if(code_seen('S') && !IsStopped()) { laser.intensity = (float) code_value(); }
				if(code_seen('L') && !IsStopped()) { laser.duration = (unsigned long) labs(code_value()); }
				if(code_seen('P') && !IsStopped()) { laser.ppm = (float) code_value(); }
				if(code_seen('B') && !IsStopped()) { laser_set_mode((int) code_value()); }

				laser.status = LASER_ON;
				laser.fired = LASER_FIRE_G1;
#endif // LASER_FIRE_G1

				prepare_arc_move(true);

#ifdef LASER_FIRE_G1
				laser.status = LASER_OFF;
#endif // LASER_FIRE_G1

				return;
			}

		//////////////////////////////////////////////////////////////////////
		// G3 CCW Arc to X Y Z with laser firing
		// X: XPos to move to
		// Y: YPos to move to
		// Z: ZPos to move to
		// I: ??????
		// J: ??????
		// S: Laser intensity %
		// L: Laser duration ??????
		// P: Laser PPM ?????????
		// B: Laser mode ?????
		//////////////////////////////////////////////////////////////////////
		case 3:
			if(Stopped == false)
			{
				get_arc_coordinates();

#ifdef LASER_FIRE_G1
				if(code_seen('S') && !IsStopped()) { laser.intensity = (float) code_value(); }
				if(code_seen('L') && !IsStopped()) { laser.duration = (unsigned long) labs(code_value()); }
				if(code_seen('P') && !IsStopped()) { laser.ppm = (float) code_value(); }
				if(code_seen('B') && !IsStopped()) { laser_set_mode((int) code_value()); }

				laser.status = LASER_ON;
				laser.fired = LASER_FIRE_G1;
#endif // LASER_FIRE_G1

				prepare_arc_move(false);

#ifdef LASER_FIRE_G1
				laser.status = LASER_OFF;
#endif // LASER_FIRE_G1

				return;
			}

		//////////////////////////////////////////////////////////////////////
		// G4 Dwell
		// P: Milliseconds to wait
		// S: Seconds to wait
		//////////////////////////////////////////////////////////////////////
		case 4:
			LCD_MESSAGEPGM(MSG_DWELL);
			codenum = 0;
			if(code_seen('P')) { codenum = code_value(); }       // milliseconds to wait
			if(code_seen('S')) { codenum = code_value() * 1000; }       // seconds to wait

			st_synchronize();
			codenum += millis();  // keep track of when we started waiting
			previous_millis_cmd = millis();
			while(millis()  < codenum)
			{
				manage_inactivity();
				lcd_update();
			}
			break;

		//////////////////////////////////////////////////////////////////////
		// G7 Trace rster line
		// L: Raw length
		// $: Increment Y axis
		// D: BASE64 encoded raster data
		//		???? Missing data
		//////////////////////////////////////////////////////////////////////
		case 7: //G7 Execute raster line
			if(code_seen('L'))
			{ laser.raster_raw_length = int (code_value()); }
			
			if(code_seen('$'))
			{
				laser.raster_direction = (bool) code_value();
				destination[Y_AXIS] = current_position[Y_AXIS] + (laser.raster_mm_per_pulse * laser.raster_aspect_ratio);   // increment Y axis
			}
			
			if(code_seen('D')) 
			{ laser.raster_num_pixels = base64_decode(laser.raster_data, &cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], laser.raster_raw_length); }
			
			if(!laser.raster_direction)
			{
				destination[X_AXIS] = current_position[X_AXIS] - (laser.raster_mm_per_pulse * laser.raster_num_pixels);
#if defined( LASER_DIAGNOSTICS )
				SERIAL_ECHO_START;
				SERIAL_ECHOLN("Negative Raster Line");
#endif
			}
			else
			{
				destination[X_AXIS] = current_position[X_AXIS] + (laser.raster_mm_per_pulse * laser.raster_num_pixels);
#if defined( LASER_DIAGNOSTICS )
				SERIAL_ECHO_START;
				SERIAL_ECHOLN("Positive Raster Line");
#endif
			}

			laser.ppm = 1 / laser.raster_mm_per_pulse; //number of pulses per millimetre
			laser.duration = (1000000 / (feedrate / 60)) / laser.ppm;     // (1 second in microseconds / (time to move 1mm in microseconds)) / (pulses per mm) = Duration of pulse, taking into account feedrate as speed and ppm

			laser.mode = RASTER;
			laser.status = LASER_ON;
			laser.fired = RASTER;
			prepare_move();

			break;

		//////////////////////////////////////////////////////////////////////
		// G28 Home all axis (optionally via an intermediate position)
		// X: XPos to move via (optional)
		// Y: YPos to move via (optional)
		// Z: ZPos to move via (optional)
		// NOTE: This may not be properly implemented
		//////////////////////////////////////////////////////////////////////
		case 28: //G28 Home all Axis one at a time
			saved_feedrate = feedrate;
			saved_feedmultiply = feedmultiply;
			feedmultiply = 100;
			previous_millis_cmd = millis();

			enable_endstops(true);

			for(int8_t i=0; i < NUM_AXIS; i++)
			{
				destination[i] = current_position[i];
			}
			feedrate = 0.0;

			home_all_axis = !((code_seen(axis_codes[0])) || (code_seen(axis_codes[1])) || (code_seen(axis_codes[2])));

#if Z_HOME_DIR > 0                      // If homing away from BED do Z first
			if((home_all_axis) || (code_seen(axis_codes[Z_AXIS])))
			{
				HOMEAXIS(Z);
			}
#endif

#ifdef QUICK_HOME
			if((home_all_axis) || (code_seen(axis_codes[X_AXIS]) && code_seen(axis_codes[Y_AXIS])))              //first diagonal move
			{
				current_position[X_AXIS] = 0;
				current_position[Y_AXIS] = 0;

				int x_axis_home_dir = home_dir(X_AXIS);

				plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
				destination[X_AXIS] = 1.5 * max_length(X_AXIS) * x_axis_home_dir;
				destination[Y_AXIS] = 1.5 * max_length(Y_AXIS) * home_dir(Y_AXIS);
				feedrate = homing_feedrate[X_AXIS];
				if(homing_feedrate[Y_AXIS]<feedrate)
				{ feedrate =homing_feedrate[Y_AXIS]; }
				plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);
				st_synchronize();

				axis_is_at_home(X_AXIS);
				axis_is_at_home(Y_AXIS);
				plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
				destination[X_AXIS] = current_position[X_AXIS];
				destination[Y_AXIS] = current_position[Y_AXIS];
				plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);
				feedrate = 0.0;
				st_synchronize();
				endstops_hit_on_purpose();

				current_position[X_AXIS] = destination[X_AXIS];
				current_position[Y_AXIS] = destination[Y_AXIS];
				current_position[Z_AXIS] = destination[Z_AXIS];
			}
#endif

			if((home_all_axis) || (code_seen(axis_codes[X_AXIS])))
			{
				HOMEAXIS(X);
			}

			if((home_all_axis) || (code_seen(axis_codes[Y_AXIS])))
			{
				HOMEAXIS(Y);
			}

#if Z_HOME_DIR < 0                      // If homing towards BED do Z last
			if((home_all_axis) || (code_seen(axis_codes[Z_AXIS])))
			{
				HOMEAXIS(Z);
			}
#endif

			if(code_seen(axis_codes[X_AXIS]))
			{
				if(code_value_long() != 0)
				{
					current_position[X_AXIS]=code_value()+add_homeing[0];
				}
			}

			if(code_seen(axis_codes[Y_AXIS]))
			{
				if(code_value_long() != 0)
				{
					current_position[Y_AXIS]=code_value()+add_homeing[1];
				}
			}

			if(code_seen(axis_codes[Z_AXIS]))
			{
				if(code_value_long() != 0)
				{
					current_position[Z_AXIS]=code_value()+add_homeing[2];
				}
			}
			plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);

#ifdef ENDSTOPS_ONLY_FOR_HOMING
			enable_endstops(false);
#endif

			feedrate = saved_feedrate;
			feedmultiply = saved_feedmultiply;
			previous_millis_cmd = millis();
			endstops_hit_on_purpose();
			break;
		case 90: // G90
			relative_mode = false;
			break;
		case 91: // G91
			relative_mode = true;
			break;
		case 92: // G92
			st_synchronize();
			for(int8_t i=0; i < NUM_AXIS; i++)
			{
				if(code_seen(axis_codes[i]))
				{
					current_position[i] = code_value()+add_homeing[i];
					plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
				}
			}
			break;
		}
	}

	else if(code_seen('M'))
	{
		switch((int) code_value())
		{
#ifdef ULTIPANEL
		case 0: // M0 - Unconditional stop - Wait for user button press on LCD
		case 1: // M1 - Conditional stop - Wait for user button press on LCD
			{
				LCD_MESSAGEPGM(MSG_USERWAIT);
				codenum = 0;
				if(code_seen('P')) { codenum = code_value(); }       // milliseconds to wait
				if(code_seen('S')) { codenum = code_value() * 1000; }       // seconds to wait

				st_synchronize();
				previous_millis_cmd = millis();
				if(codenum > 0)
				{
					codenum += millis();  // keep track of when we started waiting
					while(millis()  < codenum && !lcd_clicked())
					{
						manage_inactivity();
						lcd_update();
					}
				}
				else
				{
					while(!lcd_clicked())
					{
						manage_inactivity();
						lcd_update();
					}
				}
				LCD_MESSAGEPGM(MSG_RESUMING);
			}
			break;
#endif
#ifdef LASER_FIRE_SPINDLE
		case 3:  //M3 - fire laser
			if(code_seen('S') && !IsStopped()) { laser.intensity = (float) code_value(); }
			if(code_seen('L') && !IsStopped()) { laser.duration = (unsigned long) labs(code_value()); }
			if(code_seen('P') && !IsStopped()) { laser.ppm = (float) code_value(); }
			if(code_seen('B') && !IsStopped()) { laser_set_mode((int) code_value()); }

			laser.status = LASER_ON;
			laser.fired = LASER_FIRE_SPINDLE;
//*=*=*=*=*=*
			lcd_update();

			prepare_move();
			break;
		case 5:  //M5 stop firing laser
			laser.status = LASER_OFF;
			lcd_update();
			prepare_move();
			break;
#endif // LASER_FIRE_SPINDLE
		case 17:
			LCD_MESSAGEPGM(MSG_NO_MOVE);
			enable_x();
			enable_y();
			enable_z();
			break;

#ifdef SDSUPPORT
		case 20: // M20 - list SD card
			SERIAL_PROTOCOLLNPGM(MSG_BEGIN_FILE_LIST);
			card.ls();
			SERIAL_PROTOCOLLNPGM(MSG_END_FILE_LIST);
			break;
		case 21: // M21 - init SD card

			card.initsd();

			break;
		case 22: //M22 - release SD card
			card.release();

			break;
		case 23: //M23 - Select file
			starpos = (strchr(strchr_pointer + 4,'*'));
			if(starpos!=NULL)
			{ * (starpos-1) ='\0'; }
			card.openFile(strchr_pointer + 4,true);
			break;
		case 24: //M24 - Start SD print
			card.startFileprint();
			starttime=millis();
			break;
		case 25: //M25 - Pause SD print
			card.pauseSDPrint();
			break;
		case 26: //M26 - Set SD index
			if(card.cardOK && code_seen('S'))
			{
				card.setIndex(code_value_long());
			}
			break;
		case 27: //M27 - Get SD status
			card.getStatus();
			break;
		case 28: //M28 - Start SD write
			starpos = (strchr(strchr_pointer + 4,'*'));
			if(starpos != NULL)
			{
				char* npos = strchr(cmdbuffer[bufindr], 'N');
				strchr_pointer = strchr(npos,' ') + 1;
				* (starpos-1) = '\0';
			}
			card.openFile(strchr_pointer+4,false);
			break;
		case 29: //M29 - Stop SD write
			//processed in write to file routine above
			//card,saving = false;
			break;
		case 30: //M30 <filename> Delete File
			if(card.cardOK)
			{
				card.closefile();
				starpos = (strchr(strchr_pointer + 4,'*'));
				if(starpos != NULL)
				{
					char* npos = strchr(cmdbuffer[bufindr], 'N');
					strchr_pointer = strchr(npos,' ') + 1;
					* (starpos-1) = '\0';
				}
				card.removeFile(strchr_pointer + 4);
			}
			break;
		case 32: //M32 - Select file and start SD print
			if(card.sdprinting)
			{
				st_synchronize();
				card.closefile();
				card.sdprinting = false;
			}
			starpos = (strchr(strchr_pointer + 4,'*'));
			if(starpos!=NULL)
			{ * (starpos-1) ='\0'; }
			card.openFile(strchr_pointer + 4,true);
			card.startFileprint();
			starttime=millis();
			break;
		case 928: //M928 - Start SD write
			starpos = (strchr(strchr_pointer + 5,'*'));
			if(starpos != NULL)
			{
				char* npos = strchr(cmdbuffer[bufindr], 'N');
				strchr_pointer = strchr(npos,' ') + 1;
				* (starpos-1) = '\0';
			}
			card.openLogFile(strchr_pointer+5);
			break;

#endif //SDSUPPORT

		case 31: //M31 take time since the start of the SD print or an M109 command
			{
				stoptime=millis();
				char time[30];
				unsigned long t= (stoptime-starttime) /1000;
				int sec,min;
				min=t/60;
				sec=t%60;
				sprintf_P(time, PSTR("%i min, %i sec"), min, sec);
				SERIAL_ECHO_START;
				SERIAL_ECHOLN(time);
				lcd_setstatus(time);
			}
			break;
		case 42: //M42 -Change pin status via gcode
			if(code_seen('S'))
			{
				int pin_status = code_value();
				int pin_number = LED_PIN;
				if(code_seen('P') && pin_status >= 0 && pin_status <= 255)
				{ pin_number = code_value(); }
				for(int8_t i = 0; i < (int8_t) sizeof(sensitive_pins); i++)
				{
					if(sensitive_pins[i] == pin_number)
					{
						pin_number = -1;
						break;
					}
				}
#if defined(FAN_PIN) && FAN_PIN > -1
				if(pin_number == FAN_PIN)
				{ fanSpeed = pin_status; }
#endif
				if(pin_number > -1)
				{
					pinMode(pin_number, OUTPUT);
					digitalWrite(pin_number, pin_status);
					analogWrite(pin_number, pin_status);
				}
			}
			break;

#if defined(FAN_PIN) && FAN_PIN > -1
		case 106: //M106 Fan On
			if(code_seen('S'))
			{
				fanSpeed=constrain(code_value(),0,255);
			}
			else
			{
				fanSpeed=255;
			}
			break;
		case 107: //M107 Fan Off
			fanSpeed = 0;
			break;
#endif //FAN_PIN

		case 82:
			axis_relative_modes[3] = false;
			break;
		case 83:
			axis_relative_modes[3] = true;
			break;
		case 18: //compatibility
		case 84: // M84
			if(code_seen('S'))
			{
				stepper_inactive_time = code_value() * 1000;
			}
			else
			{
				bool all_axis = !((code_seen(axis_codes[X_AXIS])) || (code_seen(axis_codes[Y_AXIS])) || (code_seen(axis_codes[Z_AXIS])));
				if(all_axis)
				{
					st_synchronize();
					finishAndDisableSteppers();
				}
				else
				{
					st_synchronize();
					if(code_seen('X'))
					{
						has_axis_homed[X_AXIS] = false;
						disable_x();
					}
					if(code_seen('Y'))
					{
						has_axis_homed[Y_AXIS] = false;
						disable_y();
					}
					if(code_seen('Z'))
					{
#ifndef Z_AXIS_IS_LEADSCREW
						has_axis_homed[Z_AXIS] = false;
#endif
						disable_z();
					}
				}
			}
			break;
		case 85: // M85
			code_seen('S');
			max_inactive_time = code_value() * 1000;
			break;
		case 92: // M92
			for(int8_t i=0; i < NUM_AXIS; i++)
			{
				if(code_seen(axis_codes[i]))
				{
					if(i == 3)    // E
					{
						float value = code_value();
						if(value < 20.0)
						{
							float factor = axis_steps_per_unit[i] / value; // increase e constants if M92 E14 is given for netfab.
							max_e_jerk *= factor;
							max_feedrate[i] *= factor;
							axis_steps_per_sqr_second[i] *= factor;
						}
						axis_steps_per_unit[i] = value;
					}
					else
					{
						axis_steps_per_unit[i] = code_value();
					}
				}
			}
			break;
		case 115: // M115
			SERIAL_PROTOCOLPGM(MSG_M115_REPORT);
			break;
		case 117: // M117 display message
			starpos = (strchr(strchr_pointer + 5,'*'));
			if(starpos!=NULL)
			{ * (starpos-1) ='\0'; }
			lcd_setstatus(strchr_pointer + 5);
			break;
		case 114: // M114
			SERIAL_PROTOCOLPGM("X:");
			SERIAL_PROTOCOL(current_position[X_AXIS]);
			SERIAL_PROTOCOLPGM("Y:");
			SERIAL_PROTOCOL(current_position[Y_AXIS]);
			SERIAL_PROTOCOLPGM("Z:");
			SERIAL_PROTOCOL(current_position[Z_AXIS]);
			SERIAL_PROTOCOLPGM("E:");
			SERIAL_PROTOCOL(0.0);

			SERIAL_PROTOCOLPGM(MSG_COUNT_X);
			SERIAL_PROTOCOL(float (st_get_position(X_AXIS)) /axis_steps_per_unit[X_AXIS]);
			SERIAL_PROTOCOLPGM("Y:");
			SERIAL_PROTOCOL(float (st_get_position(Y_AXIS)) /axis_steps_per_unit[Y_AXIS]);
			SERIAL_PROTOCOLPGM("Z:");
			SERIAL_PROTOCOL(float (st_get_position(Z_AXIS)) /axis_steps_per_unit[Z_AXIS]);

			SERIAL_PROTOCOLLN("");
			break;
		case 120: // M120
			enable_endstops(false) ;
			break;
		case 121: // M121
			enable_endstops(true) ;
			break;
		case 119: // M119
			SERIAL_PROTOCOLLN(MSG_M119_REPORT);
#if defined(X_MIN_PIN) && X_MIN_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_X_MIN);
			SERIAL_PROTOCOLLN(((READ(X_MIN_PIN) ^X_MIN_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
#if defined(X_MAX_PIN) && X_MAX_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_X_MAX);
			SERIAL_PROTOCOLLN(((READ(X_MAX_PIN) ^X_MAX_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
#if defined(Y_MIN_PIN) && Y_MIN_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_Y_MIN);
			SERIAL_PROTOCOLLN(((READ(Y_MIN_PIN) ^Y_MIN_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
#if defined(Y_MAX_PIN) && Y_MAX_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_Y_MAX);
			SERIAL_PROTOCOLLN(((READ(Y_MAX_PIN) ^Y_MAX_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
#if defined(Z_MIN_PIN) && Z_MIN_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_Z_MIN);
			SERIAL_PROTOCOLLN(((READ(Z_MIN_PIN) ^Z_MIN_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
#if defined(Z_MAX_PIN) && Z_MAX_PIN > -1
			SERIAL_PROTOCOLPGM(MSG_Z_MAX);
			SERIAL_PROTOCOLLN(((READ(Z_MAX_PIN) ^Z_MAX_ENDSTOP_INVERTING) ?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
#endif
			break;
		//TODO: update for all axis, use for loop
		case 201: // M201
			for(int8_t i=0; i < NUM_AXIS; i++)
			{
				if(code_seen(axis_codes[i]))
				{
					max_acceleration_units_per_sq_second[i] = code_value();
				}
			}
			// steps per sq second need to be updated to agree with the units per sq second (as they are what is used in the planner)
			reset_acceleration_rates();
			break;
		case 203: // M203 max feedrate mm/sec
			for(int8_t i=0; i < NUM_AXIS; i++)
			{
				if(code_seen(axis_codes[i])) { max_feedrate[i] = code_value(); }
			}
			break;
		case 204: // M204 acclereration S normal moves T filmanent only moves
			{
				if(code_seen('S')) { acceleration = code_value() ; }
				if(code_seen('T')) { retract_acceleration = code_value() ; }
			}
			break;
		case 205: //M205 advanced settings:  minimum travel speed S=while printing T=travel only,  B=minimum segment time X= maximum xy jerk, Z=maximum Z jerk
			{
				if(code_seen('S')) { minimumfeedrate = code_value(); }
				if(code_seen('T')) { mintravelfeedrate = code_value(); }
				if(code_seen('B')) { minsegmenttime = code_value() ; }
				if(code_seen('X')) { max_xy_jerk = code_value() ; }
				if(code_seen('Z')) { max_z_jerk = code_value() ; }
				if(code_seen('E')) { max_e_jerk = code_value() ; }
			}
			break;
		case 206: // M206 additional homeing offset
			for (int8_t i = 0; i < NUM_AXIS; i++)
			{
				if(code_seen(axis_codes[i])) { add_homeing[i] = code_value(); }
			}
			break;
		case 220: // M220 S<factor in percent>- set speed factor override percentage
			{
				if(code_seen('S'))
				{
					feedmultiply = code_value() ;
				}
			}
			break;

		case 221: // M221 S<factor in percent>- set extrude factor override percentage
			{
				if(code_seen('S'))
				{
					extrudemultiply = code_value() ;
				}
			}
			break;

#if LARGE_FLASH == true && ( BEEPER > 0 || defined(ULTRALCD) )
		case 300: // M300
			{
				int beepS = code_seen('S') ? code_value() : 110;
				int beepP = code_seen('P') ? code_value() : 1000;
				if(beepS > 0)
				{
#if BEEPER > 0
					tone(BEEPER, beepS);
					delay(beepP);
					noTone(BEEPER);
#elif defined(ULTRALCD)
					lcd_buzz(beepS, beepP);
#endif
				}
				else
				{
					delay(beepP);
				}
			}
			break;
#endif // M300

#ifdef DOGLCD
		case 250: // M250  Set LCD contrast value: C<value> (value 0..63)
			{
				if(code_seen('C'))
				{
					lcd_setcontrast(((int) code_value()) &63);
				}
				SERIAL_PROTOCOLPGM("lcd contrast value: ");
				SERIAL_PROTOCOL(lcd_contrast);
				SERIAL_PROTOCOLLN("");
			}
			break;
#endif
		case 400: // M400 finish all moves
			{
				st_synchronize();
			}
			break;
		case 500: // M500 Store settings in EEPROM
			{
				Config_StoreSettings();
			}
			break;
		case 501: // M501 Read settings from EEPROM
			{
				Config_RetrieveSettings();
			}
			break;
		case 502: // M502 Revert to default settings
			{
				Config_ResetDefault();
			}
			break;
		case 503: // M503 print settings currently in memory
			{
				Config_PrintSettings();
			}
			break;
#ifdef ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED
		case 540:
			{
				if(code_seen('S')) { abort_on_endstop_hit = code_value() > 0; }
			}
			break;
#endif

		case 649: // M649 set laser options
			{
				if(code_seen('S') && !IsStopped())
				{
					laser.intensity = (float) code_value();
					laser.rasterlaserpower =  laser.intensity;
				}
				if(code_seen('L') && !IsStopped()) { laser.duration = (unsigned long) labs(code_value()); }
				if(code_seen('P') && !IsStopped()) { laser.ppm = (float) code_value(); }
				if(code_seen('B') && !IsStopped()) { laser_set_mode((int) code_value()); }
				if(code_seen('R') && !IsStopped()) { laser.raster_mm_per_pulse = ((float) code_value()); }
				if(code_seen('F'))
				{
					next_feedrate = code_value();
					if(next_feedrate > 0.0) { feedrate = next_feedrate; }
				}

			}
			break;

		case 907: // M907 Set digital trimpot motor current using axis codes.
			{
#if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
				for(int i=0; i<NUM_AXIS; i++) if(code_seen(axis_codes[i])) { digipot_current(i,code_value()); }
				if(code_seen('B')) { digipot_current(4,code_value()); }
				if(code_seen('S')) for(int i=0; i<=4; i++) { digipot_current(i,code_value()); }
#endif
			}
			break;
		case 908: // M908 Control digital trimpot directly.
			{
#if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
				uint8_t channel,current;
				if(code_seen('P')) { channel=code_value(); }
				if(code_seen('S')) { current=code_value(); }
				digitalPotWrite(channel, current);
#endif
			}
			break;
		case 350: // M350 Set microstepping mode. Warning: Steps per unit remains unchanged. S code sets stepping mode for all drivers.
			{
#if defined(X_MS1_PIN) && X_MS1_PIN > -1
				if(code_seen('S')) for(int i=0; i<=4; i++) { microstep_mode(i,code_value()); }
				for(int i=0; i<NUM_AXIS; i++) if(code_seen(axis_codes[i])) { microstep_mode(i, (uint8_t) code_value()); }
				if(code_seen('B')) { microstep_mode(4,code_value()); }
				microstep_readings();
#endif
			}
			break;
		case 351: // M351 Toggle MS1 MS2 pins directly, S# determines MS1 or MS2, X# sets the pin high/low.
			{
#if defined(X_MS1_PIN) && X_MS1_PIN > -1
				if(code_seen('S')) switch((int) code_value())
					{
					case 1:
						for(int i=0; i<NUM_AXIS; i++) if(code_seen(axis_codes[i])) { microstep_ms(i,code_value(),-1); }
						if(code_seen('B')) { microstep_ms(4,code_value(),-1); }
						break;
					case 2:
						for(int i=0; i<NUM_AXIS; i++) if(code_seen(axis_codes[i])) { microstep_ms(i,-1,code_value()); }
						if(code_seen('B')) { microstep_ms(4,-1,code_value()); }
						break;
					}
				microstep_readings();
#endif
			}
			break;
		case 999: // M999: Restart after being stopped
			Stopped = false;
			lcd_reset_alert_level();
			gcode_LastN = Stopped_gcode_LastN;
			FlushSerialRequestResend();
			break;
		}
	}
	else
	{
		SERIAL_ECHO_START;
		SERIAL_ECHOPGM(MSG_UNKNOWN_COMMAND);
		SERIAL_ECHO(cmdbuffer[bufindr]);
		SERIAL_ECHOLNPGM("\"");
	}

	ClearToSend();
}

void FlushSerialRequestResend()
{
	//char cmdbuffer[bufindr][100]="Resend:";
	MYSERIAL.flush();
	SERIAL_PROTOCOLPGM(MSG_RESEND);
	SERIAL_PROTOCOLLN(gcode_LastN + 1);
	ClearToSend();
}

void ClearToSend()
{
	previous_millis_cmd = millis();
#ifdef SDSUPPORT
	if(fromsd[bufindr])
	{ return; }
#endif //SDSUPPORT
	SERIAL_PROTOCOLLNPGM(MSG_OK);
}

void get_coordinates()
{
	bool seen[4]= {false,false,false,false};
	for(int8_t i=0; i < NUM_AXIS; i++)
	{
		if(code_seen(axis_codes[i]))
		{
			destination[i] = (float) code_value() + (axis_relative_modes[i] || relative_mode) *current_position[i];
			seen[i]=true;
		}
		else //Are these else lines really needed?
		{
			destination[i] = current_position[i]; 
		}
	}

	if(code_seen('F'))
	{
		next_feedrate = code_value();
		if(next_feedrate > 0.0) { feedrate = next_feedrate; }
	}
}

void get_arc_coordinates()
{
#ifdef SF_ARC_FIX
	bool relative_mode_backup = relative_mode;
	relative_mode = true;
#endif
	get_coordinates();
#ifdef SF_ARC_FIX
	relative_mode=relative_mode_backup;
#endif

	if(code_seen('I'))
	{
		offset[0] = code_value();
	}
	else
	{
		offset[0] = 0.0;
	}
	if(code_seen('J'))
	{
		offset[1] = code_value();
	}
	else
	{
		offset[1] = 0.0;
	}
}

void clamp_to_software_endstops(float target[3])
{
	if(min_software_endstops)
	{
		if(target[X_AXIS] < min_pos[X_AXIS]) { target[X_AXIS] = min_pos[X_AXIS]; }
		if(target[Y_AXIS] < min_pos[Y_AXIS]) { target[Y_AXIS] = min_pos[Y_AXIS]; }
		if(target[Z_AXIS] < min_pos[Z_AXIS]) { target[Z_AXIS] = min_pos[Z_AXIS]; }
	}

	if(max_software_endstops)
	{
		if(target[X_AXIS] > max_pos[X_AXIS]) { target[X_AXIS] = max_pos[X_AXIS]; }
		if(target[Y_AXIS] > max_pos[Y_AXIS]) { target[Y_AXIS] = max_pos[Y_AXIS]; }
		if(target[Z_AXIS] > max_pos[Z_AXIS]) { target[Z_AXIS] = max_pos[Z_AXIS]; }
	}
}

void prepare_move()
{
	clamp_to_software_endstops(destination);

	previous_millis_cmd = millis();

	// Do not use feedmultiply for Z only moves
	if((current_position[X_AXIS] == destination [X_AXIS]) && (current_position[Y_AXIS] == destination [Y_AXIS]))
	{
		plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate/60);
	}
	else
	{
		plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], feedrate*feedmultiply/60/100.0);
	}

	for(int8_t i=0; i < NUM_AXIS; i++)
	{
		current_position[i] = destination[i];
	}
}

void prepare_arc_move(char isclockwise)
{
	float r = hypot(offset[X_AXIS], offset[Y_AXIS]);    // Compute arc radius for mc_arc

	// Trace the arc
	mc_arc(current_position, destination, offset, X_AXIS, Y_AXIS, Z_AXIS, feedrate*feedmultiply/60/100.0, r, isclockwise);

	// As far as the parser is concerned, the position is now == target. In reality the
	// motion control system might still be processing the action and the real tool position
	// in any intermediate location.
	for(int8_t i=0; i < NUM_AXIS; i++)
	{
		current_position[i] = destination[i];
	}
	previous_millis_cmd = millis();
}

#if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1

#if defined(FAN_PIN)
	#if CONTROLLERFAN_PIN == FAN_PIN
		#error "You cannot set CONTROLLERFAN_PIN equal to FAN_PIN"
	#endif
#endif

unsigned long lastMotor = 0; //Save the time for when a motor was turned on last
unsigned long lastMotorCheck = 0;

void controllerFan()
{
	if((millis() - lastMotorCheck) >= 2500)      //Not a time critical function, so we only check every 2500ms
	{
		lastMotorCheck = millis();

		if(!READ(X_ENABLE_PIN) || !READ(Y_ENABLE_PIN) || !READ(Z_ENABLE_PIN))     //If any of the drivers are enabled...
		{
			lastMotor = millis(); //... set time to NOW so the fan will turn on
		}

		if((millis() - lastMotor) >= (CONTROLLERFAN_SECS*1000UL) || lastMotor == 0)        //If the last time any driver was enabled, is longer since than CONTROLLERSEC...
		{
			digitalWrite(CONTROLLERFAN_PIN, 0);
			analogWrite(CONTROLLERFAN_PIN, 0);
		}
		else
		{
			// allows digital or PWM fan output to be used (see M42 handling)
			digitalWrite(CONTROLLERFAN_PIN, CONTROLLERFAN_SPEED);
			analogWrite(CONTROLLERFAN_PIN, CONTROLLERFAN_SPEED);
		}
	}
}
#endif

void manage_inactivity()
{
	if((millis() - previous_millis_cmd) >  max_inactive_time)
		if(max_inactive_time)
		{ kill(); }
	if(stepper_inactive_time)
	{
		if((millis() - previous_millis_cmd) >  stepper_inactive_time)
		{
			if(blocks_queued() == false)
			{
				disable_x();
				disable_y();
				disable_z();
#ifndef Z_AXIS_IS_LEADSCREW
				has_axis_homed[Z_AXIS] = false;
#endif
				has_axis_homed[X_AXIS] = false;
				has_axis_homed[Y_AXIS] = false;

				if(laser.time / 60000 > 0)
				{
					laser.lifetime += laser.time / 60000; // convert to minutes
					laser.time = 0;
					Config_StoreSettings();
				}
				laser_init();

#ifdef LASER_PERIPHERALS
				laser_peripherals_off();
#endif
			}
		}
	}
#if defined(KILL_PIN) && KILL_PIN > -1
	if(0 == READ(KILL_PIN))
	{ kill(); }
#endif
#if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1
	controllerFan(); //Check if fan should be turned on to cool stepper drivers down
#endif

	check_axes_activity();
}

void kill()
{
	cli(); // Stop interrupts

	disable_x();
	disable_y();
	disable_z();

	laser_init();

#ifdef LASER_PERIPHERALS
	laser_peripherals_off();
#endif // LASER_PERIPHERALS

	SERIAL_ERROR_START;
	SERIAL_ERRORLNPGM(MSG_ERR_KILLED);
	LCD_ALERTMESSAGEPGM(MSG_KILLED);
	suicide();
	while(1) { /* Intentionally left empty */ }    // Wait for reset
}

void Stop()
{
#if defined( LASER_DIAGNOSTICS )
	SERIAL_ECHOLN("Laser set to off, stop() called");
#endif
	laser_extinguish();

#ifdef LASER_PERIPHERALS
	laser_peripherals_off();
#endif
	if(Stopped == false)
	{
		Stopped = true;
		Stopped_gcode_LastN = gcode_LastN; // Save last g_code for restart
		SERIAL_ERROR_START;
		SERIAL_ERRORLNPGM(MSG_ERR_STOPPED);
		LCD_MESSAGEPGM(MSG_STOPPED);
	}
}

bool IsStopped() { return Stopped; };

