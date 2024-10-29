#include <M5Core2.h>
#include "storage.h"
#include "helper.h"
#include "global.h"

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;
sensor_data storageData;
char SDStrbuff[64];

void store_data(void* _) {
  while(1) {
    // Wait to receive data from queue
    xQueueReceive(storageQueue, &storageData, portMAX_DELAY);
    writeToScreen((M5.Lcd.width()-(11*6))/2, 120, "                      ");
    // Create new log every day
    M5.Rtc.GetDate(&RTCDate);
    sprintf(SDStrbuff, "/weather-data_%d-%02d-%02d.csv", RTCDate.Year, 
          RTCDate.Month, RTCDate.Date);
    File file = SD.open(SDStrbuff, FILE_APPEND);
    if (file) {
      sprintf(SDStrbuff, "%d/%02d/%02d %02d:%02d:%02d", RTCDate.Year,
          RTCDate.Month, RTCDate.Date, RTCtime.Hours, RTCtime.Minutes,
          RTCtime.Seconds);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.rain_fall);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.wind_speed);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.wind_direction);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.temperature);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.humidity);
      file.printf(SDStrbuff);
      sprintf(SDStrbuff, ",%f", storageData.pressure);
      file.printf(SDStrbuff);

      file.printf("\n");
      file.close();
      writeToScreen((M5.Lcd.width()-(11*6))/2, 120, "Wrote to SD");
    } 
    else {
      writeToScreen((M5.Lcd.width()-(11*6))/2, 120, "Error writing to file", RED, BLACK);
    }
    sleep(2000);
    writeToScreen((M5.Lcd.width()-(11*6))/2, 120, "                            ");
    sleep(1000);
  }
}