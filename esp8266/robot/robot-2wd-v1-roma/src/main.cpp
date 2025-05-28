#include <Arduino.h>
#include <Servo.h>
#include <Ultrasonic.h>

#define motorAspeed 	D1
#define motorAdir   	D3
#define motorBspeed 	D2
#define motorBdir   	D4

#define SERVO_PIN    	D5

#define TRIGGER_PIN 	D6
#define ECHO_PIN    	D7

#define FULL_SPEED  	1023
#define DIRECTIONS  	7
#define ANGLE       	180 / (DIRECTIONS-1)  
#define SERVO_DELAY  	ANGLE
#define TURN_DELAY  	200
#define FWD_RUN_DELAY   50
#define TARGET_RANGE 	30.0

#define OPMODE_AVOID    true
#define NDEBUG    		true


volatile unsigned int 	speed = FULL_SPEED;
volatile unsigned int 	lookDir = 1;
volatile unsigned int 	dir = 0;
const unsigned int 		dir_fwd = DIRECTIONS / 2 ;
const unsigned int 		dir_left = DIRECTIONS -1  ;
const unsigned int 		dir_right = 0  ;

volatile float 			dist[DIRECTIONS] = {};

Servo servo;
Ultrasonic sonar(TRIGGER_PIN, ECHO_PIN);

// останов
void stop(void) {
#ifdef DEBUG
  Serial.println("Stop");
#endif
  digitalWrite(motorAspeed, LOW );
  digitalWrite(motorBspeed, LOW );
}

// вперед
void moveForward(void) {
#ifdef DEBUG
   Serial.println("FF");
#endif
     analogWrite(motorAspeed, speed);
     analogWrite(motorBspeed, speed);
     digitalWrite(motorAdir, HIGH);
     digitalWrite(motorBdir, HIGH);
}

// назад
void moveBackward(void) {
#ifdef DEBUG
   Serial.println("RW");
#endif
  analogWrite(motorAspeed, speed);
  analogWrite(motorBspeed, speed);
  digitalWrite(motorAdir, LOW);
  digitalWrite(motorBdir, LOW);
}
// влево
void turnLeft(void) {
#ifdef DEBUG
   Serial.println("TL");
#endif
  analogWrite(motorAspeed, speed);
  analogWrite(motorBspeed, speed);
  digitalWrite(motorAdir, HIGH);
  digitalWrite(motorBdir, LOW);

}
// вправо
void turnRight(void) {
#ifdef DEBUG
   Serial.println("TR");
#endif
  analogWrite(motorAspeed, speed);
  analogWrite(motorBspeed, speed);
  digitalWrite(motorAdir, LOW);
  digitalWrite(motorBdir, HIGH);
}

// sense distance
float getDistance() {
  long microsec = sonar.timing();
  float cmMsec = sonar.convert(microsec, Ultrasonic::CM);
  return cmMsec;
}

// setup MCU
void setup() {
	 Serial.begin(9600);
	 
     pinMode(motorAspeed, OUTPUT);
     digitalWrite(motorAspeed, LOW );
     pinMode(motorAdir, OUTPUT);
     digitalWrite(motorAdir, LOW );
     pinMode(motorBspeed, OUTPUT);
     digitalWrite(motorBspeed, LOW );
     pinMode(motorBdir, OUTPUT);
     digitalWrite(motorBdir, LOW );

     servo.attach(SERVO_PIN);
     servo.write(0);
     Serial.print("Sleep 10s");
     delay(10000);
     Serial.println("Ready");
 }


// look left / right
void look(void) {
	lookDir = 1 - lookDir;
	
	for (unsigned int i=dir_right ;i < dir_left+1; i++) {
		if (lookDir == 0) {
			dir = i;
		} else {
			dir = dir_left-i;
		}
		servo.write(dir * ANGLE );
		delay( SERVO_DELAY );
		dist[dir] = getDistance();
	}
#ifdef DEBUG
	Serial.print("D[] ");
	for (int i=0;i < DIRECTIONS; i++) {
		Serial.print(" ");
		Serial.print(dist[i]);
	}
	Serial.println();
#endif
}

// find closest target
unsigned int findTarget() {
	unsigned int target = dir_right;
	
	for (unsigned int i=dir_right;i < dir_left+1; i++) {
		if (dist[i] < dist[target]) {
			target = i;
		} 
	}
#ifdef DEBUG
	Serial.print("Target: ");
	Serial.print(target);	
	Serial.print(" at ");
	Serial.print(dist[target]);
	Serial.println("cm");	
#endif
	return target;
}


//main loop
void loop() {
//	stop();
	look();
	unsigned int target = findTarget();

#ifdef OPMODE_AVOID
	// 1 avoid obstacles

	if (dist[target] < TARGET_RANGE ) { // target is close to robot
		#ifdef DEBUG
			Serial.print("Target is close: ");
			Serial.println(dist[target]);
			Serial.print("Target: ");
			Serial.print(target);	
			Serial.print(" dir F: ");
			Serial.println(dir_fwd);
			Serial.print(" dir L: ");
			Serial.println(dir_left);
			Serial.print(" dir R: ");
			Serial.println(dir_right);
			
		#endif
		if ( target == dir_fwd) {
			moveBackward();
			delay(100);
		} else {
		// target angle ranges 
		if (target < dir_fwd ) {
				// target is on a right
				// turn left
				turnLeft();
				delay(TURN_DELAY+100*(target));
				//stop();
		} else {
			    // target is on a left
				// turn right
				turnRight();
				delay(TURN_DELAY+100*(2*dir_fwd-target) );
				//stop();
		}
		}
	} // check close targets

#endif // avoid mode

// 2 follow target
#ifdef OPMODE_FOLLOW

#endif	// follow mode

	// can move fwd
	moveForward();		
	delay(FWD_RUN_DELAY);

}
