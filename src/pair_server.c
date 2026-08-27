#include "pair_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink_protocol.h"
#include "log.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

enum {
    PAIR_PORT = 8088,
    REQUEST_MAX = 1400
};

typedef struct {
    PairServerConfig config;
    struct tcp_pcb *listener;
    struct tcp_pcb *client;
    char request[REQUEST_MAX + 1];
    size_t request_length;
    bool complete;
} PairServer;

static bool mdns_initialized;

static void close_client(PairServer *server) {
    if (server->client == NULL) {
        return;
    }
    tcp_arg(server->client, NULL);
    tcp_recv(server->client, NULL);
    tcp_err(server->client, NULL);
    if (tcp_close(server->client) != ERR_OK) {
        tcp_abort(server->client);
    }
    server->client = NULL;
}

static void respond(PairServer *server, const char *status, const char *type, const char *body) {
    if (server->client == NULL) {
        return;
    }
    char response[512];
    size_t body_length = body == NULL ? 0 : strlen(body);
    int length = snprintf(response,
                          sizeof(response),
                          "HTTP/1.1 %s\r\nContent-Length: %u\r\nContent-Type: %s\r\nConnection: close\r\n\r\n%s",
                          status,
                          (unsigned int)body_length,
                          type,
                          body == NULL ? "" : body);
    if (length <= 0 || (size_t)length >= sizeof(response) ||
        tcp_write(server->client, response, (u16_t)length, TCP_WRITE_FLAG_COPY) != ERR_OK) {
        APP_LOG("PAIR_HTTP response=write_error\n");
        close_client(server);
        return;
    }
    APP_LOG("PAIR_HTTP response=%s\n", status);
    tcp_output(server->client);
    close_client(server);
}

static size_t content_length(const char *headers) {
    const char *field = strstr(headers, "\r\nContent-Length:");
    if (field == NULL) {
        return 0;
    }
    field += strlen("\r\nContent-Length:");
    while (*field == ' ') {
        field++;
    }
    char *end = NULL;
    unsigned long value = strtoul(field, &end, 10);
    return end == field || value > INK_PAYLOAD_MAX ? 0 : (size_t)value;
}

static void handle_request(PairServer *server) {
    char *body = strstr(server->request, "\r\n\r\n");
    if (body == NULL) {
        return;
    }
    size_t headers_length = (size_t)(body - server->request) + 4;
    size_t expected = content_length(server->request);
    if (server->request_length < headers_length + expected) {
        return;
    }
    body += 4;

    if (strncmp(server->request, "GET /v1/info ", 13) == 0) {
        APP_LOG("PAIR_HTTP request=info\n");
        char json[256];
        snprintf(json,
                 sizeof(json),
                 "{\"id\":\"%s\",\"name\":\"Ink Display %s\",\"version\":1,\"paired\":false}",
                 server->config.device_id,
                 server->config.device_id);
        respond(server, "200 OK", "application/json", json);
        return;
    }
    if (strncmp(server->request, "POST /v1/pair ", 14) != 0 || expected == 0) {
        APP_LOG("PAIR_HTTP request=unknown\n");
        respond(server, "404 Not Found", "text/plain", "Not found");
        return;
    }

    char payload[INK_PAYLOAD_MAX];
    memcpy(payload, body, expected);
    InkPairRequest request;
    if (!ink_protocol_pair_parse(payload, expected, &request) ||
        request.code != server->config.code) {
        APP_LOG("PAIR_HTTP request=pair result=invalid_code\n");
        respond(server, "403 Forbidden", "text/plain", "Invalid code");
        return;
    }
    if (!device_store_pair(server->config.provisioning_id,
                           &request,
                           server->config.settings)) {
        APP_LOG("PAIR_HTTP request=pair result=storage_error\n");
        respond(server, "500 Internal Server Error", "text/plain", "Storage error");
        return;
    }
    server->complete = true;
    APP_LOG("PAIR_HTTP request=pair result=ok\n");
    respond(server, "204 No Content", "text/plain", NULL);
}

static err_t receive(void *context, struct tcp_pcb *pcb, struct pbuf *data, err_t error) {
    PairServer *server = context;
    if (data == NULL || error != ERR_OK) {
        close_client(server);
        return ERR_OK;
    }
    size_t total = data->tot_len;
    size_t available = REQUEST_MAX - server->request_length;
    size_t copied = total < available ? total : available;
    pbuf_copy_partial(data, server->request + server->request_length, (u16_t)copied, 0);
    server->request_length += copied;
    server->request[server->request_length] = '\0';
    tcp_recved(pcb, (u16_t)total);
    pbuf_free(data);
    if (copied == 0 || copied < total) {
        respond(server, "413 Content Too Large", "text/plain", "Request too large");
        return ERR_OK;
    }
    handle_request(server);
    return ERR_OK;
}

static void connection_error(void *context, err_t error) {
    PairServer *server = context;
    (void)error;
    server->client = NULL;
}

static err_t accept_client(void *context, struct tcp_pcb *client, err_t error) {
    PairServer *server = context;
    if (error != ERR_OK || server->client != NULL) {
        tcp_abort(client);
        return ERR_ABRT;
    }
    server->client = client;
    APP_LOG("PAIR_HTTP connection=accepted\n");
    server->request_length = 0;
    tcp_arg(client, server);
    tcp_recv(client, receive);
    tcp_err(client, connection_error);
    return ERR_OK;
}

static void service_text(struct mdns_service *service, void *context) {
    PairServer *server = context;
    char id[40];
    snprintf(id, sizeof(id), "id=%s", server->config.device_id);
    mdns_resp_add_service_txtitem(service, id, (u8_t)strlen(id));
    mdns_resp_add_service_txtitem(service, "version=1", 9);
    mdns_resp_add_service_txtitem(service, "paired=0", 8);
}

static bool publish(PairServer *server) {
    char hostname[40];
    snprintf(hostname, sizeof(hostname), "ha-ink-%s", server->config.device_id);
    if (!mdns_initialized) {
        mdns_resp_init();
        mdns_initialized = true;
    }
    if (mdns_resp_add_netif(netif_default, hostname) != ERR_OK) {
        return false;
    }
    if (mdns_resp_add_service(netif_default,
                              hostname,
                              "_ha-ink",
                              DNSSD_PROTO_TCP,
                              PAIR_PORT,
                              service_text,
                              server) < 0) {
        mdns_resp_remove_netif(netif_default);
        return false;
    }
    mdns_resp_announce(netif_default);
    APP_LOG("PAIR_MDNS state=published port=%d\n", PAIR_PORT);
    return true;
}

bool pair_server_run(const PairServerConfig *config) {
    if (config == NULL || config->device_id == NULL || config->settings == NULL) {
        return false;
    }
    PairServer server;
    memset(&server, 0, sizeof(server));
    server.config = *config;
    server.listener = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (server.listener == NULL ||
        tcp_bind(server.listener, IP_ANY_TYPE, PAIR_PORT) != ERR_OK) {
        if (server.listener != NULL) {
            tcp_close(server.listener);
        }
        return false;
    }
    struct tcp_pcb *listener = tcp_listen_with_backlog(server.listener, 1);
    if (listener == NULL) {
        tcp_close(server.listener);
        return false;
    }
    server.listener = listener;
    tcp_arg(listener, &server);
    tcp_accept(listener, accept_client);
    bool mdns_published = publish(&server);
    if (!mdns_published) {
        APP_LOG("PAIR_MDNS state=failed\n");
        tcp_close(listener);
        return false;
    }

    absolute_time_t link_check = make_timeout_time_ms(1000);
    while (!server.complete) {
        cyw43_arch_poll();
        if (time_reached(link_check)) {
            int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            if (link == CYW43_LINK_DOWN ||
                link == CYW43_LINK_FAIL ||
                link == CYW43_LINK_NONET ||
                link == CYW43_LINK_BADAUTH) {
                APP_LOG("PAIR_WIFI state=lost status=%d\n", link);
                break;
            }
            link_check = make_timeout_time_ms(1000);
        }
        sleep_ms(2);
    }
    if (server.complete) {
        absolute_time_t grace = make_timeout_time_ms(250);
        while (!time_reached(grace)) {
            cyw43_arch_poll();
            sleep_ms(2);
        }
    }
    close_client(&server);
    tcp_arg(listener, NULL);
    tcp_accept(listener, NULL);
    tcp_close(listener);
    if (mdns_published) {
        mdns_resp_remove_netif(netif_default);
    }
    return server.complete;
}
