#include "Arduino_BMI270_BMM150.h"  // Library for the BMI270 (accelerometer/gyroscope) and BMM150 (magnetometer)
#include <Arduino_APDS9960.h>       // Library for the APDS9960 (light sensor)
#include <PDM.h>                    // Library for the microphone (PDM)

#define MIC_PIN 2  // Microphone connected to pin 2 (PDM interface)

// // Create instances for the sensors
// Arduino_BMI270_BMM150 IMU;         // Instance for the BMI270 IMU (accelerometer/gyroscope + magnetometer)
// Arduino_APDS9960 APDS;             // Instance for the APDS9960 light sensor

short samples[256];  // Buffer for microphone samples

// Number of audio samples read
volatile int samplesRead;

// edge impulse
#include <PDM.h>
#include <PROJET_MICRO_ARDUINO_inferencing.h>

/** Audio buffers, pointers and selectors */
typedef struct {
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

/**
 * @brief      Arduino setup function
 */

void setup() {
  // Start serial communication
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Started");

  // Initialize the IMU (BMI270 and BMM150)
  if (!IMU.begin()) {
    Serial.println("Error initializing BMI270 IMU!");
    while (1);  // Halt if initialization fails
  }

  // Initialize the light sensor (APDS9960)
  if (!APDS.begin()) {
    Serial.println("Error initializing APDS9960 Light Sensor!");
    while (1);  // Halt if initialization fails
  }

  // Initialize the microphone (PDM)
  PDM.onReceive(onPDMData);
  PDM.begin(1, 16000);  // Initialize PDM microphone with one channel and 16 kHz sample rate
  PDM.setGain(20);      // Set microphone gain




    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }
}


void loop() {
  // Read accelerometer and gyroscope data
  float gx, gy, gz;
  IMU.readGyroscope(gx, gy, gz);  // Get gyroscope values

  // Read ambient light level (APDS9960)
  int proximity = 0;
  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
  }

  // Read sound level (from microphone)
  int soundLevel = getSoundLevel();

  // edge impulse
    ei_printf("Starting inferencing in 2 seconds...\n");

    delay(100);

    ei_printf("Recording...\n");

    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    ei_printf("Recording done\n");

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif



  Serial.print(" | ");
  Serial.print(gx);
  Serial.print(", ");
  Serial.print(gy);
  Serial.print(", ");
  Serial.print(gz);


  Serial.print(" ");
  Serial.print(proximity);

  Serial.print(" ");
  Serial.println(soundLevel);

  // Wait for a while before collecting the next set of data
  delay(500);
}

// Function to calculate sound level based on microphone data
int getSoundLevel() {
  int soundValue = 0;
  int sampleCount = sizeof(samples) / sizeof(samples[0]);

  // Accumulate absolute values of all microphone samples
  for (int i = 0; i < sampleCount; i++) {
    soundValue += abs(samples[i]);
  }

  // Return the average sound value
  return soundValue / sampleCount;
}

// PDM callback function to collect samples from the microphone
void onPDMData() {
  int bytesRead = PDM.available();
  if (bytesRead) {
    PDM.read(samples, bytesRead);  // Read PDM data into the samples buffer
  }
}
static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (inference.buf_ready == 0) {
        for(int i = 0; i < bytesRead>>1; i++) {
            inference.buffer[inference.buf_count++] = sampleBuffer[i];

            if(inference.buf_count >= inference.n_samples) {
                inference.buf_count = 0;
                inference.buf_ready = 1;
                break;
            }
        }
    }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if(inference.buffer == NULL) {
        return false;
    }

    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize(4096);

    // initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
        microphone_inference_end();

        return false;
    }

    // set the gain, defaults to 20
    PDM.setGain(127);

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;

    while(inference.buf_ready == 0) {
        delay(10);
    }

    return true;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
