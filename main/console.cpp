/*
  REPL console implementation via ESP32-IDF console library
*/

#include "console.h"
#include "network.h"
#include "storage.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <M5Core2.h>
#include <string.h>
#include "global.h"

static esp_console_repl_t *repl;

static struct {
  struct arg_rex *ap = arg_rex0(NULL, NULL, "ap", NULL, ARG_REX_ICASE, "Configure access point");
  struct arg_rex *sta = arg_rex0(NULL, NULL, "station", NULL, ARG_REX_ICASE, "Configure station mode");
  struct arg_str *ssid = arg_str1(NULL, NULL, "<ssid>", "SSID for access point");
  struct arg_str *password = arg_str1(NULL, NULL, "<pass>", "Password for access point, must be at least 8 characters");
  struct arg_end *end = arg_end(2);
} wifiConf_args;

static struct {
    struct arg_str *ip;
    struct arg_int *port;
    struct arg_end *end;
} dbConf_args;

static struct {
    struct arg_int *time;
    struct arg_end *end;
} setFreq_args;

static struct {
  struct arg_str *timestamp_start;
  struct arg_str *timestamp_end;
  struct arg_end *end;
} recoverData_args;

void init_console() {
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

  esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
  esp_console_register_help_command();
  esp_console_start_repl(repl);

  register_wifiConf_cmd();
  register_dbConf_cmd();
  register_setFreq_cmd();
  register_recoverData_cmd();
}

/* 
  Implementation of wificonf command, relies on network.cpp
*/
static int wifiConf_impl(int argc, char** argv) {
  int err = arg_parse(argc, argv, (void **) &wifiConf_args);
  if (err != 0) {
      arg_print_errors(stderr, wifiConf_args.end, argv[0]);
      return 1;
  }
  if ((wifiConf_args.ap->count + wifiConf_args.sta->count) != 1) {
    printf("Please specify either the 'ap' flag or 'station' flag.\n");
    return 1;
  }
  bool isAP = true;

  if(wifiConf_args.ap->count) {
    if (strlen(wifiConf_args.password->sval[0]) < 8) {
      printf("Password too short!\n");
      return 1;
    }
    printf("Setting up AP with SSID '%s' and password '%s'\n",
          wifiConf_args.ssid->sval[0], wifiConf_args.password->sval[0]);
  }
  else {
    isAP = false;
    printf("Enabling station mode and connecting to SSID '%s'\n",
          wifiConf_args.ssid->sval[0]);
  }

  bool connected = start_wifi_cmd(wifiConf_args.ssid->sval[0],
                                  wifiConf_args.password->sval[0],
                                  isAP);
  if(!connected) {
    printf("Failed to start wifi mode\n");
    return 1;
  }
  printf("Configured wifi\n");
  return 0;
}

void register_wifiConf_cmd() {
  esp_console_cmd_t wifiConf_cmd {
    .command = "wificonf",
    .help = "Configure WiFi access point",
    .hint = NULL,
    .func = &wifiConf_impl,
    .argtable = &wifiConf_args
  };

  esp_console_cmd_register(&wifiConf_cmd);
}

/*
  Implementation of dbconf command.
*/
static int dbConf_impl(int argc, char** argv) {
  int err = arg_parse(argc, argv, (void **) &dbConf_args);
  if (err != 0) {
      arg_print_errors(stderr, dbConf_args.end, argv[0]);
      return 1;
  }
  printf("Connecting to DB with ip '%s' and port %d\n",
           dbConf_args.ip->sval[0], dbConf_args.port->ival[0]);
           
  start_http_server(dbConf_args.ip->sval[0], dbConf_args.port->ival[0]);
  return 0;
}

void register_dbConf_cmd() {
  dbConf_args.ip = arg_str1(NULL, NULL, "<ip>", "IP address for InfluxDB");
  dbConf_args.port = arg_int1(NULL, NULL, "<port>", "Port used for InfluxDB");
  dbConf_args.end = arg_end(2);

  esp_console_cmd_t dbConf_cmd {
    .command = "dbconf",
    .help = "Configure InfluxDB connection",
    .hint = NULL,
    .func = &dbConf_impl,
    .argtable = &dbConf_args
  };

  esp_console_cmd_register(&dbConf_cmd);
}

/*
  Implementation of setfreq command. Deletes old clock and starts new one.
*/
static int setFreq_impl(int argc, char** argv) {
  int err = arg_parse(argc, argv, (void **) &setFreq_args);
  if (err != 0) {
      arg_print_errors(stderr, setFreq_args.end, argv[0]);
      return 1;
  }
  printf("Changing data storage frequency to %d\n",
           setFreq_args.time->ival[0]);
           
  if(xTimerChangePeriod(threadTimer, pdMS_TO_TICKS(setFreq_args.time->ival[0] * 1000), 300) == pdFAIL) {
    printf("Failed to change timer period\n");
    return 1;
  }
  return 0;
}

void register_setFreq_cmd() {
  setFreq_args.time = arg_int1(NULL, NULL, "<time>", "Data storage frequency, in seconds");
  setFreq_args.end = arg_end(2);

  esp_console_cmd_t setFreq_cmd {
    .command = "setfreq",
    .help = "Set data storage frequency",
    .hint = NULL,
    .func = &setFreq_impl,
    .argtable = &setFreq_args
  };

  esp_console_cmd_register(&setFreq_cmd);
}

/*
  Implementation of recoverData command. Retrieves all data in SD card stored between the two dates
*/
static int recoverData_impl(int argc, char** argv) {
  int err = arg_parse(argc, argv, (void **) &recoverData_args);
  if (err != 0) {
      arg_print_errors(stderr, recoverData_args.end, argv[0]);
      return 1;
  }
  struct tm time_start = {0};
  struct tm time_end = {0};
  printf("Sending data stored between %s and %s\n",
           recoverData_args.timestamp_start->sval[0],
           recoverData_args.timestamp_end->sval[0]);
  if(strptime(recoverData_args.timestamp_start->sval[0], "%Y/%m/%d %H:%M:%S", &time_start) == NULL) {
    printf("Malformed start timestamp, could not convert to UNIX timestamp\n");
    return 1;
  }
  if(strptime(recoverData_args.timestamp_end->sval[0], "%Y/%m/%d %H:%M:%S", &time_end) == NULL) {
    printf("Malformed end timestamp, could not convert to UNIX timestamp\n");
    return 1;
  }
  time_start.tm_isdst = _daylight;
  time_end.tm_isdst = _daylight;
  readDataAndQueue(mktime(&time_start), mktime(&time_end));

  return 0;
}

void register_recoverData_cmd() {
  recoverData_args.timestamp_start = arg_str1(NULL, NULL, "<time_start>", "Beginning timestamp, in YYYY/MM/DD HH:MM:SS format");
  recoverData_args.timestamp_end = arg_str1(NULL, NULL, "<time_end>", "Ending timestamp, in YYYY/MM/DD HH:MM:SS format");
  recoverData_args.end = arg_end(2);

  esp_console_cmd_t recoverData_cmd {
    .command = "recoverData",
    .help = "Send all data stored in SD card within the timestamp range to local DB",
    .hint = NULL,
    .func = &recoverData_impl,
    .argtable = &recoverData_args
  };

  esp_console_cmd_register(&recoverData_cmd);
}

