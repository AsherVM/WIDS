// Expose Espressif SDK functionality
extern "C" {
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int  wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int  wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

#include "./structures.h"

#define MAX_CLIENTS_TRACKED 100

int nothing_new = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

String formatMac1(uint8_t mac[ETH_MAC_LEN]) {
  String hi = "";
  for (int i = 0; i < ETH_MAC_LEN; i++) {
    if (mac[i] < 16) hi = hi + "0" + String(mac[i], HEX);
    else hi = hi + String(mac[i], HEX);
    if (i < 5) hi = hi + ":";
  }
  return hi;
}

int register_client(clientinfo &ci) {
  // if (ci.channel == -2) { // If probe request is from randomised MAC:
  // } // Could use "RTS" frames to still find source https://www.theregister.co.uk/2017/03/10/mac_address_randomization/?page=2
  if (ci.channel == 0) 
    return 0;

  // If already have ci.station in known_clients don't reregister (set known = 1 and update RSSI)
  int known = 0;
  for (int u = 0; u < clients_known_count; u++) {
    if (! memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN)) {
      clients_known[u].rssi = ci.rssi;
      return 0;
    }
  }
  Serial.println(clients_known_count);
  
  // If new then add client to clients_known
  if (!known) {
    Serial.println(formatMac1(ci.station));
    Serial.println();
    Serial.println();

    memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));
    clients_known_count++;
    nothing_new = 0;
  }

  // IF MORE DEVICES THAN MAX_DEVICES RESET LIST
  if ((unsigned int) clients_known_count >= sizeof (clients_known) / sizeof (clients_known[0]) ) { 
    Serial.printf("exceeded max clients_known\n");
    clients_known_count = 0;
  }

  return clients_known_count;
}

void promisc_cb(uint8_t *buf, uint16_t len) {
  if (len == 128) { // Is a "management" frame?
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    
    // If first byte indicates client "probe request" frame
    if (sniffer->buf[0] == 0x40) { // Then parse probe and register client
      struct clientinfo ci = parse_probe(sniffer->buf, 36, sniffer->rx_ctrl.rssi);
      register_client(ci);
    }
  } 
  
  if (len > 128) { // Larger than 128 means it's a "data" frame
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) { //If data or QOS frame?
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      register_client(ci);
      // if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {} // If sending MAC doesn't match AP MAC (if frame.src != AP.MAC)
    }
  }
}


void promisc_cb_old(uint8_t *buf, uint16_t len) {
  // int i = 0;
  // uint16_t seq_n_new = 0;
  if (len == 12) { // Is a "control" frame
    //struct RxControl *sniffer = (struct RxControl*) buf; 
    // sent sniffer->buf[4-9] = 6 Byte MAC 
  } else if (len == 128) { // Is a "management" frame?
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    
    if (sniffer->buf[0] == 0x40) { // If first byte indicates client "probe request" frame
      struct clientinfo ci = parse_probe(sniffer->buf, 36, sniffer->rx_ctrl.rssi);

      if (register_client(ci) == 0)
        nothing_new = 0;
    }
  } else { 
    // Larger than 128 means it's a "data" frame
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) { //If data or QOS frame?
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      if ((memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) && (register_client(ci) == 0))
        nothing_new = 0;
    }
  }
}
