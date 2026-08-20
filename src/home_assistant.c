#include "home_assistant.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

enum {
    HTTP_REQUEST_SIZE = 768,
    HTTP_RESPONSE_SIZE = 1024,
    HTTP_TIMEOUT_MS = 10000
};

typedef enum {
    HTTP_PENDING,
    HTTP_OK,
    HTTP_NETWORK_ERROR,
    HTTP_RESPONSE_ERROR
} HttpStatus;

typedef struct {
    struct tcp_pcb *pcb;
    const char *host;
    uint16_t port;
    char request[HTTP_REQUEST_SIZE];
    char response[HTTP_RESPONSE_SIZE];
    size_t response_len;
    HttpStatus status;
    bool done;
} HttpRequest;

static HttpRequest http;

static void finish(HttpRequest *request, HttpStatus status) {
    request->status = status;
    request->done = true;
}

static void close_connection(HttpRequest *request) {
    if (request->pcb == NULL) {
        return;
    }

    tcp_arg(request->pcb, NULL);
    tcp_recv(request->pcb, NULL);
    tcp_err(request->pcb, NULL);
    if (tcp_close(request->pcb) != ERR_OK) {
        tcp_abort(request->pcb);
    }
    request->pcb = NULL;
}

static err_t receive(void *arg, struct tcp_pcb *pcb, struct pbuf *data, err_t error) {
    HttpRequest *request = arg;
    if (data == NULL) {
        request->pcb = pcb;
        close_connection(request);
        finish(request, error == ERR_OK ? HTTP_OK : HTTP_NETWORK_ERROR);
        return ERR_OK;
    }

    if (error != ERR_OK) {
        pbuf_free(data);
        finish(request, HTTP_NETWORK_ERROR);
        return error;
    }

    u16_t total = data->tot_len;
    size_t available = sizeof(request->response) - request->response_len - 1U;
    size_t size = total < available ? total : available;
    if (size > 0) {
        pbuf_copy_partial(data, request->response + request->response_len, size, 0);
        request->response_len += size;
        request->response[request->response_len] = '\0';
    }
    tcp_recved(pcb, total);
    pbuf_free(data);

    if (size < total) {
        request->pcb = pcb;
        close_connection(request);
        finish(request, HTTP_RESPONSE_ERROR);
    }
    return ERR_OK;
}

static void connection_error(void *arg, err_t error) {
    HttpRequest *request = arg;
    request->pcb = NULL;
    finish(request, error == ERR_OK ? HTTP_OK : HTTP_NETWORK_ERROR);
}

static err_t connected(void *arg, struct tcp_pcb *pcb, err_t error) {
    HttpRequest *request = arg;
    if (error != ERR_OK) {
        finish(request, HTTP_NETWORK_ERROR);
        return error;
    }

    request->pcb = pcb;
    err_t result = tcp_write(pcb,
                             request->request,
                             strlen(request->request),
                             TCP_WRITE_FLAG_COPY);
    if (result == ERR_OK) {
        result = tcp_output(pcb);
    }
    if (result != ERR_OK) {
        close_connection(request);
        finish(request, HTTP_NETWORK_ERROR);
    }
    return result;
}

static void connect_to_host(HttpRequest *request, const ip_addr_t *address) {
    request->pcb = tcp_new_ip_type(IP_GET_TYPE(address));
    if (request->pcb == NULL) {
        finish(request, HTTP_NETWORK_ERROR);
        return;
    }

    tcp_arg(request->pcb, request);
    tcp_recv(request->pcb, receive);
    tcp_err(request->pcb, connection_error);
    if (tcp_connect(request->pcb, address, request->port, connected) != ERR_OK) {
        close_connection(request);
        finish(request, HTTP_NETWORK_ERROR);
    }
}

static void resolved(const char *name, const ip_addr_t *address, void *arg) {
    (void)name;
    HttpRequest *request = arg;
    if (address == NULL) {
        finish(request, HTTP_NETWORK_ERROR);
        return;
    }
    connect_to_host(request, address);
}

static HttpStatus request_state(const HomeAssistantConfig *config, const char *entity) {
    memset(&http, 0, sizeof(http));
    http.host = config->host;
    http.port = config->port;
    http.status = HTTP_PENDING;

    int size = snprintf(http.request,
                        sizeof(http.request),
                        "GET /api/states/%s HTTP/1.1\r\n"
                        "Host: %s:%u\r\n"
                        "Authorization: Bearer %s\r\n"
                        "Accept: application/json\r\n"
                        "Connection: close\r\n\r\n",
                        entity,
                        config->host,
                        config->port,
                        config->token);
    if (size < 0 || (size_t)size >= sizeof(http.request)) {
        return HTTP_RESPONSE_ERROR;
    }

    ip_addr_t address;
    err_t result = dns_gethostbyname(config->host, &address, resolved, &http);
    if (result == ERR_OK) {
        connect_to_host(&http, &address);
    } else if (result != ERR_INPROGRESS) {
        return HTTP_NETWORK_ERROR;
    }

    absolute_time_t deadline = make_timeout_time_ms(HTTP_TIMEOUT_MS);
    while (!http.done && !time_reached(deadline)) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
    if (!http.done) {
        if (http.pcb != NULL) {
            tcp_arg(http.pcb, NULL);
            tcp_abort(http.pcb);
            http.pcb = NULL;
        }
        return HTTP_NETWORK_ERROR;
    }
    return http.status;
}

static bool parse_state(double *value) {
    if (value == NULL ||
        (strncmp(http.response, "HTTP/1.1 200", 12) != 0 &&
         strncmp(http.response, "HTTP/1.0 200", 12) != 0)) {
        return false;
    }

    char *field = strstr(http.response, "\"state\"");
    if (field == NULL) {
        return false;
    }
    field = strchr(field, ':');
    if (field == NULL) {
        return false;
    }
    field++;
    while (*field == ' ' || *field == '\t') {
        field++;
    }
    if (*field != '\"') {
        return false;
    }

    char *end = NULL;
    *value = strtod(field + 1, &end);
    return end != field + 1 && end != NULL && *end == '\"';
}

static HomeAssistantStatus read_value(const HomeAssistantConfig *config,
                                      const char *entity,
                                      double *value) {
    if (entity == NULL || entity[0] == '\0') {
        return HOME_ASSISTANT_NOT_CONFIGURED;
    }

    HttpStatus status = request_state(config, entity);
    if (status == HTTP_NETWORK_ERROR) {
        return HOME_ASSISTANT_NETWORK_ERROR;
    }
    if (status != HTTP_OK || !parse_state(value)) {
        return HOME_ASSISTANT_RESPONSE_ERROR;
    }
    return HOME_ASSISTANT_OK;
}

static int round_value(double value) {
    return (int)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

static int month_number(const char *month) {
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int index = 0; index < 12; ++index) {
        if (strcmp(month, names[index]) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static int weekday(int year, int month, int day) {
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year--;
    }
    return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

static int month_days(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) {
        return 29;
    }
    return days[month - 1];
}

static bool daylight_saving(int year, int month, int day, int hour) {
    if (month > 3 && month < 10) {
        return true;
    }
    if (month < 3 || month > 10) {
        return false;
    }

    int last_day = month_days(year, month);
    int last_sunday = last_day - weekday(year, month, last_day);
    if (month == 3) {
        return day > last_sunday || (day == last_sunday && hour >= 1);
    }
    return day < last_sunday || (day == last_sunday && hour < 1);
}

static bool parse_time(int *hour, int *minute, int *second) {
    if (hour == NULL || minute == NULL || second == NULL) {
        return false;
    }

    char *header = strstr(http.response, "\r\nDate:");
    if (header == NULL) {
        return false;
    }

    const char *date = header + 2;
    static const int digit_positions[] = {11, 12, 18, 19, 20, 21, 23, 24, 26, 27, 29, 30};
    if (strncmp(date, "Date: ", 6) != 0 || date[9] != ',' || date[13] != ' ' ||
        date[17] != ' ' || date[22] != ' ' || date[25] != ':' ||
        date[28] != ':' || strncmp(date + 31, " GMT", 4) != 0) {
        return false;
    }
    for (size_t index = 0; index < sizeof(digit_positions) / sizeof(digit_positions[0]); ++index) {
        char value = date[digit_positions[index]];
        if (value < '0' || value > '9') {
            return false;
        }
    }

    char month_name[4] = {date[14], date[15], date[16], '\0'};
    int day = (date[11] - '0') * 10 + date[12] - '0';
    int year = (date[18] - '0') * 1000 + (date[19] - '0') * 100 +
               (date[20] - '0') * 10 + date[21] - '0';
    int utc_hour = (date[23] - '0') * 10 + date[24] - '0';
    *minute = (date[26] - '0') * 10 + date[27] - '0';
    *second = (date[29] - '0') * 10 + date[30] - '0';

    int month = month_number(month_name);
    if (month == 0 || day < 1 || day > month_days(year, month) ||
        utc_hour < 0 || utc_hour > 23 || *minute < 0 || *minute > 59 ||
        *second < 0 || *second > 60) {
        return false;
    }

    int offset = daylight_saving(year, month, day, utc_hour) ? 2 : 1;
    *hour = (utc_hour + offset) % 24;
    return true;
}

HomeAssistantStatus home_assistant_read(const HomeAssistantConfig *config,
                                        HomeAssistantReading *reading) {
    if (config == NULL || reading == NULL || config->host == NULL || config->token == NULL ||
        config->host[0] == '\0' || config->token[0] == '\0') {
        return HOME_ASSISTANT_NOT_CONFIGURED;
    }

    double temperature;
    double humidity;
    double co2;
    double pm25;
    double external_temperature;
    HomeAssistantStatus status = read_value(config, config->temperature, &temperature);
    if (status == HOME_ASSISTANT_OK) {
        status = read_value(config, config->humidity, &humidity);
    }
    if (status == HOME_ASSISTANT_OK) {
        status = read_value(config, config->co2, &co2);
    }
    if (status == HOME_ASSISTANT_OK) {
        status = read_value(config, config->pm25, &pm25);
    }
    if (status == HOME_ASSISTANT_OK) {
        status = read_value(config, config->external_temperature, &external_temperature);
    }
    if (status != HOME_ASSISTANT_OK) {
        return status;
    }

    reading->temperature_tenths = round_value(temperature * 10.0);
    reading->humidity = round_value(humidity);
    reading->co2 = round_value(co2);
    reading->pm25 = round_value(pm25);
    reading->external_temperature_tenths = round_value(external_temperature * 10.0);
    if (!parse_time(&reading->hour, &reading->minute, &reading->second)) {
        return HOME_ASSISTANT_RESPONSE_ERROR;
    }
    return HOME_ASSISTANT_OK;
}
