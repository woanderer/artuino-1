// VL53L0X (distance sensor) BEGIN
#include "Adafruit_VL53L0X.h"
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
// VL53L0X (distance sensor) END     
   
// User defined function that returns sum of 
// arr[] using accumulate() library function. 

// MPU6050 (gyroscope/accelerometer) BEGIN
// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"

#include "MPU6050_6Axis_MotionApps20.h"
//#include "MPU6050.h" // not necessary if using MotionApps include file

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 mpu;
//MPU6050 mpu(0x69); // <-- use for AD0 high

/* =========================================================================
   NOTE: In addition to connection 3.3v, GND, SDA, and SCL, this sketch
   depends on the MPU-6050's INT pin being connected to the Arduino's
   external interrupt #0 pin. On the Arduino Uno and Mega 2560, this is
   digital I/O pin 2.
 * ========================================================================= */

/* =========================================================================
   NOTE: Arduino v1.0.1 with the Leonardo board generates a compile error
   when using Serial.write(buf, len). The Teapot output uses this method.
   The solution requires a modification to the Arduino USBAPI.h file, which
   is fortunately simple, but annoying. This will be fixed in the next IDE
   release. For more info, see these links:

   http://arduino.cc/forum/index.php/topic,109987.0.html
   http://code.google.com/p/arduino/issues/detail?id=958
 * ========================================================================= */


const int n_calibration_readings = 100;
int calibration_count = 0;

#define LED_PIN 13 // (Arduino is 13, Teensy is 11, Teensy++ is 6)
bool blinkState = false;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

int acc_x_arr[n_calibration_readings];
int acc_y_arr[n_calibration_readings];
int acc_z_arr[n_calibration_readings];
float yaw_arr[n_calibration_readings];
float pitch_arr[n_calibration_readings];
float roll_arr[n_calibration_readings];

float offset_yaw; 
float offset_pitch; 
float offset_roll; 
int offset_acc_x;
int offset_acc_y;
int offset_acc_z;


// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };



// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}
// MPU6050 (gyroscope/accelerometer) END

float float_avg(const float v[], int n )
{
    float sum = 0.0f;

    for ( int i = 0; i < n; i++ )
    {
        sum += v[i]; //sum all the numbers in the vector v
    }

    return sum / n;
}

float int_avg(const int v[], int n )
{
    float sum = 0.0f;

    for ( int i = 0; i < n; i++ )
    {
        sum += v[i]; //sum all the numbers in the vector v
    }

    return sum / n;
}

void setup() {
    Serial.begin(115200);

    // wait until serial port opens for native USB devices
    while (!Serial); // wait for Leonardo enumeration, others continue immediately

    
    Serial.println("Adafruit VL53L0X test");
    if (!lox.begin()) {
        Serial.println(F("Failed to boot VL53L0X"));
        while(1);
    }
    // power 
    // Serial.println(F("VL53L0X API Simple Ranging example\n\n")); 

    // MPU6050 BEGIN

    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)

    // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3v or Ardunio
    // Pro Mini running at 3.3v, cannot handle this baud rate reliably due to
    // the baud timing being too misaligned with processor ticks. You must use
    // 38400 or slower in these cases, or use some kind of external separate
    // crystal solution for the UART timer.

    // initialize device
    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // wait for ready
    Serial.println(F("\nSend any character to begin DMP programming and demo: "));
    while (Serial.available() && Serial.read()); // empty buffer
    while (!Serial.available());                 // wait for data
    while (Serial.available() && Serial.read()); // empty buffer again

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
        attachInterrupt(0, dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    // configure LED for output
    pinMode(LED_PIN, OUTPUT);
}


void loop() {
    int delay_val = 2;
    int state = 0; //uncalibrated
    
    // if programming failed, don't try to do anything
    if (!dmpReady) return;

    // wait for MPU interrupt or extra packet(s) available
    while (!mpuInterrupt && fifoCount < packetSize) {
        // other program behavior stuff here
        // .
        // .
        // .
        // if you are really paranoid you can frequently test in between other
        // stuff to see if mpuInterrupt is true, and if so, "break;" from the
        // while() loop to immediately process the MPU data
        // .
        // .
        // .
    }
    VL53L0X_RangingMeasurementData_t measure;
    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();

    // get current FIFO count
    fifoCount = mpu.getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));

    // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);
        
        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;

        // display initial world-frame acceleration, adjusted to remove gravity
        // and rotated based on known orientation from quaternion
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
        lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

        // blink LED to indicate activity
        blinkState = !blinkState;
        digitalWrite(LED_PIN, blinkState);

        int distance = measure.RangeMilliMeter;
        float yaw = ypr[0] * 180/M_PI;
        float pitch = ypr[1] * 180/M_PI;
        float roll = ypr[2] * 180/M_PI;
        int acc_x = aaWorld.x;
        int acc_y = aaWorld.y;
        int acc_z = aaWorld.z;

        if (state == 0) { //calibration
            Serial.print(distance); 
            Serial.print("\t");
            Serial.print(yaw); 
            Serial.print("\t");
            Serial.print(pitch); 
            Serial.print("\t");
            Serial.print(roll); 
            Serial.print("\t");
            Serial.print(acc_x); 
            Serial.print("\t");
            Serial.print(acc_y); 
            Serial.print("\t");
            Serial.println(acc_z);
            yaw_arr[calibration_count] = yaw;
            pitch_arr[calibration_count] = pitch;
            roll_arr[calibration_count] = roll;
            acc_x_arr[calibration_count] = acc_x;
            acc_y_arr[calibration_count] = acc_y;
            acc_z_arr[calibration_count] = acc_z;
            calibration_count++;
            if (calibration_count == n_calibration_readings) {
                offset_yaw = float_avg(yaw_arr, calibration_count); 
                offset_pitch = float_avg(pitch_arr, calibration_count); 
                offset_roll = float_avg(roll_arr, calibration_count); 
                offset_acc_x = int_avg(acc_x_arr, calibration_count);
                offset_acc_y = int_avg(acc_y_arr, calibration_count);
                offset_acc_z = int_avg(acc_z_arr, calibration_count);

                Serial.println("CALIBRATION COMPLETE");
                Serial.println("OFFSET VALUES:");
                Serial.println("y\tp\tr\ta_x\ta_y\ta_z");

                Serial.print(offset_yaw); 
                Serial.print("\t");
                Serial.print(offset_pitch); 
                Serial.print("\t");
                Serial.println(offset_roll);
                Serial.print("\t");
                Serial.print(offset_acc_x); 
                Serial.print("\t");
                Serial.print(offset_acc_y); 
                Serial.print("\t");
                Serial.print(offset_acc_z); 

                // mpu.setXAccelOffset(offset_acc_x);
                // mpu.setYAccelOffset(offset_acc_y);
                // mpu.setZAccelOffset(offset_acc_z);
                // mpu.setXGyroOffset(offset_yaw);
                // mpu.setYGyroOffset(offset_pitch);
                // mpu.setZGyroOffset(offset_roll);
                state++;
            }   
        } else if (state == 1) {
            delay_val = 100;
            state++;
            Serial.println("\n\nCALIBRATED READINGS BEGIN:\n");
            Serial.println("d\ty\tp\tr\ta_x\ta_y\ta_z");
        } else if (state == 2) {
            yaw = yaw - offset_yaw;
            pitch = pitch - offset_pitch;
            roll = roll - offset_roll;
            acc_x = acc_x - offset_acc_x;
            acc_y = acc_y - offset_acc_y;
            acc_z = acc_z - offset_acc_z;

            Serial.print(distance); 
            Serial.print("\t");
            Serial.print(yaw); 
            Serial.print("\t");
            Serial.print(pitch); 
            Serial.print("\t");
            Serial.print(roll); 
            Serial.print("\t");
            Serial.print(acc_x); 
            Serial.print("\t");
            Serial.print(acc_y); 
            Serial.print("\t");
            Serial.println(acc_z);
        }

    } 


  delay(delay_val);
}