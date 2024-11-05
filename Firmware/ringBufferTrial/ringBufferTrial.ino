#include <Arduino.h>
#include "freertos/ringbuf.h"
#include "esp32-hal-log.h"

RingbufHandle_t ring_buffer_handle;

static char tx_item[] = "Hello World!";

uint8_t count = 0;

enum PACKET_ELEMENTS{
  WAIT_FOR_RESPONSE = 0xFFFD,
  NO_RESPONSE = 0xFFFE,
  PACKET_ELEMENT_ERROR = 0xFFFF,
};

uint16_t trial_packet[] = {0x5A, 0x80, 0x00, 0x06, 0x11, 0xF1, NO_RESPONSE};
uint16_t trial_packet_2[] = {0x46, 0x80, 0x00, 0x06, 0x13, 0xF1, WAIT_FOR_RESPONSE};

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  ring_buffer_handle = xRingbufferCreate(1028, RINGBUF_TYPE_BYTEBUF);
  if (ring_buffer_handle == NULL){
    log_e("Failed to create ring buffer!");
    while(1){
      ;
    }
  }

}

void loop() {
  
  // Send the item to the ring buffer
  UBaseType_t res =  xRingbufferSend(ring_buffer_handle, trial_packet, sizeof(trial_packet), pdMS_TO_TICKS(1000));
  if (res != pdTRUE) {
      printf("Failed to send item\n");
  }

  //Receive an item from the ring buffer
  size_t item_size;
  uint16_t *item = (uint16_t *)xRingbufferReceive(ring_buffer_handle, &item_size, pdMS_TO_TICKS(1000));
  
  if (item != NULL) {
      log_i("Item length: %i", item_size);
      //Print item
      for (int i = 0; i < item_size; i++) {
          printf("%04x ", item[i]);
      }
      printf("\n");
      //Return Item
      vRingbufferReturnItem(ring_buffer_handle, (void *)item);
  } else {
      //Failed to receive item
      printf("Failed to receive item\n");
  }

  if (count >= 1){
    count = 0;
  } else {
    count++;
    res =  xRingbufferSend(ring_buffer_handle, trial_packet_2, sizeof(trial_packet_2), pdMS_TO_TICKS(1000));
    if (res != pdTRUE) {
        printf("Failed to send item\n");
    }
  }
  

  vTaskDelay(pdMS_TO_TICKS(1000));
}
