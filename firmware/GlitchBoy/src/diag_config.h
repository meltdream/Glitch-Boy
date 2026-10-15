#ifndef DIAG_CONFIG_H
#define DIAG_CONFIG_H

#ifndef EVENT_LOGS
#define EVENT_LOGS 1
#endif

#ifndef EVENT_LOG_BAUD
#define EVENT_LOG_BAUD 115200u
#endif

#ifndef BLE_HOGP_ENABLED
#define BLE_HOGP_ENABLED 1
#endif

#ifndef DIAG_FLAGS_ENABLE
#define DIAG_FLAGS_ENABLE 0
#endif

#ifndef SERIAL_DIAG_LOGS
#define SERIAL_DIAG_LOGS DIAG_FLAGS_ENABLE
#endif

#endif
