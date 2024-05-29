//Import libraries and defining some variables for the program
#include "setup.h"
#if EXAMPLE == 3
#include "Arduino.h"
#include "PWMServo.h"
#include "CameraOV7670.h"
#include "avr/io.h"
#include "avr/interrupt.h"
#define UART_MODE 2


/*
This code deals with image proceessing using UART and pixel formatting
It is created for the OV7670 Camera Module with the Arduino UNO
*/


//This Part of code is the configuration process
/*
Constants:

VERSION: This constant defines the system version (0x10 in this case).
COMMAND_*: These constants represent commands used for UART communication:
COMMAND_NEW_FRAME: Used by the commandStartNewFrame function to signal a new image frame.
COMMAND_DEBUG_DATA: Used by the commandDebugPrint function to send debug messages.
UART_PIXEL_FORMAT_RGB565: Specifies the RGB565 pixel format for UART transmission (5 bits red, 6 bits green, 5 bits blue).
H_BYTE_* and L_BYTE_*: Constants related to pixel byte parity checking:
H_BYTE_PARITY_CHECK: Bit mask to check for an odd number of bits in the high byte (red and green components).
H_BYTE_PARITY_INVERT: Inverts the parity check result for the high byte.
L_BYTE_PARITY_CHECK: Similar to H_BYTE for even parity in the low byte (green and blue).
L_BYTE_PARITY_INVERT: Inverts the parity check result for the low byte.
L_BYTE_PREVENT_ZERO: Ensures the low byte value is always above zero (avoiding issues with zero as an end-of-line marker) by setting the least significant bit.
COLOR_*: Hexadecimal values representing colors:
COLOR_GREEN: Green color value.
COLOR_RED: Red color value.

Function Prototypes:
processRgbFrameBuffered and processRgbFrameDirect: Function prototypes (declarations without implementation) for processing RGB frames. The actual implementation is likely defined elsewhere.
ProcessFrameData: This is a function pointer type that can point to either processRgbFrameBuffered or processRgbFrameDirect.
commandStartNewFrame(uint8_t pixelFormat): Prototype for sending a "new frame" command with the specified pixel format over UART.
commandDebugPrint(const String debugText): Prototype for sending a debug message over UART.
sendNextCommandByte(uint8_t checksum, uint8_t commandByte): Prototype for sending a command byte and updating the checksum for error detection.
sendBlankFrame(uint16_t color): Prototype for sending a blank frame filled with a specified color value.

Configuration (Conditional Compilation):
#if UART_MODE==1 and #endif: These define two conditional compilation blocks based on the value of UART_MODE.
Each block sets various configuration options like:
Image resolution (lineLength and lineCount).
Baud rate (baud).
Function pointer (processFrameData) to choose the frame processing function (likely processRgbFrameBuffered in this case).
Line buffer size (lineBufferLength).
Buffering flag (isSendWhileBuffering).
Pixel format (uartPixelFormat).
Camera initialization with resolution and pixel format settings.

Global Variables:
lineBuffer: Array of bytes to store the captured image data (each pixel requires two bytes).
lineBufferSendByte: Pointer to the current byte being processed in the line buffer.
isLineBufferSendHighByte: Flag indicating if the current byte is the high byte of a pixel.
isLineBufferByteFormatted: Flag indicating if the current byte has been formatted for UART transmission.
frameCounter: Counter for the number of processed frames.
processedByteCountDuringCameraRead: Tracks the number of bytes processed during camera data reading.

Inline Function Prototypes:
These functions are likely defined elsewhere with the inline keyword suggesting the compiler to inline them for efficiency. They handle low-level tasks related to:
Processing the next pixel byte in the buffer.
Attempting to send the next pixel byte over UART.
Formatting the next pixel byte for UART transmission.
Formatting individual high and low bytes of a pixel.
Waiting for the previous UART byte transmission to complete.
Checking if the UART is ready for another byte transmission.
*/

const uint8_t VERSION = 0x10; //This constant is for defining version of the system
const uint8_t COMMAND_NEW_FRAME = 0x01 | VERSION; // This constant is used in the commandStartNewFrame function
const uint8_t COMMAND_DEBUG_DATA = 0x03 | VERSION; // This constant is used in the commandDebugPrint function
const uint16_t UART_PIXEL_FORMAT_RGB565 = 0x01; // This constant specify the RGB565 format (5 = red, 6 = green, 5 = blue) 

// Pixel byte parity check:
// Pixel Byte H: odd number of bits under H_BYTE_PARITY_CHECK and H_BYTE_PARITY_INVERT
// Pixel Byte L: even number of bits under L_BYTE_PARITY_CHECK and L_BYTE_PARITY_INVERT

//H:RRRRRGGG
const uint8_t H_BYTE_PARITY_CHECK =  0b00100000; // Byte mask to check for odd number in bits in the high byte (for red and green components)
const uint8_t H_BYTE_PARITY_INVERT = 0b00001000; // inverts the parity check results for high byte
//L:GGGBBBBB
const uint8_t L_BYTE_PARITY_CHECK =  0b00001000; // Byte mask to check for even number in bits in the low byte (for green and blue components0
const uint8_t L_BYTE_PARITY_INVERT = 0b00100000; // inverts the parity check result for low byte

// Since the parity for L byte can reach zero, it must be ensured that it cannot reach zero
// Increasing the lowest bit of blue color is fine for that.
const uint8_t L_BYTE_PREVENT_ZERO  = 0b00000001; // This constant ensures that the total byte value is above zero by increaseing the lowest bit of blue color
const uint16_t COLOR_GREEN = 0x07E0; // Hexadecimal for green
const uint16_t COLOR_RED = 0xF800; // Hexadecimat for red

//Calls the function for initzialization
void processRgbFrameBuffered();
void processRgbFrameDirect();
typedef void (*ProcessFrameData)(void) ;

#if UART_MODE==1 // Serial and Camera Configuration #1
const uint16_t lineLength = 320; //Resolution 320 pixels
const uint16_t lineCount = 240; // Resolution 240 Pixels
// Both combined into 320 x 240 pixels
const uint32_t baud  = 500000; // Baud rate is set to 500000/5kbps
const ProcessFrameData processFrameData = processRgbFrameBuffered; //function pointer to process frame in RGB format
const uint16_t lineBufferLength = lineLength * 2; // total length of line buffer
const bool isSendWhileBuffering = true; // Buffering flag
const uint8_t uartPixelFormat = UART_PIXEL_FORMAT_RGB565; // Pixel format fort UART Communication
CameraOV7670 camera(CameraOV7670::RESOLUTION_QVGA_320x240, CameraOV7670::PIXEL_RGB565, 32); // Instance of CameraOV7670 with resolution and pixel format settings
#endif

#if UART_MODE==2 // Serial and Camera Configuration #2
const uint16_t lineLength = 320;
const uint16_t lineCount = 240;
const uint32_t baud  = 1000000;
const ProcessFrameData processFrameData = processRgbFrameBuffered;
const uint16_t lineBufferLength = lineLength * 2;
const bool isSendWhileBuffering = true;
const uint8_t uartPixelFormat = UART_PIXEL_FORMAT_RGB565;
CameraOV7670 camera(CameraOV7670::RESOLUTION_QVGA_320x240, CameraOV7670::PIXEL_RGB565, 16);
#endif

uint8_t lineBuffer [lineBufferLength]; // Array of bytes in which each pixel requires two bytes per pixel
uint8_t * lineBufferSendByte; // Pointer to the current byte 
bool isLineBufferSendHighByte; // Bool flag to indicate if the current byte being sent is high byte
bool isLineBufferByteFormatted; // bool flage to indicate if the current byte being sent is low byte
uint16_t frameCounter = 0; // Counter for tracking the numbers of frame being created
uint16_t processedByteCountDuringCameraRead = 0; // tracks the number of bytes processed during camera read

//Calls the function for initzialization
void commandStartNewFrame(uint8_t pixelFormat); 
void commandDebugPrint(const String debugText);
uint8_t sendNextCommandByte(uint8_t checksum, uint8_t commandByte);
void sendBlankFrame(uint16_t color);

/*
The inline functions are initialized because they have a specific purpose in low-level programming
The inline keyword suggests to the compiler that the function should be expanded (inlined) at the call site rather than being invoked as a separate function call.
When a function is marked as inline, the compiler replaces the function call with the actual function body during compilation.

__attribute__((always_inline)):
This attribute is specific to GCC (GNU Compiler Collection) and informs the compiler to always inline the function, regardless of its size or other considerations.
It overrides the compiler’s usual inlining decisions.
*/
inline void processNextRgbPixelByteInBuffer() __attribute__((always_inline));
inline void tryToSendNextRgbPixelByteInBuffer() __attribute__((always_inline));
inline void formatNextRgbPixelByteInBuffer() __attribute__((always_inline));
inline uint8_t formatRgbPixelByteH(uint8_t byte) __attribute__((always_inline));
inline uint8_t formatRgbPixelByteL(uint8_t byte) __attribute__((always_inline));
inline void waitForPreviousUartByteToBeSent() __attribute__((always_inline));
inline bool isUartReady() __attribute__((always_inline));


// This part of code is the setup process
/*
Setup Process:

Servo Initialization:

PWMServo myServo;: This line creates an instance of the PWMServo class to control a servo motor.
Servo Positions:
int servoPositions[] = {10, 35, 60, 85, 110, 135, 160, 175};: This array defines the sequence of positions (in degrees) that the servo will move through.

Current Servo Position:
int currentPositionIndex = 0;: This variable keeps track of the current position index within the servoPositions array.

Timer Interrupt Service Routine (ISR):
This function is triggered at regular intervals defined by the timer configuration.

ISR(TIMER1_COMPA_vect): This line declares the function as an Interrupt Service Routine (ISR) for the Timer/Counter1 Compare Match A interrupt vector.

Update Servo Position:
The code increments or decrements the currentPositionIndex to move through the servoPositions array in a loop.

Start New Frame:
commandStartNewFrame(uartPixelFormat);: This sends a "new frame" command over UART to indicate the beginning of a new image frame.

Process Frame Data:
processFrameData();: This function call executes either processRgbFrameBuffered or processRgbFrameDirect (depending on the configuration) to handle capturing and transmitting the image frame data.

Frame Counter:
frameCounter++;: This variable keeps track of the total number of frames processed.

Debug Message:
commandDebugPrint("Frame " + String(frameCounter));: This sends a debug message over UART indicating the current frame number.

Move Servo:
myServo.write(servoPositions[currentPositionIndex]);: This line instructs the servo motor to move to the position specified by the current index in the servoPositions array.

Arduino setup() function:
This function runs only once at the beginning of the program.

Serial Communication:
Serial.begin(baud);: This initializes the serial communication (UART) with the specified baud rate (baud defined elsewhere).

Camera Initialization:
It checks if the camera initialization using camera.init() is successful.
Based on the result, it sends either a green or red blank frame using sendBlankFrame (presumably for visual indication).

Timer1 Configuration:

It disables global interrupts (cli()), sets Timer1 registers (TCCR1A and TCCR1B) to zero, and configures the timer for:
A desired timer count (OCR1A) for a 1-second interval (considering clock speed and prescaler).
CTC (Clear Timer on Compare Match) mode.
A 256 prescaler (CS12).
Enabling the Timer Compare Match A interrupt (OCIE1A).
Finally, it re-enables global interrupts (sei()).

Servo Attachment:
myServo.attach(10);: This attaches the servo motor to the digital pin D10 for controlling its movement.

Arduino loop() function:
This function repeatedly executes the main program logic.

Frame Switching Time:
unsigned long lastFrameTime = millis();: This variable stores the time of the last frame processing for timing calculations.

Motion Time:
unsigned long lastMotionTime = 0;: This variable keeps track of the time when motion was last detected (presumably for image processing control).

Processed Byte Count:
processedByteCountDuringCameraRead = 0;: This variable is initialized to zero to track the number of bytes processed during camera reading within the frame processing function.

Start New Frame:
Similar to the timer interrupt routine, it sends a "new frame" command using commandStartNewFrame.

Enable Timer Interrupt:
TIMSK1 |= (1 << OCIE1A);: This enables the Timer Compare Match A interrupt for the servo movement logic.

Frame Counter:
Same as the timer interrupt routine, it increments the frameCounter.

Debug Message:
Similar to the timer interrupt routine, it sends a debug message
*/

PWMServo myServo; // Initialize instance of PWMServo
volatile bool updateServo = false; // Set the initial state to false
int servoPositions[] = {10, 35, 60, 85, 110, 135, 160, 175}; //Array holding the degrees of the servo to be traversed
int currentPositionIndex; // Set the current position to index 0
bool servoDirection; // Set initial direction to forward (optional)
bool firstLoop = true;


// Timer interrupt service routine
ISR(TIMER1_COMPA_vect) {
  // Increment or decrement the position index (wrap around if needed)
// Check current position and direction
  if (currentPositionIndex == sizeof(servoPositions) / sizeof(servoPositions[0]) - 1 && servoDirection) {
    // Reached the end (175 degrees), reverse direction
    currentPositionIndex--;
    servoDirection = false;
  } else if (currentPositionIndex == 0 && !servoDirection) {
    // Reached the beginning (10 degrees), reverse direction
    currentPositionIndex++;
    servoDirection = true;
  } else {
    // Move in the current direction
    if (servoDirection) {
      currentPositionIndex++;
    } else {
      currentPositionIndex--;
    }
  }

  // Move the servo based on the updated index
  myServo.write(servoPositions[currentPositionIndex]);

  // Start a new frame with the specified pixel format
  commandStartNewFrame(uartPixelFormat);

  // Process the frame data
  processFrameData();

  // Increment the frame counter
  frameCounter++;

  // Send a debug message indicating the frame number
  commandDebugPrint("Frame " + String(frameCounter));

  //currentPositionIndex = 0;
}


// Arduino setup()
void initializeScreenAndCamera() {
  Serial.begin(baud);
  if (camera.init()) {
    sendBlankFrame(COLOR_GREEN);
    delay(1000);
  } else {
    sendBlankFrame(COLOR_RED);
    delay(3000);
  }

  // Set up Timer1
  cli(); // Disable global interrupts
  TCCR1A = 0; // Set entire TCCR1A register to 0
  TCCR1B = 0; // Same for TCCR1B

  // Set compare match register to desired timer count:
  OCR1A = 15624; // 1 second at 16MHz with prescaler of 256
  //15624
  // Turn on CTC mode:
  TCCR1B |= (1 << WGM12);

  // Set CS12 bit for 256 prescaler:
  TCCR1B |= (1 << CS12);

  // Enable timer compare interrupt:
  TIMSK1 |= (1 << OCIE1A);

  sei(); // Enable global interrupts
  
  myServo.attach(10); //Set the servo to read from the D10 pin

  servoDirection = true; // Set initial direction to forward (optional)
  
  currentPositionIndex = 0;
}


// Arduino loop()
void processFrame() {
  // Initialize time for frame switching
  unsigned long lastFrameTime = millis();
  unsigned long lastMotionTime = 0;

  if (firstLoop) {
    currentPositionIndex = 0;
    firstLoop = false; // Reset flag after first loop
  }
  
  // Initialize processed byte count during camera read
  processedByteCountDuringCameraRead = 0;

  // Start a new frame with the specified pixel format
  commandStartNewFrame(uartPixelFormat);

  // Enable timer interrupt for servo movement
  TIMSK1 |= (1 << OCIE1A);

  // Increment the frame counter
  frameCounter++;

  // Send a debug message indicating the frame number
  commandDebugPrint("Frame " + String(frameCounter));
}


// This part of code is for image processing and transmission
/*
1. processRgbFrameBuffered (Buffered Processing)

This function takes a buffered approach to handling the image data. Here's a breakdown of the steps:

Line-by-line processing: It iterates through each line (height) of the frame.
Line buffer: It uses a line buffer to temporarily store the pixel data for a single line.
Alternating formatting and sending: Due to timing constraints, the function alternates between formatting the pixel byte (high or low) in the buffer and sending the previously formatted byte over UART.
Flags: It uses flags to track the state of the byte being processed (high or low) and whether it's already formatted.
Sending while buffering: This functionality allows sending data over UART even while capturing the next line of pixels.

2. processRgbFrameDirect (Direct Processing)

This function takes a more direct approach to handling the image data:

Pixel-by-pixel processing: It iterates through each pixel within a line (width) of the frame.
No line buffer: It doesn't use a line buffer. Instead, it processes each pixel byte directly.
Formatting and sending: For each pixel, it reads the byte from the camera, formats it (high or low byte), waits for UART to be ready, and then transmits the formatted byte.


Buffered processing is generally more efficient for high baud rates (faster UART communication) as it avoids waiting for individual bytes to be sent before processing the next pixel.
Direct processing might be simpler to understand but could be less efficient at higher baud rates due to the wait times involved for UART transmission.
Both functions assume helper functions for formatting the individual pixel bytes (high and low) according to the chosen UART pixel format (e.g., RGB565) and handling UART communication details like waiting for transmission to complete.
*/


// For initialization of the first frame to ensure the camera is working properly
void sendBlankFrame(uint16_t color) {
  // Extract the high and low bytes of the specified color
  uint8_t colorH = (color >> 8) & 0xFF;
  uint8_t colorL = color & 0xFF;

  // Start a new frame with the specified pixel format (RGB565)
  commandStartNewFrame(UART_PIXEL_FORMAT_RGB565);

  // Iterate through each line (height) of the frame
  for (uint16_t j = 0; j < lineCount; j++) {
    // Iterate through each pixel in the line (width)
    for (uint16_t i = 0; i < lineLength; i++) {
      // Wait for the previous UART byte to be sent
      waitForPreviousUartByteToBeSent();

      // Send the high byte of the color
      UDR0 = formatRgbPixelByteH(colorH);

      // Wait for the previous UART byte to be sent
      waitForPreviousUartByteToBeSent();

      // Send the low byte of the color
      UDR0 = formatRgbPixelByteL(colorL);
    }
  }
}

// This is for the buffered image processing
void processRgbFrameBuffered() {
  // Wait for the vertical sync signal (Vsync)
  camera.waitForVsync();
  commandDebugPrint("Vsync");

  // Ignore any vertical padding (if present)
  camera.ignoreVerticalPadding();

  // Iterate through each line (height) of the frame
  for (uint16_t y = 0; y < lineCount; y++) {
    // Initialize the line buffer send pointer
    lineBufferSendByte = &lineBuffer[0];
    // Line starts with the high byte
    isLineBufferSendHighByte = true;
    // Flag indicating whether the byte in the line buffer has been formatted
    isLineBufferByteFormatted = false;

    // Ignore any left horizontal padding
    camera.ignoreHorizontalPaddingLeft();

    // Iterate through each pixel in the line (width)
    for (uint16_t x = 0; x < lineBufferLength; x++) {
      // Wait for the rising edge of the pixel clock
      camera.waitForPixelClockRisingEdge();
      // Read the pixel byte from the camera
      camera.readPixelByte(lineBuffer[x]);
      // If sending data while buffering is enabled, process the next RGB pixel byte
      if (isSendWhileBuffering) {
        processNextRgbPixelByteInBuffer();
      }
    }

    // Ignore any right horizontal padding
    camera.ignoreHorizontalPaddingRight();

    // Debug info: Calculate the number of processed bytes during line read
    processedByteCountDuringCameraRead = lineBufferSendByte - (&lineBuffer[0]);

    // Send the remaining part of the line
    while (lineBufferSendByte < &lineBuffer[lineLength * 2]) {
      processNextRgbPixelByteInBuffer();
    }
  }
}


// 1st function for buffered processing
void processNextRgbPixelByteInBuffer() {
  // Format pixel bytes and send them out in different cycles.
  // Due to time constraints, we alternate between formatting and sending.
  if (isLineBufferByteFormatted) {
    // If the byte is already formatted, try to send it
    tryToSendNextRgbPixelByteInBuffer();
  } else {
    // Otherwise, format the next pixel byte
    formatNextRgbPixelByteInBuffer();
  }
}

// 2nd function fot buffered processing
void tryToSendNextRgbPixelByteInBuffer() {
  // Check if the UART is ready for transmission
  if (isUartReady()) {
    // Send the byte pointed to by lineBufferSendByte
    UDR0 = *lineBufferSendByte;
    // Move the pointer to the next byte in the buffer
    lineBufferSendByte++;
    // Mark the byte as unformatted (to be formatted next time)
    isLineBufferByteFormatted = false;
  }
}

// 3rd function for buffered processing
void formatNextRgbPixelByteInBuffer() {
  // Determine whether the current byte is the high or low byte
  if (isLineBufferSendHighByte) {
    // Format the high byte of the RGB pixel
    *lineBufferSendByte = formatRgbPixelByteH(*lineBufferSendByte);
  } else {
    // Format the low byte of the RGB pixel
    *lineBufferSendByte = formatRgbPixelByteL(*lineBufferSendByte);
  }

  // Mark the byte as formatted
  isLineBufferByteFormatted = true;

  // Toggle the flag for the next byte (high or low)
  isLineBufferSendHighByte = !isLineBufferSendHighByte;
}


/// This is for the direct image processing
void processRgbFrameDirect() {
  // Wait for the vertical sync signal (Vsync)
  camera.waitForVsync();
  commandDebugPrint("Vsync");

  // Ignore any vertical padding (if present)
  camera.ignoreVerticalPadding();

  // Iterate through each line (height) of the frame
  for (uint16_t y = 0; y < lineCount; y++) {
    // Ignore any left horizontal padding
    camera.ignoreHorizontalPaddingLeft();

    // Iterate through each pixel in the line (width)
    for (uint16_t x = 0; x < lineLength; x++) {
      // Wait for the rising edge of the pixel clock
      camera.waitForPixelClockRisingEdge();
      // Read the pixel byte from the camera
      camera.readPixelByte(lineBuffer[0]);
      // Format the high byte of the RGB pixel
      lineBuffer[0] = formatRgbPixelByteH(lineBuffer[0]);
      // Wait for the previous UART byte to be sent
      waitForPreviousUartByteToBeSent();
      // Send the formatted high byte over UART
      UDR0 = lineBuffer[0];

      // Wait for the rising edge of the pixel clock
      camera.waitForPixelClockRisingEdge();
      // Read the pixel byte from the camera
      camera.readPixelByte(lineBuffer[0]);
      // Format the low byte of the RGB pixel
      lineBuffer[0] = formatRgbPixelByteL(lineBuffer[0]);
      // Wait for the previous UART byte to be sent
      waitForPreviousUartByteToBeSent();
      // Send the formatted low byte over UART
      UDR0 = lineBuffer[0];
    }

    // Ignore any right horizontal padding
    camera.ignoreHorizontalPaddingRight();
  }
}


// This part of code is for pixel formatting
/*
formatRgbPixelByteH(uint8_t pixelByteH):

This function formats the high byte of an RGB pixel for transmission over UART.
It ensures two things:
Non-zero color value: It guarantees the pixel value is always slightly above zero by using the L_BYTE_PREVENT_ZERO constant. This is because zero is often used as an end-of-line marker in UART communication.
Odd parity: It verifies if an odd number of bits are set in the high byte using the H_BYTE_PARITY_CHECK constant. It then adjusts the parity bit (H_BYTE_PARITY_INVERT) to maintain an odd number of set bits for error detection during transmission.

formatRgbPixelByteL(uint8_t pixelByteL):
This function formats the low byte of an RGB pixel for UART transmission.
Similar to the high byte function, it ensures:
Non-zero color value: The pixel value is kept slightly above zero using L_BYTE_PREVENT_ZERO.
Even parity: It checks if an even number of bits are set in the low byte using L_BYTE_PARITY_CHECK and adjusts the parity bit (L_BYTE_PARITY_INVERT) to achieve even parity.
*/

// Format the high byte of an RGB pixel
// Ensures that:
// A: The pixel color is always slightly above 0 (since zero is an end-of-line marker)
// B: An odd number of bits are set for the H byte under H_BYTE_PARITY_CHECK and H_BYTE_PARITY_INVERT
uint8_t formatRgbPixelByteH(uint8_t pixelByteH) {
  // Check if the parity check bit is set
  if (pixelByteH & H_BYTE_PARITY_CHECK) {
    // Clear the parity inversion bit to maintain an odd number of set bits
    return pixelByteH & (~H_BYTE_PARITY_INVERT);
  } else {
    // Set the parity inversion bit to ensure an odd number of set bits
    return pixelByteH | H_BYTE_PARITY_INVERT;
  }
}



// Format the low byte of an RGB pixel
// Ensures that:
// A: The pixel color is always slightly above 0 (to avoid using 0 as an end-of-line marker)
// B: An even number of bits are set for the L byte under L_BYTE_PARITY_CHECK and L_BYTE_PARITY_INVERT
uint8_t formatRgbPixelByteL(uint8_t pixelByteL) {
  // Check if the parity check bit is set
  if (pixelByteL & L_BYTE_PARITY_CHECK) {
    // Set the parity inversion bit and prevent zero value
    return pixelByteL | L_BYTE_PARITY_INVERT | L_BYTE_PREVENT_ZERO;
  } else {
    // Clear the parity inversion bit and prevent zero value
    return (pixelByteL & (~L_BYTE_PARITY_INVERT)) | L_BYTE_PREVENT_ZERO;
  }
}


// This part of code is for UART Communication
/*
commandStartNewFrame(uint8_t pixelFormat):

This function sends a "new frame" command to the receiving device over UART, indicating the start of a new image frame.
Here's a breakdown of the steps:
Command marker: It transmits a new command marker (0x00) to signal the start of a command sequence.
Command length: It sends the length of the following command data (4 bytes in this case).
Checksum calculation: It calculates a checksum for error detection during transmission. This involves XOR-ing each command byte with the current checksum value.
Command data: It sends the following information:
COMMAND_NEW_FRAME: This constant specifies the "new frame" command type.
Lower 8 bits of image width (lineLength & 0xFF)
Lower 8 bits of image height (lineCount & 0xFF)
A combination of higher 2 bits of width ((lineLength >> 8) & 0x03), higher 2 bits of height ((lineCount >> 6) & 0x0C), and the pixel format ((pixelFormat << 4) & 0xF0) packed into a single byte.
Checksum byte: Finally, it transmits the calculated checksum for error verification at the receiving end.

commandDebugPrint(const String debugText):
This function transmits a debug message over UART, typically used for debugging purposes.
It follows a similar structure to commandStartNewFrame:
Command marker: Sends a new command marker (0x00).
Command length: Sends the total length of the debug text message (including the command code).
Checksum calculation: Initializes a checksum variable and calculates it over the following bytes.
Command code: Sends the COMMAND_DEBUG_DATA constant to indicate a debug message.
Debug text: Iterates through each character of the debugText string and transmits each character while updating the checksum.
Checksum byte: Transmits the final checksum value.

sendNextCommandByte(uint8_t checksum, uint8_t commandByte):
This helper function transmits a single command byte over UART and updates the checksum for error detection.
It waits for the previous byte to be transmitted and then sends the provided commandByte.
It calculates the XOR of the current checksum and the commandByte and returns the updated checksum value.

waitForPreviousUartByteToBeSent():
This function simply waits until the previous byte transmission over UART is complete.
It checks the UDRE0 bit (USART Data Register Empty) in the UCSR0A register to determine if the transmit buffer is empty.

isUartReady():
This function checks if the UART is ready to transmit another byte.
It returns true if the UDRE0 bit (USART Data Register
*/

void commandStartNewFrame(uint8_t pixelFormat) {
  // Send the new command marker (0x00)
  waitForPreviousUartByteToBeSent();
  UDR0 = 0x00;

  // Send the command length (4 bytes)
  waitForPreviousUartByteToBeSent();
  UDR0 = 4;

  // Calculate the checksum for error detection
  uint8_t checksum = 0;
  checksum = sendNextCommandByte(checksum, COMMAND_NEW_FRAME);
  checksum = sendNextCommandByte(checksum, lineLength & 0xFF); // Lower 8 bits of image width
  checksum = sendNextCommandByte(checksum, lineCount & 0xFF); // Lower 8 bits of image height
  checksum = sendNextCommandByte(checksum,
                                 ((lineLength >> 8) & 0x03) // Higher 2 bits of image width
                                 | ((lineCount >> 6) & 0x0C) // Higher 2 bits of image height
                                 | ((pixelFormat << 4) & 0xF0));

  // Send the checksum byte
  waitForPreviousUartByteToBeSent();
  UDR0 = checksum;
}

// Send a debug message over UART
void commandDebugPrint(const String debugText) {
  if (debugText.length() > 0) {
    // Send the new command marker (0x00)
    waitForPreviousUartByteToBeSent();
    UDR0 = 0x00;

    // Calculate the command length (debugText length + 1 for command code)
    waitForPreviousUartByteToBeSent();
    UDR0 = debugText.length() + 1;

    // Calculate the checksum for error detection
    uint8_t checksum = 0;
    // Send the command code (COMMAND_DEBUG_DATA)
    checksum = sendNextCommandByte(checksum, COMMAND_DEBUG_DATA);
    // Send each character of the debugText and update the checksum
    for (uint16_t i = 0; i < debugText.length(); i++) {
      checksum = sendNextCommandByte(checksum, debugText[i]);
    }

    // Send the checksum byte
    waitForPreviousUartByteToBeSent();
    UDR0 = checksum;
  }
}


// Send the next command byte over UART
// Calculates a checksum for error detection
uint8_t sendNextCommandByte(uint8_t checksum, uint8_t commandByte) {
  // Wait until the previous UART byte has been transmitted
  waitForPreviousUartByteToBeSent();

  // Send the command byte
  UDR0 = commandByte;

  // Calculate the checksum by XOR-ing with the command byte
  return checksum ^ commandByte;
}

// Wait until the previous UART byte has been successfully transmitted
void waitForPreviousUartByteToBeSent() {
  while (!isUartReady()); // Wait for the byte to transmit
}

// Check if the UART (USART Data Register Empty) is ready for transmission
bool isUartReady() {
  // The UDRE0 bit in UCSR0A indicates whether the transmit buffer is empty
  return UCSR0A & (1 << UDRE0);
}


#endif
