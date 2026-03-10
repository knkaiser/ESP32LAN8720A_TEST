#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

static const char *TAG = "eth_dns";

#define ETH_MDC_GPIO     23
#define ETH_MDIO_GPIO    18
#define ETH_PHY_ADDR     0
#define ETH_PHY_RST_GPIO 5

/* ---- LAN8720A MII register addresses ---- */
#define REG_BMCR    0x00    /* Basic Mode Control */
#define REG_BMSR    0x01    /* Basic Mode Status */
#define REG_PHYID1  0x02    /* PHY Identifier 1 */
#define REG_PHYID2  0x03    /* PHY Identifier 2 */
#define REG_ANAR    0x04    /* Auto-Negotiation Advertisement */
#define REG_ANLPAR  0x05    /* Auto-Negotiation Link Partner Ability */
#define REG_ANER    0x06    /* Auto-Negotiation Expansion */
#define REG_MCSR    0x11    /* Mode Control/Status */
#define REG_SMR     0x12    /* Special Modes */
#define REG_SECR    0x1A    /* Symbol Error Counter */
#define REG_CSIR    0x1B    /* Control/Status Indications */
#define REG_ISR     0x1D    /* Interrupt Source */
#define REG_IMR     0x1E    /* Interrupt Mask */
#define REG_PSCSR   0x1F    /* PHY Special Control/Status */

/* BMCR bits */
#define BMCR_RESET          (1 << 15)
#define BMCR_LOOPBACK       (1 << 14)
#define BMCR_SPEED_100      (1 << 13)
#define BMCR_AN_ENABLE      (1 << 12)
#define BMCR_POWER_DOWN     (1 << 11)
#define BMCR_ISOLATE        (1 << 10)
#define BMCR_AN_RESTART     (1 << 9)
#define BMCR_FULL_DUPLEX    (1 << 8)

/* BMSR bits */
#define BMSR_100TX_FD       (1 << 14)
#define BMSR_100TX_HD       (1 << 13)
#define BMSR_10T_FD         (1 << 12)
#define BMSR_10T_HD         (1 << 11)
#define BMSR_AN_COMPLETE    (1 << 5)
#define BMSR_REMOTE_FAULT   (1 << 4)
#define BMSR_AN_ABILITY     (1 << 3)
#define BMSR_LINK_STATUS    (1 << 2)
#define BMSR_JABBER_DETECT  (1 << 1)
#define BMSR_EXT_CAPABLE    (1 << 0)

/* ANAR / ANLPAR bits */
#define AN_100TX_FD         (1 << 8)
#define AN_100TX_HD         (1 << 7)
#define AN_10T_FD           (1 << 6)
#define AN_10T_HD           (1 << 5)
#define AN_SELECTOR_IEEE    0x0001

/* MCSR bits */
#define MCSR_EDPWRDOWN      (1 << 13)
#define MCSR_FAR_LOOPBACK   (1 << 9)
#define MCSR_ALT_INT        (1 << 6)
#define MCSR_ENERGYON       (1 << 1)

/* CSIR bits */
#define CSIR_AMDIX_DISABLE  (1 << 15)
#define CSIR_AMDIX_CROSS    (1 << 13)
#define CSIR_SQE_DISABLE    (1 << 11)
#define CSIR_POLARITY_10T   (1 << 4)

/* PSCSR bits */
#define PSCSR_AN_DONE       (1 << 12)
#define PSCSR_SPEED_MASK    (0x7 << 2)
#define PSCSR_SPEED_SHIFT   2

static EventGroupHandle_t eth_event_group;
#define ETH_GOT_IP_BIT BIT0

static esp_eth_handle_t s_eth_handle = NULL;

static uint32_t phy_read_reg(uint8_t reg)
{
    uint32_t val = 0;
    esp_eth_phy_reg_rw_data_t rd = { .reg_addr = reg, .reg_value_p = &val };
    esp_eth_ioctl(s_eth_handle, ETH_CMD_READ_PHY_REG, &rd);
    return val;
}

static void phy_write_reg(uint8_t reg, uint32_t val)
{
    esp_eth_phy_reg_rw_data_t wd = { .reg_addr = reg, .reg_value_p = &val };
    esp_eth_ioctl(s_eth_handle, ETH_CMD_WRITE_PHY_REG, &wd);
}

static const char *speed_str(uint32_t pscsr)
{
    switch ((pscsr & PSCSR_SPEED_MASK) >> PSCSR_SPEED_SHIFT) {
    case 0x01: return "10Base-T Half-Duplex";
    case 0x05: return "10Base-T Full-Duplex";
    case 0x02: return "100Base-TX Half-Duplex";
    case 0x06: return "100Base-TX Full-Duplex";
    default:   return "not resolved";
    }
}

static void an_caps_str(uint32_t reg, char *buf, size_t len)
{
    snprintf(buf, len, "%s%s%s%s",
             (reg & AN_100TX_FD) ? "100FD " : "",
             (reg & AN_100TX_HD) ? "100HD " : "",
             (reg & AN_10T_FD)  ? "10FD "  : "",
             (reg & AN_10T_HD)  ? "10HD "  : "");
}

static void dump_phy_id(void)
{
    uint32_t id1 = phy_read_reg(REG_PHYID1);
    uint32_t id2 = phy_read_reg(REG_PHYID2);
    uint32_t oui = (id1 << 6) | ((id2 >> 10) & 0x3F);
    uint32_t model = (id2 >> 4) & 0x3F;
    uint32_t rev = id2 & 0x0F;
    ESP_LOGI(TAG, "PHY ID: 0x%04x:0x%04x  OUI=0x%06x  Model=0x%02x  Rev=%d",
             (unsigned)id1, (unsigned)id2, (unsigned)oui, (unsigned)model, (unsigned)rev);
}

static void dump_mcsr(const char *label)
{
    uint32_t mcsr = phy_read_reg(REG_MCSR);
    ESP_LOGI(TAG, "%s MCSR(0x11)=0x%04x  ENERGYON=%d  EDPWRDOWN=%d  FarLoopback=%d",
             label, (unsigned)mcsr,
             !!(mcsr & MCSR_ENERGYON),
             !!(mcsr & MCSR_EDPWRDOWN),
             !!(mcsr & MCSR_FAR_LOOPBACK));
}

static void dump_bmcr(const char *label)
{
    uint32_t bmcr = phy_read_reg(REG_BMCR);
    ESP_LOGI(TAG, "%s BMCR(0x00)=0x%04x  Speed100=%d  AN=%d  Duplex=%d  PwrDown=%d  Isolate=%d  Loopback=%d",
             label, (unsigned)bmcr,
             !!(bmcr & BMCR_SPEED_100),
             !!(bmcr & BMCR_AN_ENABLE),
             !!(bmcr & BMCR_FULL_DUPLEX),
             !!(bmcr & BMCR_POWER_DOWN),
             !!(bmcr & BMCR_ISOLATE),
             !!(bmcr & BMCR_LOOPBACK));
}

static void dump_bmsr(const char *label)
{
    phy_read_reg(REG_BMSR);  /* first read clears latched bits */
    uint32_t bmsr = phy_read_reg(REG_BMSR);
    ESP_LOGI(TAG, "%s BMSR(0x01)=0x%04x  Link=%d  AN_Complete=%d  RemoteFault=%d  Jabber=%d",
             label, (unsigned)bmsr,
             !!(bmsr & BMSR_LINK_STATUS),
             !!(bmsr & BMSR_AN_COMPLETE),
             !!(bmsr & BMSR_REMOTE_FAULT),
             !!(bmsr & BMSR_JABBER_DETECT));
    ESP_LOGI(TAG, "  Capabilities: %s%s%s%s",
             (bmsr & BMSR_100TX_FD) ? "100TX-FD " : "",
             (bmsr & BMSR_100TX_HD) ? "100TX-HD " : "",
             (bmsr & BMSR_10T_FD)   ? "10T-FD "   : "",
             (bmsr & BMSR_10T_HD)   ? "10T-HD "   : "");
}

static void dump_an(const char *label)
{
    uint32_t anar   = phy_read_reg(REG_ANAR);
    uint32_t anlpar = phy_read_reg(REG_ANLPAR);
    uint32_t aner   = phy_read_reg(REG_ANER);
    char our[64], lp[64];
    an_caps_str(anar, our, sizeof(our));
    an_caps_str(anlpar, lp, sizeof(lp));
    ESP_LOGI(TAG, "%s ANAR(0x04)=0x%04x   Our advertised: %s", label, (unsigned)anar, our);
    ESP_LOGI(TAG, "%s ANLPAR(0x05)=0x%04x Link partner:   %s", label, (unsigned)anlpar, lp);
    ESP_LOGI(TAG, "%s ANER(0x06)=0x%04x   LP_AN_Able=%d  PageRx=%d  ParallelFault=%d",
             label, (unsigned)aner, !!(aner & 1), !!(aner & 2), !!(aner & 4));
}

static void dump_speed(const char *label)
{
    uint32_t pscsr = phy_read_reg(REG_PSCSR);
    ESP_LOGI(TAG, "%s PSCSR(0x1F)=0x%04x  AN_Done=%d  Speed: %s",
             label, (unsigned)pscsr, !!(pscsr & PSCSR_AN_DONE), speed_str(pscsr));
}

static void dump_csir(const char *label)
{
    uint32_t csir = phy_read_reg(REG_CSIR);
    ESP_LOGI(TAG, "%s CSIR(0x1B)=0x%04x   AutoMDIX=%s  Crossover=%s  Polarity10T=%s  SQE=%s",
             label, (unsigned)csir,
             (csir & CSIR_AMDIX_DISABLE) ? "OFF" : "ON",
             (csir & CSIR_AMDIX_CROSS)   ? "MDIX" : "MDI",
             (csir & CSIR_POLARITY_10T)   ? "reversed" : "normal",
             (csir & CSIR_SQE_DISABLE)    ? "disabled" : "enabled");
}

static void dump_smr(const char *label)
{
    uint32_t smr = phy_read_reg(REG_SMR);
    uint32_t mode = (smr >> 5) & 0x7;
    uint32_t addr = smr & 0x1F;
    const char *modes[] = {
        "10BT-HD (no AN)", "10BT-FD (no AN)", "100TX-HD (no AN)", "100TX-FD (no AN)",
        "100TX-HD (AN)", "Rsvd", "Rsvd", "All capable (AN)"
    };
    ESP_LOGI(TAG, "%s SMR(0x12)=0x%04x    PHY_ADDR=%d  Mode=%d (%s)",
             label, (unsigned)smr, (unsigned)addr, (unsigned)mode,
             mode < 8 ? modes[mode] : "unknown");
}

static void dump_interrupts(const char *label)
{
    uint32_t isr = phy_read_reg(REG_ISR);
    uint32_t imr = phy_read_reg(REG_IMR);
    ESP_LOGI(TAG, "%s ISR(0x1D)=0x%04x    EnergyOn=%d  AN_Complete=%d  LinkDown=%d  RemoteFault=%d  AN_LP_Ack=%d",
             label, (unsigned)isr,
             !!(isr & (1 << 7)), !!(isr & (1 << 6)),
             !!(isr & (1 << 4)), !!(isr & (1 << 5)),
             !!(isr & (1 << 3)));
    ESP_LOGI(TAG, "%s IMR(0x1E)=0x%04x", label, (unsigned)imr);
}

static void dump_all(const char *label)
{
    ESP_LOGI(TAG, "---- %s ----", label);
    dump_bmcr(label);
    dump_bmsr(label);
    dump_mcsr(label);
    dump_an(label);
    dump_speed(label);
    dump_csir(label);
    dump_smr(label);
    dump_interrupts(label);
    uint32_t secr = phy_read_reg(REG_SECR);
    ESP_LOGI(TAG, "%s SECR(0x1A)=0x%04x   SymbolErrors=%d", label, (unsigned)secr, (unsigned)secr);
    ESP_LOGI(TAG, "---- end %s ----", label);
}

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, ">>> LINK UP  MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
        dump_all("LinkUp");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, ">>> LINK DOWN");
        dump_mcsr("LinkDown");
        dump_bmsr("LinkDown");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, ">>> ETHERNET STARTED");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGW(TAG, ">>> ETHERNET STOPPED");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, ">>> GOT IP ADDRESS");
    ESP_LOGI(TAG, "  IP:      " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "  Netmask: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "  Gateway: " IPSTR, IP2STR(&ip_info->gw));

    const ip_addr_t *dns0 = dns_getserver(0);
    const ip_addr_t *dns1 = dns_getserver(1);
    if (dns0) ESP_LOGI(TAG, "  DNS0:    " IPSTR, IP2STR(&dns0->u_addr.ip4));
    if (dns1) ESP_LOGI(TAG, "  DNS1:    " IPSTR, IP2STR(&dns1->u_addr.ip4));

    xEventGroupSetBits(eth_event_group, ETH_GOT_IP_BIT);
}

static void dns_lookup(const char *hostname)
{
    ESP_LOGI(TAG, "Resolving \"%s\" ...", hostname);

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *result = NULL;

    int err = getaddrinfo(hostname, NULL, &hints, &result);
    if (err != 0 || result == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for %s (err=%d)", hostname, err);
        if (result) freeaddrinfo(result);
        return;
    }

    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntoa_r(addr->sin_addr, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "  %s -> %s", hostname, ip_str);
    }

    freeaddrinfo(result);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " HamGeek01 Plus Ethernet DNS Test");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    eth_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ---- EMAC + LAN8720A PHY ---- */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num  = ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = ETH_MDIO_GPIO;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr       = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));

    /* ---- PHY identification & initial state ---- */
    dump_phy_id();
    dump_all("Post-Init");

    /* Disable EDPD before starting, to prevent the ENERGYON bug */
    uint32_t mcsr = phy_read_reg(REG_MCSR);
    if (mcsr & MCSR_EDPWRDOWN) {
        mcsr &= ~MCSR_EDPWRDOWN;
        phy_write_reg(REG_MCSR, mcsr);
        ESP_LOGI(TAG, "EDPD was enabled — now DISABLED");
    } else {
        ESP_LOGI(TAG, "EDPD already disabled (good)");
    }

    gpio_set_drive_capability(GPIO_NUM_17, GPIO_DRIVE_CAP_0);

    vTaskDelay(pdMS_TO_TICKS(300));

    /* ---- Network interface ---- */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    /* ---- Event handlers ---- */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* ---- Start ---- */
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
    ESP_LOGI(TAG, "Ethernet started (10BT-HD strap mode)");

    /* Let the EMAC stabilize at 10BT before changing PHY mode */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /*
     * Now enable AN while the EMAC runs at 10 Mbps.
     * At 10BT, the RMII bus is quieter (NLPs vs 100TX scrambled idle),
     * giving the PHY a better chance to exchange FLP capability pages.
     */
    uint32_t anar = AN_SELECTOR_IEEE | AN_10T_HD | AN_10T_FD | AN_100TX_HD | AN_100TX_FD;
    phy_write_reg(REG_ANAR, anar);

    uint32_t bmcr = phy_read_reg(REG_BMCR);
    bmcr |= BMCR_AN_ENABLE | BMCR_AN_RESTART;
    phy_write_reg(REG_BMCR, bmcr);
    ESP_LOGI(TAG, "AN enabled after EMAC stabilised (BMCR=0x%04x)", (unsigned)bmcr);

    dump_bmcr("Post-AN");
    dump_mcsr("Post-AN");

    ESP_LOGI(TAG, "Waiting for link & DHCP ...");

    /* Poll PHY status every 1 s while waiting for IP */
    for (int i = 0; i < 60; i++) {
        EventBits_t bits = xEventGroupWaitBits(eth_event_group, ETH_GOT_IP_BIT,
                                               pdFALSE, pdTRUE,
                                               pdMS_TO_TICKS(1000));
        if (bits & ETH_GOT_IP_BIT)
            break;

        uint32_t bc    = phy_read_reg(REG_BMCR);
        uint32_t bmsr_latch = phy_read_reg(REG_BMSR);
        (void)bmsr_latch;
        uint32_t bmsr  = phy_read_reg(REG_BMSR);
        uint32_t mr    = phy_read_reg(REG_MCSR);
        uint32_t pscsr = phy_read_reg(REG_PSCSR);
        ESP_LOGI(TAG, "[%2ds] BMCR=0x%04x BMSR=0x%04x Link=%d AN=%d/%d ENERGYON=%d Speed: %s",
                 i + 1,
                 (unsigned)bc,
                 (unsigned)bmsr,
                 !!(bmsr & BMSR_LINK_STATUS),
                 !!(bc & BMCR_AN_ENABLE),
                 !!(bmsr & BMSR_AN_COMPLETE),
                 !!(mr & MCSR_ENERGYON),
                 speed_str(pscsr));
    }

    EventBits_t bits = xEventGroupGetBits(eth_event_group);
    if (!(bits & ETH_GOT_IP_BIT)) {
        ESP_LOGE(TAG, "Timed out waiting for IP address");
        dump_all("Timeout");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    /* ---- DNS lookups ---- */
    dns_lookup("www.google.com");
    dns_lookup("espressif.com");
    dns_lookup("github.com");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Test complete");
    ESP_LOGI(TAG, "========================================");
}
