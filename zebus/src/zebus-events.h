#ifndef ZEBUS_EVENTS_H
#define ZEBUS_EVENTS_H

#include <esp_event.h>
#include <espnow-types.h>

ESP_EVENT_DECLARE_BASE(ZEBUS_EVENTS);

enum zebus_events : uint8_t {
    EVNT_RECVD_FLOW,
    EVNT_RECVD_FLOW_TEMP,
    EVNT_RECVD_RETURN_TEMP,
    EVNT_RECVD_MAX_FLOW_SETPOINT,
    EVNT_RECVD_MODULATION,
    EVNT_RECVD_SENSOR,
    EVNT_RECVD_FLAME,
    EVNT_RECVD_EBUS_CONTROL,
    EVNT_UPD_QUEUE_SIZE,
    MAX_EVNT,
};

void initEventLoop();

#endif
