#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "HEADPHONE";

// ==================== HARDWARE PINS ====================
#define LED_GPIO        GPIO_NUM_22
#define PA_CTRL         GPIO_NUM_21      // Speaker amplifier control
#define PJ_DET          GPIO_NUM_19      // Headphone jack detection (0=plugged)

// ==================== I2C PINS ====================
#define I2C_SDA         GPIO_NUM_18
#define I2C_SCL         GPIO_NUM_23
#define I2C_PORT        I2C_NUM_0
#define ES8311_ADDR     0x18             // Found by your scanner

// ==================== I2S for ES8311 DAC ====================
#define I2S_PORT        I2S_NUM_0
#define I2S_MCLK        GPIO_NUM_0
#define I2S_BCLK        GPIO_NUM_5
#define I2S_WS          GPIO_NUM_25
#define I2S_DOUT        GPIO_NUM_26

#define SAMPLE_RATE     16000

static i2s_chan_handle_t tx_handle;
static int audio_volume = 28000;  // Global volume control

static esp_err_t es8311_write(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static uint8_t es8311_read(uint8_t reg)
{
    uint8_t val = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return val;
}

static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(TAG, "I2C initialized (ES8311 at 0x%02X)", ES8311_ADDR);
}

static void i2s_dac_init(void)
{
    ESP_LOGI(TAG, "Initializing I2S DAC...");
    
    i2s_chan_config_t chan_cfg = {
        .id = I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 240,
        .auto_clear = true,
    };
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 16,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_LOGI(TAG, "I2S DAC initialized");
}

static void es8311_init(void)
{
    ESP_LOGI(TAG, "Initializing ES8311 for Headphone Output...");
    
    // Check communication
    uint8_t chip_id = es8311_read(0xFD);
    ESP_LOGI(TAG, "ES8311 Chip ID = 0x%02X (should be 0x83)", chip_id);
    
    if (chip_id != 0x83) {
        ESP_LOGE(TAG, "Wrong chip ID!");
        return;
    }
    
    // ===== STEP 1: SOFTWARE RESET =====
    ESP_LOGI(TAG, "Step 1: Software Reset");
    es8311_write(0x00, 0x1F);  // Reset all except I2C
    vTaskDelay(pdMS_TO_TICKS(100));
    es8311_write(0x00, 0x00);  // Clear reset
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // ===== STEP 2: CLOCK CONFIGURATION =====
    ESP_LOGI(TAG, "Step 2: Clock Configuration");
    es8311_write(0x01, 0x3F);  // Enable all clocks
    es8311_write(0x02, 0x10);  // MCLK input
    es8311_write(0x03, 0x10);  // ADC OSR = 64 (for 48kHz)
    es8311_write(0x04, 0x10);  // DAC OSR = 64 (for 48kHz)
    es8311_write(0x05, 0x00);  // ADC/DAC clock dividers
    es8311_write(0x06, 0x03);  // BCLK divider
    
    // ===== STEP 3: I2S FORMAT =====
    ESP_LOGI(TAG, "Step 3: I2S Format");
    es8311_write(0x09, 0x0C);  // I2S, 16-bit, left channel to DAC
    es8311_write(0x0A, 0x00);  // I2S output format
    
    // ===== STEP 4: DAC CONFIGURATION =====
    ESP_LOGI(TAG, "Step 4: DAC Configuration");
    es8311_write(0x31, 0x00);  // DAC unmute
    es8311_write(0x32, 0x00);  // DAC volume scale 0dB
    es8311_write(0x33, 0x40);  // DAC volume control (0dB)
    es8311_write(0x34, 0x00);  // DRC disable
    es8311_write(0x35, 0x00);  // DRC levels
    es8311_write(0x36, 0x00);  // DRC winsize
    es8311_write(0x37, 0x08);  // DAC ramp rate
    
    // ===== STEP 5: POWER SEQUENCE =====
    ESP_LOGI(TAG, "Step 5: Power Up Sequence");
    
    // Power up analog circuits
    es8311_write(0x0D, 0x01);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Power up DAC modulator
    es8311_write(0x0E, 0x02);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // System power management
    es8311_write(0x0F, 0x44);
    es8311_write(0x10, 0x19);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // ===== STEP 6: OUTPUT MIXER =====
    ESP_LOGI(TAG, "Step 6: Output Mixer Configuration");
    es8311_write(0x17, 0xFF);  // Enable all output mixer blocks
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // ===== STEP 7: OUTPUT ROUTING =====
    ESP_LOGI(TAG, "Step 7: Output Routing");
    es8311_write(0x37, 0x48);  // DAC to OUTP/OUTN routing
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // ===== STEP 8: VOLUME CONTROL =====
    ESP_LOGI(TAG, "Step 8: Volume Control");
    es8311_write(0x15, 0x40);  // Volume transition time
    es8311_write(0x16, 0xC3);  // Ramp rate
    es8311_write(0x18, 0x00);  // Left channel volume (0dB max)
    es8311_write(0x19, 0x00);  // Right channel volume (0dB max)
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // ===== FINAL STATUS =====
    ESP_LOGI(TAG, "\n=== ES8311 STATUS ===");
    ESP_LOGI(TAG, "DAC Mute (0x31): 0x%02X %s", es8311_read(0x31), 
             (es8311_read(0x31) == 0x00) ? "✓ UNMUTED" : "✗ MUTED");
    ESP_LOGI(TAG, "DAC Power (0x0E): 0x%02X", es8311_read(0x0E));
    ESP_LOGI(TAG, "System Power (0x10): 0x%02X", es8311_read(0x10));
    ESP_LOGI(TAG, "Output Mixer (0x17): 0x%02X", es8311_read(0x17));
    ESP_LOGI(TAG, "Routing (0x37): 0x%02X", es8311_read(0x37));
    ESP_LOGI(TAG, "Volume L (0x18): 0x%02X (0dB)", es8311_read(0x18));
    ESP_LOGI(TAG, "Volume R (0x19): 0x%02X (0dB)", es8311_read(0x19));
    
    if ((es8311_read(0x0E) & 0x02) && (es8311_read(0x10) == 0x19) && (es8311_read(0x17) == 0xFF)) {
        ESP_LOGI(TAG, "✅ ES8311 configured for headphone output!");
        ESP_LOGI(TAG, "Audio should be available on OUTP/OUTN pins");
    } else {
        ESP_LOGE(TAG, "❌ ES8311 configuration incomplete");
    }
}

static void generate_sine_wave(int16_t *buf, int samples, int freq)
{
    static float phase = 0;
    float inc = 2 * M_PI * freq / SAMPLE_RATE;
    
    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(audio_volume * sin(phase));
        phase += inc;
        if (phase >= 2 * M_PI) phase -= 2 * M_PI;
    }
}

static void test_audio_path(void)
{
    ESP_LOGI(TAG, "\n=== TESTING AUDIO PATH ===");
    
    // Test 1: Toggle PA_CTRL to check for clicks
    ESP_LOGI(TAG, "Test 1: Toggling PA_CTRL - listen for clicks/pops");
    for (int i = 0; i < 5; i++) {
        gpio_set_level(PA_CTRL, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PA_CTRL, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    gpio_set_level(PA_CTRL, 1);  // Leave enabled
    
    // Test 2: Try different output routings
    ESP_LOGI(TAG, "\nTest 2: Trying alternative routings (5 seconds each)");
    uint8_t routes[] = {0x48, 0x4C, 0x58, 0x5C};
    for (int i = 0; i < 4; i++) {
        es8311_write(0x37, routes[i]);
        ESP_LOGI(TAG, "Testing routing 0x%02X for 3 seconds...", routes[i]);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    es8311_write(0x37, 0x48);  // Restore original routing
    
    // Test 3: Try maximum volume
    ESP_LOGI(TAG, "\nTest 3: Setting maximum volume");
    es8311_write(0x18, 0xC0);
    es8311_write(0x19, 0xC0);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 4: Restore normal volume
    es8311_write(0x18, 0x00);
    es8311_write(0x19, 0x00);
    ESP_LOGI(TAG, "Restored normal volume");
}

void app_main(void)
{
    // Configure headphone detection and speaker control
    gpio_set_direction(PJ_DET, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PJ_DET, GPIO_PULLUP_ONLY);
    gpio_set_direction(PA_CTRL, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    
    // Check if headphones are plugged in
    int headphones_plugged = (gpio_get_level(PJ_DET) == 0);
    
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "HEADPHONE OUTPUT TEST");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "PJ_DET (GPIO19) = %d (%s)", gpio_get_level(PJ_DET), 
             headphones_plugged ? "HEADPHONES PLUGGED" : "NO HEADPHONES");
    
    if (headphones_plugged) {
        ESP_LOGI(TAG, "✅ Headphones detected!");
        // ENABLE speaker amplifier for headphones
        gpio_set_level(PA_CTRL, 1);
        ESP_LOGI(TAG, "PA_CTRL = 1 (Speaker amplifier ENABLED for headphone output)");
    } else {
        ESP_LOGI(TAG, "⚠️ Please plug headphones into the AUDIO OUTPUT jack");
        gpio_set_level(PA_CTRL, 0);
    }
    ESP_LOGI(TAG, "========================================\n");
    
    // Initialize audio
    i2s_dac_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    es8311_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Run audio path tests
    test_audio_path();
    
    ESP_LOGI(TAG, "\nPlaying 440Hz tone through headphones...");
    ESP_LOGI(TAG, "If you don't hear anything, check:");
    ESP_LOGI(TAG, "  1. Headphones are plugged into the correct jack");
    ESP_LOGI(TAG, "  2. Volume is turned up on headphones");
    ESP_LOGI(TAG, "  3. There might be a separate headphone amp enable pin");
    ESP_LOGI(TAG, "========================================\n");
    
    int16_t buf[256];
    size_t written;
    int test_count = 0;
    
    while (1) {
        // Check if headphones are still plugged in
        if (gpio_get_level(PJ_DET) == 0) {
            generate_sine_wave(buf, 256, 440);
            i2s_channel_write(tx_handle, buf, sizeof(buf), &written, portMAX_DELAY);
            
            // Blink LED
            static int cnt = 0;
            if (cnt++ % 100 == 0) {
                gpio_set_level(LED_GPIO, !gpio_get_level(LED_GPIO));
                test_count++;
                
                // Every 30 seconds, print status
                if (test_count % 300 == 0) {
                    ESP_LOGI(TAG, "Audio playing... Volume: %d", audio_volume);
                }
            }
        } else {
            // No headphones - turn off audio and LED
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}