#include "ink_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "mbedtls/md.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

enum {
    HTTP_REQUEST_SIZE = 512,
    HTTP_RESPONSE_SIZE = 1400,
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
    uint16_t port;
    char request[HTTP_REQUEST_SIZE];
    char response[HTTP_RESPONSE_SIZE];
    size_t response_length;
    HttpStatus status;
    bool done;
} HttpRequest;

static HttpRequest http;

static bool request_signature(const DeviceSettings *settings,
                              uint64_t revision,
                              char output[INK_SECRET_HEX_SIZE + 1]) {
    char message[40];
    int length = snprintf(message,
                          sizeof(message),
                          "POLL\n%llu\n",
                          (unsigned long long)revision);
    uint8_t digest[INK_SECRET_SIZE];
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (length <= 0 || (size_t)length >= sizeof(message) || info == NULL ||
        mbedtls_md_hmac(info,
                        settings->secret,
                        sizeof(settings->secret),
                        (const uint8_t *)message,
                        (size_t)length,
                        digest) != 0) {
        return false;
    }
    ink_protocol_hex_encode(digest, sizeof(digest), output);
    memset(digest, 0, sizeof(digest));
    return true;
}

static void finish(HttpStatus status) {
    http.status = status;
    http.done = true;
}

static void close_connection(void) {
    if (http.pcb == NULL) {
        return;
    }
    tcp_arg(http.pcb, NULL);
    tcp_recv(http.pcb, NULL);
    tcp_err(http.pcb, NULL);
    if (tcp_close(http.pcb) != ERR_OK) {
        tcp_abort(http.pcb);
    }
    http.pcb = NULL;
}

static err_t receive(void *context, struct tcp_pcb *pcb, struct pbuf *data, err_t error) {
    (void)context;
    if (data == NULL) {
        http.pcb = pcb;
        close_connection();
        finish(error == ERR_OK ? HTTP_OK : HTTP_NETWORK_ERROR);
        return ERR_OK;
    }
    if (error != ERR_OK) {
        pbuf_free(data);
        finish(HTTP_NETWORK_ERROR);
        return error;
    }
    size_t total = data->tot_len;
    size_t available = sizeof(http.response) - http.response_length - 1;
    size_t copied = total < available ? total : available;
    if (copied > 0) {
        pbuf_copy_partial(data, http.response + http.response_length, (u16_t)copied, 0);
        http.response_length += copied;
        http.response[http.response_length] = '\0';
    }
    tcp_recved(pcb, data->tot_len);
    pbuf_free(data);
    if (copied < total) {
        close_connection();
        finish(HTTP_RESPONSE_ERROR);
    }
    return ERR_OK;
}

static void connection_error(void *context, err_t error) {
    (void)context;
    http.pcb = NULL;
    finish(error == ERR_OK ? HTTP_OK : HTTP_NETWORK_ERROR);
}

static err_t connected(void *context, struct tcp_pcb *pcb, err_t error) {
    (void)context;
    if (error != ERR_OK) {
        finish(HTTP_NETWORK_ERROR);
        return error;
    }
    http.pcb = pcb;
    err_t result = tcp_write(pcb,
                             http.request,
                             (u16_t)strlen(http.request),
                             TCP_WRITE_FLAG_COPY);
    if (result == ERR_OK) {
        result = tcp_output(pcb);
    }
    if (result != ERR_OK) {
        close_connection();
        finish(HTTP_NETWORK_ERROR);
    }
    return result;
}

static void connect_address(const ip_addr_t *address) {
    http.pcb = tcp_new_ip_type(IP_GET_TYPE(address));
    if (http.pcb == NULL) {
        finish(HTTP_NETWORK_ERROR);
        return;
    }
    tcp_arg(http.pcb, NULL);
    tcp_recv(http.pcb, receive);
    tcp_err(http.pcb, connection_error);
    if (tcp_connect(http.pcb, address, http.port, connected) != ERR_OK) {
        close_connection();
        finish(HTTP_NETWORK_ERROR);
    }
}

static void resolved(const char *name, const ip_addr_t *address, void *context) {
    (void)name;
    (void)context;
    if (address == NULL) {
        finish(HTTP_NETWORK_ERROR);
        return;
    }
    connect_address(address);
}

static HttpStatus request(const DeviceSettings *settings, uint64_t revision) {
    memset(&http, 0, sizeof(http));
    http.port = settings->port;
    http.status = HTTP_PENDING;
    char authorization[INK_SECRET_HEX_SIZE + 1];
    if (!request_signature(settings, revision, authorization)) {
        return HTTP_RESPONSE_ERROR;
    }
    int size = snprintf(http.request,
                        sizeof(http.request),
                        "GET %s?revision=%llu HTTP/1.1\r\n"
                        "Host: %s:%u\r\n"
                        "X-Ink-Authorization: %s\r\n"
                        "Accept: text/plain\r\n"
                        "Connection: close\r\n\r\n",
                        settings->path,
                        (unsigned long long)revision,
                        settings->host,
                        settings->port,
                        authorization);
    memset(authorization, 0, sizeof(authorization));
    if (size <= 0 || (size_t)size >= sizeof(http.request)) {
        return HTTP_RESPONSE_ERROR;
    }

    ip_addr_t address;
    err_t result = dns_gethostbyname(settings->host, &address, resolved, NULL);
    if (result == ERR_OK) {
        connect_address(&address);
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

static bool constant_equal(const uint8_t *first, const uint8_t *second, size_t size) {
    uint8_t difference = 0;
    for (size_t index = 0; index < size; ++index) {
        difference |= first[index] ^ second[index];
    }
    return difference == 0;
}

static bool signature_valid(const DeviceSettings *settings, const char *body, size_t body_length) {
    const char *header = strstr(http.response, "\r\nX-Ink-Signature: ");
    if (header == NULL) {
        return false;
    }
    header += strlen("\r\nX-Ink-Signature: ");
    if ((size_t)(http.response + http.response_length - header) < INK_SECRET_HEX_SIZE) {
        return false;
    }
    char encoded[INK_SECRET_HEX_SIZE + 1];
    memcpy(encoded, header, INK_SECRET_HEX_SIZE);
    encoded[INK_SECRET_HEX_SIZE] = '\0';
    uint8_t received[INK_SECRET_SIZE];
    uint8_t expected[INK_SECRET_SIZE];
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!ink_protocol_hex_decode(encoded, received, sizeof(received)) || info == NULL ||
        mbedtls_md_hmac(info,
                        settings->secret,
                        sizeof(settings->secret),
                        (const uint8_t *)body,
                        body_length,
                        expected) != 0) {
        return false;
    }
    return constant_equal(received, expected, sizeof(expected));
}

InkClientStatus ink_client_poll(const DeviceSettings *settings,
                                uint64_t revision,
                                InkFrame *frame) {
    if (settings == NULL || !settings->paired || frame == NULL) {
        return INK_CLIENT_RESPONSE_ERROR;
    }
    HttpStatus status = request(settings, revision);
    if (status == HTTP_NETWORK_ERROR) {
        return INK_CLIENT_NETWORK_ERROR;
    }
    if (status != HTTP_OK) {
        return INK_CLIENT_RESPONSE_ERROR;
    }
    if (strncmp(http.response, "HTTP/1.1 204", 12) == 0 ||
        strncmp(http.response, "HTTP/1.0 204", 12) == 0) {
        return INK_CLIENT_UNCHANGED;
    }
    if (strncmp(http.response, "HTTP/1.1 401", 12) == 0 ||
        strncmp(http.response, "HTTP/1.0 401", 12) == 0) {
        return INK_CLIENT_AUTH_ERROR;
    }
    if (strncmp(http.response, "HTTP/1.1 200", 12) != 0 &&
        strncmp(http.response, "HTTP/1.0 200", 12) != 0) {
        return INK_CLIENT_RESPONSE_ERROR;
    }
    char *body = strstr(http.response, "\r\n\r\n");
    if (body == NULL) {
        return INK_CLIENT_RESPONSE_ERROR;
    }
    body += 4;
    size_t body_length = http.response_length - (size_t)(body - http.response);
    if (!signature_valid(settings, body, body_length) ||
        !ink_protocol_frame_parse(body, body_length, frame) ||
        frame->revision <= revision) {
        return INK_CLIENT_AUTH_ERROR;
    }
    return INK_CLIENT_UPDATED;
}
