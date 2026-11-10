#include "xhci.h"
#include "../keyboard.h"
#include "../../sys/kernel/network/pci.h"
#include "../../sys/kernel/memory/pmm.h"
#include "../../sys/kernel/memory/mmu.h"
#include "../../sys/kernel/kernel.h"
#include <stdint.h>
#include <stddef.h>

#define XHCI_MAX_CONTROLLERS       16
#define XHCI_RING_TRBS             256u
#define XHCI_EVENT_TRBS            256u
#define XHCI_CONTROL_BUFFER_SIZE   4096u
#define XHCI_CONFIG_LIMIT          2048u
#define XHCI_WAIT_LIMIT 20000000u
#define XHCI_PORT_WAIT_LIMIT       30000000u
#define XHCI_HUB_WAIT_LIMIT        128u
#define XHCI_HUB_MAX_DEPTH         4u
#define XHCI_MFINDEX_MASK          0x3FFFu
#define XHCI_ROOT_CONNECT_WAIT     1600u   /* 24000u */
#define XHCI_MFINDEX_SPIN_LIMIT    40000000u
#define XHCI_PAGE_SIZE             4096u
#define XHCI_EXT_CAP_LIMIT         0x40000u
#define XHCI_MMIO_MAP_SIZE         0x100000ull
#define XHCI_RESCAN_INTERVAL       250000u
#define XHCI_FULL_RETRY_INTERVAL   500000u

#define XHCI_CAP_CAPLENGTH         0x00u
#define XHCI_CAP_HCSPARAMS1        0x04u
#define XHCI_CAP_HCSPARAMS2        0x08u
#define XHCI_CAP_HCCPARAMS1        0x10u
#define XHCI_CAP_DBOFF             0x14u
#define XHCI_CAP_RTSOFF            0x18u
#define XHCI_USBCMD                0x00u
#define XHCI_USBSTS                0x04u
#define XHCI_PAGESIZE              0x08u
#define XHCI_CRCR                  0x18u
#define XHCI_DCBAAP                0x30u
#define XHCI_CONFIG                0x38u
#define XHCI_PORTSC_BASE           0x400u
#define XHCI_PORTSC_STRIDE         0x10u

#define XHCI_CMD_RUN               (1u << 0)
#define XHCI_CMD_RESET             (1u << 1)
#define XHCI_STS_HALTED            (1u << 0)
#define XHCI_STS_HSE               (1u << 2)
#define XHCI_STS_EINT              (1u << 3)
#define XHCI_STS_PCD               (1u << 4)
#define XHCI_STS_NOT_READY         (1u << 11)
#define XHCI_STS_HCE               (1u << 12)
#define XHCI_PORT_CCS              (1u << 0)
#define XHCI_PORT_PED              (1u << 1)
#define XHCI_PORT_PR               (1u << 4)
#define XHCI_PORT_PP               (1u << 9)
#define XHCI_PORT_SPEED_SHIFT      10u
#define XHCI_PORT_SPEED_MASK       (0xFu << XHCI_PORT_SPEED_SHIFT)
#define XHCI_PORT_WPR              (1u << 31)
#define XHCI_PORT_CHANGE_BITS      (0x7Fu << 17)
#define XHCI_TRB_CYCLE             (1u << 0)
#define XHCI_TRB_TOGGLE_CYCLE      (1u << 1)
#define XHCI_TRB_CHAIN             (1u << 4)
#define XHCI_TRB_IOC               (1u << 5)
#define XHCI_TRB_ISP               (1u << 2)
#define XHCI_TRB_IDT               (1u << 6)
#define XHCI_TRB_TYPE_SHIFT        10u
#define XHCI_TRB_TYPE_MASK         (0x3Fu << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_DIR_IN            (1u << 16)
#define XHCI_TRB_LINK               6u
#define XHCI_TRB_ENABLE_SLOT        9u
#define XHCI_TRB_DISABLE_SLOT      10u
#define XHCI_TRB_ADDRESS_DEVICE    11u
#define XHCI_TRB_CONFIGURE_EP      12u
#define XHCI_TRB_EVALUATE_CONTEXT  13u
#define XHCI_TRB_RESET_ENDPOINT    14u
#define XHCI_TRB_SET_TR_DEQUEUE    16u
#define XHCI_TRB_SETUP_STAGE        2u
#define XHCI_TRB_DATA_STAGE         3u
#define XHCI_TRB_STATUS_STAGE       4u
#define XHCI_TRB_NORMAL             1u
#define XHCI_EVENT_TRANSFER         32u
#define XHCI_EVENT_COMMAND          33u
#define XHCI_EVENT_PORT             34u
#define XHCI_CC_SUCCESS             1u
#define XHCI_CC_SHORT_PACKET        13u
#define XHCI_RT_INTERRUPTER0        0x20u
#define XHCI_IMAN                   0x00u
#define XHCI_IMOD                   0x04u
#define XHCI_ERSTSZ                 0x08u
#define XHCI_ERSTBA                 0x10u
#define XHCI_ERDP                   0x18u
#define XHCI_IMAN_IP                (1u << 0)
#define XHCI_ERDP_EHB               (1u << 3)
#define XHCI_EP0_ID                 1u
#define XHCI_USB_EP_INTERRUPT_IN    7u

#define XHCI_SPEED_LOW              1u
#define XHCI_SPEED_FULL             2u
#define XHCI_SPEED_HIGH             3u
#define XHCI_SPEED_SUPER            4u
#define XHCI_SPEED_SUPER_PLUS       5u
static int xhci_wait_microframes(uint32_t ticks);
struct xhci_trb { uint32_t d[4]; } __attribute__((aligned(16)));
typedef struct xhci_trb xhci_trb_t;
typedef struct xhci_erst_entry {
    uint64_t base;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

#define HID_MAX_SEGS 16u
typedef struct xhci_kbd_layout {
    uint8_t valid, has_id, report_id, nsegs;
    uint16_t seg_off[HID_MAX_SEGS], seg_count[HID_MAX_SEGS], seg_base[HID_MAX_SEGS];
    uint16_t arr_off;
    uint8_t arr_size, arr_count;
    uint16_t report_bytes;
} xhci_kbd_layout_t;

#define XHCI_MAX_KEYBOARDS         4u

typedef struct xhci_keyboard {
    xhci_trb_t *ring;
    uint8_t *report_buffer;
    uint64_t ring_phys, report_phys;
    uint32_t events;
    uint16_t index;
    uint8_t cycle, used, slot_id, endpoint_id, packet_size;
    uint8_t report_offset, report_id, pending, errors, need_recover;
    uint8_t use_layout;
    uint16_t queue_len;
    xhci_kbd_layout_t layout;
} xhci_keyboard_t;

static void keyboard_handle_event(const xhci_trb_t *ev);

typedef struct xhci_state {
    volatile uint8_t *mmio, *op, *runtime, *doorbells;
    uint32_t max_ports, max_slots, context_size;
    uint64_t dcbaa_phys, command_ring_phys, event_ring_phys, erst_phys;
    uint64_t device_context_phys, input_context_phys, ep0_ring_phys;
    uint64_t keyboard_ring_phys, control_buffer_phys, config_buffer_phys;
    uint64_t report_buffer_phys;
    uint64_t *dcbaa, *scratchpad_array;
    xhci_trb_t *command_ring, *event_ring, *ep0_ring, *keyboard_ring;
    xhci_erst_entry_t *erst;
    uint8_t *device_context, *input_context, *control_buffer;
    uint8_t *config_buffer, *report_buffer;
    uint16_t command_index, event_index, ep0_index, keyboard_index;
    uint8_t command_cycle, event_cycle, ep0_cycle, keyboard_cycle;
    uint8_t slot_id, port_id, speed, speed_class, protocol_major;
    uint8_t keyboard_endpoint_id, keyboard_interface;
    uint8_t keyboard_packet_size, keyboard_interval;
    uint8_t keyboard_report_offset, keyboard_report_id;
    uint8_t keyboard_boot_protocol, keyboard_pending, ready, initialized;
    uint8_t keyboard_via_hub, port_power_control, controller_running;
    uint16_t keyboard_report_descriptor_length;
    uint32_t root_connected_ports, root_reset_ports;
    uint32_t rescan_countdown, full_retry_countdown;
    uint32_t hubs_seen, hub_status_errors, hub_connected_ports, hub_reset_ports;
    uint8_t port_protocol[256];
    uint8_t speed_class_by_protocol[4][16];
    uint8_t speed_id_by_class[4][6];
    const char *last_error;
    const char *status;
    uint8_t bios_released;
    uint8_t keyboard_errors, diag;
    uint8_t last_xfer_cc, last_cmd_cc;
    uint32_t dbg_xfer_events, dbg_xfer_errors, dbg_reports, dbg_polls;
    xhci_keyboard_t keyboards[XHCI_MAX_KEYBOARDS];
    uint8_t keyboard_count;
    xhci_kbd_layout_t kbd_layout;
    uint8_t keyboard_use_layout;
} xhci_state_t;

typedef struct xhci_device_snapshot {
    uint64_t device_context_phys, input_context_phys, ep0_ring_phys;
    uint64_t control_buffer_phys, config_buffer_phys;
    uint8_t *device_context, *input_context;
    xhci_trb_t *ep0_ring;
    uint8_t *control_buffer, *config_buffer;
    uint16_t ep0_index;
    uint8_t ep0_cycle, slot_id, port_id, speed, speed_class, protocol_major;
} xhci_device_snapshot_t;

static xhci_state_t g_xhci;

static inline void xhci_pause(void) { __asm__ volatile("pause" ::: "memory"); }
static inline uint32_t mmio_read32(volatile uint8_t *b, uint32_t o)
{ return *(volatile uint32_t *)(b + o); }
static inline void mmio_write32(volatile uint8_t *b, uint32_t o, uint32_t v)
{ *(volatile uint32_t *)(b + o) = v; }
static inline void mmio_write64(volatile uint8_t *b, uint32_t o, uint64_t v)
{
    mmio_write32(b, o, (uint32_t)v);
    mmio_write32(b, o + 4u, (uint32_t)(v >> 32));
}
static void zero_bytes(void *ptr, size_t n)
{
    volatile uint8_t *p=(volatile uint8_t *)ptr;
    for (size_t i=0;i<n;++i) p[i]=0;
}
static void *dma_alloc(size_t n,size_t a,uint64_t *phys)
{ return pmm_alloc_dma32(n,a,phys); }
static uint32_t trb_type(uint32_t c)
{ return (c & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT; }

static uint32_t *ctx_in(uint32_t index)
{ return (uint32_t *)(g_xhci.input_context + (size_t)index * g_xhci.context_size); }
static volatile uint32_t *ctx_out(uint32_t index)
{ return (volatile uint32_t *)(g_xhci.device_context + (size_t)index * g_xhci.context_size); }

static void dbg_value(const char *what,uint32_t value)
{
    if (!g_xhci.diag) return;
    print(what); print_int(value); print("\n");
}
static void dbg_hex(const char *what,uint32_t value)
{
    static const char digits[]="0123456789ABCDEF";
    char text[11];
    if (!g_xhci.diag) return;
    text[0]='0'; text[1]='x';
    for (unsigned int i=0;i<8u;++i) text[2u+i]=digits[(value>>(28u-4u*i))&0xFu];
    text[10]=0;
    print(what); print(text); print("\n");
}

static void speed_fill_usb2_defaults(void)
{
    if (!g_xhci.speed_class_by_protocol[2][1] && !g_xhci.speed_id_by_class[2][XHCI_SPEED_FULL]) {
        g_xhci.speed_class_by_protocol[2][1]=XHCI_SPEED_FULL;
        g_xhci.speed_id_by_class[2][XHCI_SPEED_FULL]=1;
    }
    if (!g_xhci.speed_class_by_protocol[2][2] && !g_xhci.speed_id_by_class[2][XHCI_SPEED_LOW]) {
        g_xhci.speed_class_by_protocol[2][2]=XHCI_SPEED_LOW;
        g_xhci.speed_id_by_class[2][XHCI_SPEED_LOW]=2;
    }
    if (!g_xhci.speed_class_by_protocol[2][3] && !g_xhci.speed_id_by_class[2][XHCI_SPEED_HIGH]) {
        g_xhci.speed_class_by_protocol[2][3]=XHCI_SPEED_HIGH;
        g_xhci.speed_id_by_class[2][XHCI_SPEED_HIGH]=3;
    }
}

static uint8_t speed_class_for(uint8_t protocol, uint8_t speed_id)
{
    if (protocol > 3u || speed_id > 15u) return 0;
    return g_xhci.speed_class_by_protocol[protocol][speed_id];
}
static uint8_t speed_id_for(uint8_t protocol, uint8_t speed_class)
{
    if (protocol > 3u || speed_class > 5u) return 0;
    return g_xhci.speed_id_by_class[protocol][speed_class];
}

static int parse_supported_protocols(uint32_t hccparams1)
{
    uint32_t offset=((hccparams1 >> 16) & 0xFFFFu) * 4u;
    uint32_t visited=0;
    int found=0;

    if (!offset || offset < 0x20u || offset >= XHCI_EXT_CAP_LIMIT) return 0;
    while (offset && offset + 16u <= XHCI_EXT_CAP_LIMIT && visited++ < 128u) {
        uint32_t header=mmio_read32(g_xhci.mmio,offset);
        uint8_t id=(uint8_t)header;
        uint8_t next=(uint8_t)(header >> 8);
        if (id==2u) {
            uint8_t minor=(uint8_t)(header >> 16);
            uint8_t major=(uint8_t)(header >> 24);
            uint32_t name=mmio_read32(g_xhci.mmio,offset+4u);
            uint32_t ports=mmio_read32(g_xhci.mmio,offset+8u);
            uint8_t first=(uint8_t)ports;
            uint8_t count=(uint8_t)(ports >> 8);
            uint8_t psic=(uint8_t)(ports >> 28);
            if (name==0x20425355u && (major==2u || major==3u) &&
                first!=0u && count!=0u &&
                (uint32_t)first+(uint32_t)count-1u<=g_xhci.max_ports &&
                offset+16u+(uint32_t)psic*4u<=XHCI_EXT_CAP_LIMIT) {
                for (uint32_t p=first;p<(uint32_t)first+count;++p)
                    g_xhci.port_protocol[p]=major;
                found=1;
                if (psic) {
                    for (uint8_t i=0;i<psic;++i) {
                        uint32_t psi=mmio_read32(g_xhci.mmio,offset+16u+(uint32_t)i*4u);
                        uint8_t psiv=(uint8_t)(psi & 0x0Fu);
                        uint8_t exp=(uint8_t)((psi >> 4) & 3u);
                        uint8_t plt=(uint8_t)((psi >> 6) & 3u);
                        uint8_t lp=(uint8_t)((psi >> 14) & 3u);
                        uint32_t mantissa=psi >> 16;
                        uint64_t rate=mantissa;
                        static const uint32_t units[4]={1u,1000u,1000000u,1000000000u};
                        uint8_t cls=0;
                        if (psiv==0u || !mantissa || plt==1u || plt==3u) continue;
                        rate *= units[exp];
                        if (major==2u) {
                            if (rate < 3000000ull) cls=XHCI_SPEED_LOW;
                            else if (rate < 100000000ull) cls=XHCI_SPEED_FULL;
                            else cls=XHCI_SPEED_HIGH;
                        } else {
                            cls=(lp==1u || rate>=8000000000ull) ?
                                XHCI_SPEED_SUPER_PLUS : XHCI_SPEED_SUPER;
                        }
                        g_xhci.speed_class_by_protocol[major][psiv]=cls;
                        if (!g_xhci.speed_id_by_class[major][cls])
                            g_xhci.speed_id_by_class[major][cls]=psiv;
                    }
                } else if (major==2u) {
                    g_xhci.speed_class_by_protocol[2][1]=XHCI_SPEED_FULL;
                    g_xhci.speed_class_by_protocol[2][2]=XHCI_SPEED_LOW;
                    g_xhci.speed_class_by_protocol[2][3]=XHCI_SPEED_HIGH;
                    g_xhci.speed_id_by_class[2][XHCI_SPEED_FULL]=1;
                    g_xhci.speed_id_by_class[2][XHCI_SPEED_LOW]=2;
                    g_xhci.speed_id_by_class[2][XHCI_SPEED_HIGH]=3;
                } else if (major==3u) {
                    g_xhci.speed_class_by_protocol[3][4]=XHCI_SPEED_SUPER;
                    g_xhci.speed_id_by_class[3][XHCI_SPEED_SUPER]=4;
                    if (minor>=0x10u) {
                        g_xhci.speed_class_by_protocol[3][5]=XHCI_SPEED_SUPER_PLUS;
                        g_xhci.speed_id_by_class[3][XHCI_SPEED_SUPER_PLUS]=5;
                    }
                    if (minor>=0x20u) {
                        g_xhci.speed_class_by_protocol[3][6]=XHCI_SPEED_SUPER_PLUS;
                        g_xhci.speed_class_by_protocol[3][7]=XHCI_SPEED_SUPER_PLUS;
                    }
                }
                if (major==2u) speed_fill_usb2_defaults();
            }
        }
        if (!next) break;
        offset += (uint32_t)next * 4u;
    }
    return found;
}

static uint64_t ring_push(xhci_trb_t *ring,uint64_t ring_phys,uint16_t *index,
                          uint8_t *cycle,uint32_t d0,uint32_t d1,uint32_t d2,
                          uint32_t control)
{
    xhci_trb_t *trb;
    uint64_t phys;
    if (*index>=XHCI_RING_TRBS-1u) {
        xhci_trb_t *link=&ring[XHCI_RING_TRBS-1u];
        uint32_t lc=((*cycle)?XHCI_TRB_CYCLE:0u)|XHCI_TRB_TOGGLE_CYCLE|
                    (XHCI_TRB_LINK<<XHCI_TRB_TYPE_SHIFT);
        if (ring[XHCI_RING_TRBS-2u].d[3]&XHCI_TRB_CHAIN) lc|=XHCI_TRB_CHAIN;
        link->d[0]=(uint32_t)ring_phys; link->d[1]=(uint32_t)(ring_phys>>32);
        link->d[2]=0; link->d[3]=lc; *index=0; *cycle^=1u;
    }
    trb=&ring[*index];
    phys=ring_phys+(uint64_t)(*index)*sizeof(xhci_trb_t);
    trb->d[0]=d0; trb->d[1]=d1; trb->d[2]=d2;
    trb->d[3]=(control&~XHCI_TRB_CYCLE)|((*cycle)?XHCI_TRB_CYCLE:0u);
    ++(*index);
    return phys;
}

static int event_pop(xhci_trb_t *out)
{
    volatile xhci_trb_t *ev=&g_xhci.event_ring[g_xhci.event_index];
    uint32_t control=ev->d[3];
    if ((control&XHCI_TRB_CYCLE)!=(g_xhci.event_cycle?XHCI_TRB_CYCLE:0u)) return 0;
    __asm__ volatile("lfence" ::: "memory");
    out->d[0]=ev->d[0]; out->d[1]=ev->d[1]; out->d[2]=ev->d[2]; out->d[3]=ev->d[3];
    if (++g_xhci.event_index>=XHCI_EVENT_TRBS) { g_xhci.event_index=0; g_xhci.event_cycle^=1u; }
    mmio_write64(g_xhci.runtime, XHCI_RT_INTERRUPTER0+XHCI_ERDP,
        (g_xhci.event_ring_phys+(uint64_t)g_xhci.event_index*sizeof(xhci_trb_t))|XHCI_ERDP_EHB);
    return 1;
}
static void event_discard_unrelated(const xhci_trb_t *ev)
{ keyboard_handle_event(ev); }

static int wait_command(uint64_t command_phys,uint8_t *slot_out)
{
    for (uint32_t spin=0;spin<XHCI_WAIT_LIMIT;++spin) {
        xhci_trb_t ev;
        if (!event_pop(&ev)) { 
            if (mmio_read32(g_xhci.op,XHCI_USBSTS) & XHCI_STS_HSE) {
                print("[XHCI] HSE detected! DMA blocked.\n");
                return 0;
            }
            xhci_pause(); 
            continue; 
        }
        if (trb_type(ev.d[3])==XHCI_EVENT_COMMAND) {
            uint64_t ptr=((uint64_t)ev.d[1]<<32)|ev.d[0];
            uint8_t cc=(uint8_t)(ev.d[2]>>24),slot=(uint8_t)(ev.d[3]>>24);
            if (ptr==command_phys) { 
                g_xhci.last_cmd_cc=cc; if (slot_out) *slot_out=slot; 
                if (cc!=XHCI_CC_SUCCESS)
                    dbg_value("[XHCI] command failed, completion code=",cc);
                return cc==XHCI_CC_SUCCESS;
            }
        }
        event_discard_unrelated(&ev);
    }
    print("[XHCI] Cmd Timeout. USBSTS=");
    print_int(mmio_read32(g_xhci.op,XHCI_USBSTS));
    print("\n");
    return 0;
}
static int wait_transfer(uint8_t slot,uint8_t ep,uint8_t *cc_out,uint32_t *residual_out)
{
    for (uint32_t spin=0;spin<XHCI_WAIT_LIMIT;++spin) {
        xhci_trb_t ev;
        if (!event_pop(&ev)) { xhci_pause(); continue; }
        if (trb_type(ev.d[3])==XHCI_EVENT_TRANSFER) {
            uint8_t es=(uint8_t)(ev.d[3]>>24),ee=(uint8_t)((ev.d[3]>>16)&0x1Fu);
            if (es==slot && ee==ep) {
                if (cc_out) *cc_out=(uint8_t)(ev.d[2]>>24);
                if (residual_out) *residual_out=ev.d[2]&0x00FFFFFFu;
                __asm__ volatile("mfence" ::: "memory");
                return 1;
            }
        }
        event_discard_unrelated(&ev);
    }
    return 0;
}
static int command_submit(uint32_t p0,uint32_t p1,uint32_t status,
                          uint32_t control,uint8_t *slot_out)
{
    uint64_t phys=ring_push(g_xhci.command_ring,g_xhci.command_ring_phys,
        &g_xhci.command_index,&g_xhci.command_cycle,p0,p1,status,control);
    __asm__ volatile("sfence" ::: "memory");
    mmio_write32(g_xhci.doorbells,0,0);
    return wait_command(phys,slot_out);
}

static void clear_active_device_fields(void)
{
    g_xhci.slot_id=g_xhci.port_id=g_xhci.speed=g_xhci.speed_class=0;
    g_xhci.device_context_phys=g_xhci.input_context_phys=g_xhci.ep0_ring_phys=0;
    g_xhci.control_buffer_phys=g_xhci.config_buffer_phys=g_xhci.report_buffer_phys=0;
    g_xhci.device_context=g_xhci.input_context=0;
    g_xhci.ep0_ring=g_xhci.keyboard_ring=0;
    g_xhci.control_buffer=g_xhci.config_buffer=g_xhci.report_buffer=0;
    g_xhci.ep0_index=g_xhci.keyboard_index=0;
    g_xhci.ep0_cycle=g_xhci.keyboard_cycle=1;
    g_xhci.keyboard_pending=0;   /* 'ready' belongs to the stashed keyboards */
    g_xhci.keyboard_endpoint_id=g_xhci.keyboard_packet_size=g_xhci.keyboard_interval=0;
    g_xhci.keyboard_report_offset=g_xhci.keyboard_report_id=0;
    g_xhci.keyboard_report_descriptor_length=0;
    g_xhci.keyboard_boot_protocol=0;
    g_xhci.keyboard_errors=0;
    g_xhci.keyboard_use_layout=0;
    g_xhci.kbd_layout.valid=0;
}

static void disable_active_slot(void)
{
    uint8_t slot=g_xhci.slot_id;
    int disabled;
    if (!slot || !g_xhci.command_ring) return;
    disabled=command_submit(0,0,0,(XHCI_TRB_DISABLE_SLOT<<XHCI_TRB_TYPE_SHIFT)|((uint32_t)slot<<24),0);
    if (disabled) {
        if (g_xhci.dcbaa && slot<=g_xhci.max_slots) g_xhci.dcbaa[slot]=0;
        if (g_xhci.device_context) pmm_free_pages((uintptr_t)g_xhci.device_context,1u);
        if (g_xhci.input_context) pmm_free_pages((uintptr_t)g_xhci.input_context,1u);
        if (g_xhci.ep0_ring) pmm_free_pages((uintptr_t)g_xhci.ep0_ring,1u);
        if (g_xhci.keyboard_ring) pmm_free_pages((uintptr_t)g_xhci.keyboard_ring,1u);
        if (g_xhci.control_buffer) pmm_free_pages((uintptr_t)g_xhci.control_buffer,1u);
        if (g_xhci.config_buffer) pmm_free_pages((uintptr_t)g_xhci.config_buffer,1u);
        if (g_xhci.report_buffer) pmm_free_pages((uintptr_t)g_xhci.report_buffer,1u);
    }
    clear_active_device_fields();
}

static void save_device_snapshot(xhci_device_snapshot_t *s)
{
    s->device_context_phys=g_xhci.device_context_phys;
    s->input_context_phys=g_xhci.input_context_phys;
    s->ep0_ring_phys=g_xhci.ep0_ring_phys;
    s->control_buffer_phys=g_xhci.control_buffer_phys;
    s->config_buffer_phys=g_xhci.config_buffer_phys;
    s->device_context=g_xhci.device_context; s->input_context=g_xhci.input_context;
    s->ep0_ring=g_xhci.ep0_ring; s->control_buffer=g_xhci.control_buffer;
    s->config_buffer=g_xhci.config_buffer; s->ep0_index=g_xhci.ep0_index;
    s->ep0_cycle=g_xhci.ep0_cycle; s->slot_id=g_xhci.slot_id;
    s->port_id=g_xhci.port_id; s->speed=g_xhci.speed;
    s->speed_class=g_xhci.speed_class; s->protocol_major=g_xhci.protocol_major;
}
static void restore_device_snapshot(const xhci_device_snapshot_t *s)
{
    clear_active_device_fields();
    g_xhci.device_context_phys=s->device_context_phys;
    g_xhci.input_context_phys=s->input_context_phys;
    g_xhci.ep0_ring_phys=s->ep0_ring_phys;
    g_xhci.control_buffer_phys=s->control_buffer_phys;
    g_xhci.config_buffer_phys=s->config_buffer_phys;
    g_xhci.device_context=s->device_context; g_xhci.input_context=s->input_context;
    g_xhci.ep0_ring=s->ep0_ring; g_xhci.control_buffer=s->control_buffer;
    g_xhci.config_buffer=s->config_buffer; g_xhci.ep0_index=s->ep0_index;
    g_xhci.ep0_cycle=s->ep0_cycle; g_xhci.slot_id=s->slot_id;
    g_xhci.port_id=s->port_id; g_xhci.speed=s->speed;
    g_xhci.speed_class=s->speed_class; g_xhci.protocol_major=s->protocol_major;
}

static int control_transfer(uint8_t request_type,uint8_t request,uint16_t value,
                            uint16_t index,void *data,uint16_t length)
{
    uint8_t setup[8],cc=0;
    uint64_t setup_value;
    uint32_t setup_control,data_control,status_control;
    if ((length&&!data)||length>XHCI_CONTROL_BUFFER_SIZE) return 0;
    setup[0]=request_type; setup[1]=request;
    setup[2]=(uint8_t)value; setup[3]=(uint8_t)(value>>8);
    setup[4]=(uint8_t)index; setup[5]=(uint8_t)(index>>8);
    setup[6]=(uint8_t)length; setup[7]=(uint8_t)(length>>8);
    setup_value=(uint64_t)setup[0]|((uint64_t)setup[1]<<8)|
        ((uint64_t)setup[2]<<16)|((uint64_t)setup[3]<<24)|
        ((uint64_t)setup[4]<<32)|((uint64_t)setup[5]<<40)|
        ((uint64_t)setup[6]<<48)|((uint64_t)setup[7]<<56);
    setup_control=XHCI_TRB_IDT|(XHCI_TRB_SETUP_STAGE<<XHCI_TRB_TYPE_SHIFT);
    if (length) setup_control|=((request_type&0x80u)?3u:2u)<<16;
    (void)ring_push(g_xhci.ep0_ring,g_xhci.ep0_ring_phys,&g_xhci.ep0_index,
        &g_xhci.ep0_cycle,(uint32_t)setup_value,(uint32_t)(setup_value>>32),8u,setup_control);
    if (length) {
        uint64_t phys=(uint64_t)(uintptr_t)data;
        data_control=(XHCI_TRB_DATA_STAGE<<XHCI_TRB_TYPE_SHIFT);
        if (request_type&0x80u) data_control|=XHCI_TRB_DIR_IN;
        (void)ring_push(g_xhci.ep0_ring,g_xhci.ep0_ring_phys,&g_xhci.ep0_index,
            &g_xhci.ep0_cycle,(uint32_t)phys,(uint32_t)(phys>>32),length,data_control);
    }
    status_control=XHCI_TRB_IOC|(XHCI_TRB_STATUS_STAGE<<XHCI_TRB_TYPE_SHIFT);
    if (!length || !(request_type&0x80u)) status_control|=XHCI_TRB_DIR_IN;
    (void)ring_push(g_xhci.ep0_ring,g_xhci.ep0_ring_phys,&g_xhci.ep0_index,
        &g_xhci.ep0_cycle,0,0,0,status_control);
    __asm__ volatile("sfence" ::: "memory");
    mmio_write32(g_xhci.doorbells,(uint32_t)g_xhci.slot_id*4u,XHCI_EP0_ID);
    if (!wait_transfer(g_xhci.slot_id,XHCI_EP0_ID,&cc,0)) {
        dbg_value("[XHCI] control transfer timeout, bRequest=",request);
        return 0;
    }
    if (cc!=XHCI_CC_SUCCESS && cc!=XHCI_CC_SHORT_PACKET) {
        dbg_value("[XHCI] control transfer failed, bRequest=",request);
        dbg_value("[XHCI]   completion code=",cc);
        return 0;
    }
    return 1;
}

static int legacy_handoff(volatile uint8_t *mmio,uint32_t hccparams1)
{
    uint32_t off=((hccparams1>>16)&0xFFFFu)*4u,visited=0;
    if (!off) return 1;
    while (off && visited++<64u && off<XHCI_EXT_CAP_LIMIT) {
        uint32_t h=mmio_read32(mmio,off);
        uint8_t id=(uint8_t)h,next=(uint8_t)(h>>8);
        if (id==1u) {
            uint32_t ctl;
            int released=!(h&(1u<<16));            /* BIOS Owned Semaphore */
            if (!released) {
                mmio_write32(mmio,off,h|(1u<<24)); /* HC OS Owned Semaphore */
                for (uint32_t spin=0;spin<40000000u;++spin) {
                    h=mmio_read32(mmio,off);
                    if (!(h&(1u<<16))) { released=1; break; }
                    xhci_pause();
                }
                if (!released) {
                    /* BIOS never answered: take the controller anyway. */
                    mmio_write32(mmio,off,(h&~(1u<<16))|(1u<<24));
                }
            }
            g_xhci.bios_released=(uint8_t)released;
            /* USBLEGCTLSTS: switch off every SMI source (bits 0,4,13,14,15)
               and clear pending SMI status (RW1C bits 29..31). */
            ctl=mmio_read32(mmio,off+4u);
            ctl&=~((1u<<0)|(1u<<4)|(1u<<13)|(1u<<14)|(1u<<15));
            ctl|=(7u<<29);
            mmio_write32(mmio,off+4u,ctl);
            return 1;
        }
        if (!next) break;
        off+=(uint32_t)next*4u;
    }
    return 1;
}

static int controller_reset(void)
{
    uint32_t cmd=mmio_read32(g_xhci.op,XHCI_USBCMD);
    mmio_write32(g_xhci.op,XHCI_USBCMD,cmd&~XHCI_CMD_RUN);
    for (uint32_t i=0;i<XHCI_WAIT_LIMIT;++i) {
        if (mmio_read32(g_xhci.op,XHCI_USBSTS)&XHCI_STS_HALTED) break;
        if (i+1u==XHCI_WAIT_LIMIT) return 0;
        xhci_pause();
    }
    mmio_write32(g_xhci.op,XHCI_USBCMD,
        (mmio_read32(g_xhci.op,XHCI_USBCMD)&~XHCI_CMD_RUN)|XHCI_CMD_RESET);
    for (uint32_t i=0;i<XHCI_WAIT_LIMIT;++i) {
        if (!(mmio_read32(g_xhci.op,XHCI_USBCMD)&XHCI_CMD_RESET)) break;
        if (i+1u==XHCI_WAIT_LIMIT) return 0;
        xhci_pause();
    }
    for (uint32_t i=0;i<XHCI_WAIT_LIMIT;++i) {
        if (!(mmio_read32(g_xhci.op,XHCI_USBSTS)&XHCI_STS_NOT_READY)) return 1;
        xhci_pause();
    }
    return 0;
}

static int controller_allocate_rings(uint32_t hcsparams2)
{
    /* HCSPARAMS2: Max Scratchpad Bufs Hi = bits 25:21, Lo = bits 31:27. */
    uint32_t scratch_count =
        (((hcsparams2 >> 21) & 0x1Fu) << 5) |
        ((hcsparams2 >> 27) & 0x1Fu);

    g_xhci.dcbaa=(uint64_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.dcbaa_phys);
    g_xhci.command_ring=(xhci_trb_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.command_ring_phys);
    g_xhci.event_ring=(xhci_trb_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.event_ring_phys);
    g_xhci.erst=(xhci_erst_entry_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.erst_phys);
    if (!g_xhci.dcbaa||!g_xhci.command_ring||!g_xhci.event_ring||!g_xhci.erst) return 0;
    mmu_mark_mmio_uncached(g_xhci.dcbaa_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.command_ring_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.event_ring_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.erst_phys, XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.dcbaa,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.command_ring,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.event_ring,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.erst,XHCI_PAGE_SIZE);
    g_xhci.dcbaa[0]=0;
    if (scratch_count) {
        uint64_t array_phys=0;
        size_t array_bytes = (size_t)scratch_count * 8u;
        if (array_bytes < XHCI_PAGE_SIZE) array_bytes = XHCI_PAGE_SIZE;
        uint64_t *array=(uint64_t *)dma_alloc(array_bytes,64u,&array_phys);
        if (!array) return 0;
        zero_bytes(array,array_bytes);
        for (uint32_t i=0;i<scratch_count;++i) {
            uint64_t page_phys=0;
            void *page=dma_alloc(XHCI_PAGE_SIZE,XHCI_PAGE_SIZE,&page_phys);
            if (!page) return 0;
            zero_bytes(page,XHCI_PAGE_SIZE);
            array[i]=page_phys;
        }
        g_xhci.scratchpad_array=array;
        g_xhci.dcbaa[0]=array_phys;
    }
    g_xhci.command_index=0; g_xhci.command_cycle=1;
    g_xhci.event_index=0; g_xhci.event_cycle=1;
    g_xhci.command_ring[XHCI_RING_TRBS-1u].d[0]=(uint32_t)g_xhci.command_ring_phys;
    g_xhci.command_ring[XHCI_RING_TRBS-1u].d[1]=(uint32_t)(g_xhci.command_ring_phys>>32);
    g_xhci.command_ring[XHCI_RING_TRBS-1u].d[2]=0;
    g_xhci.command_ring[XHCI_RING_TRBS-1u].d[3]=XHCI_TRB_CYCLE|XHCI_TRB_TOGGLE_CYCLE|
        (XHCI_TRB_LINK<<XHCI_TRB_TYPE_SHIFT);
    g_xhci.erst[0].base=g_xhci.event_ring_phys;
    g_xhci.erst[0].size=XHCI_EVENT_TRBS; g_xhci.erst[0].reserved=0;
    mmio_write64(g_xhci.op,XHCI_DCBAAP,g_xhci.dcbaa_phys);
    mmio_write64(g_xhci.op,XHCI_CRCR,g_xhci.command_ring_phys|1u);
    mmio_write32(g_xhci.runtime,XHCI_RT_INTERRUPTER0+XHCI_ERSTSZ,1u);
    mmio_write64(g_xhci.runtime,XHCI_RT_INTERRUPTER0+XHCI_ERSTBA,g_xhci.erst_phys);
    mmio_write64(g_xhci.runtime,XHCI_RT_INTERRUPTER0+XHCI_ERDP,g_xhci.event_ring_phys);
    mmio_write32(g_xhci.runtime,XHCI_RT_INTERRUPTER0+XHCI_IMOD,0u);
    mmio_write32(g_xhci.runtime,XHCI_RT_INTERRUPTER0+XHCI_IMAN,XHCI_IMAN_IP);
    return 1;
}

static int xhci_wait_microframes(uint32_t ticks);

static int controller_start(volatile uint8_t *mmio)
{
    uint32_t caplen=mmio_read32(mmio,XHCI_CAP_CAPLENGTH)&0xFFu;
    uint32_t hcs1=mmio_read32(mmio,XHCI_CAP_HCSPARAMS1);
    uint32_t hcs2=mmio_read32(mmio,XHCI_CAP_HCSPARAMS2);
    uint32_t hcc1=mmio_read32(mmio,XHCI_CAP_HCCPARAMS1);
    uint32_t dboff=mmio_read32(mmio,XHCI_CAP_DBOFF)&~3u;
    uint32_t rtsoff=mmio_read32(mmio,XHCI_CAP_RTSOFF)&~0x1Fu;
    if (caplen<0x20u||!(hcs1&0xFFu)) { g_xhci.status="xHCI: invalid capability registers"; return 0; }
    g_xhci.mmio=mmio; g_xhci.op=mmio+caplen;
    g_xhci.runtime=mmio+rtsoff; g_xhci.doorbells=mmio+dboff;
    g_xhci.max_slots=hcs1&0xFFu; g_xhci.max_ports=hcs1>>24;
    g_xhci.context_size=(hcc1&(1u<<2))?64u:32u;
    g_xhci.port_power_control=(uint8_t)((hcc1>>3)&1u);
    if (!g_xhci.max_slots||!g_xhci.max_ports) { g_xhci.status="xHCI: no slots or root ports"; return 0; }
    if (!(mmio_read32(g_xhci.op,XHCI_PAGESIZE)&1u)) { g_xhci.status="xHCI: 4 KiB pages unsupported"; return 0; }
    if (!parse_supported_protocols(hcc1)) {
        g_xhci.status="xHCI: Supported Protocol capabilities missing or invalid";
        return 0;
    }
    g_xhci.status="xHCI: BIOS ownership handoff";
    (void)legacy_handoff(mmio,hcc1);
    g_xhci.status="xHCI: controller reset";
    if (!controller_reset()) { g_xhci.status="xHCI: halt/reset timed out"; return 0; }
    mmio_write32(g_xhci.op, XHCI_USBSTS, 0x1Eu);
    g_xhci.status="xHCI: allocating DMA rings";
    if (!controller_allocate_rings(hcs2)) { g_xhci.status="xHCI: DMA ring allocation failed"; return 0; }
    mmio_write32(g_xhci.op,XHCI_CONFIG,g_xhci.max_slots);
    mmio_write32(g_xhci.op,XHCI_USBCMD,XHCI_CMD_RUN | (1u << 3) | (1u << 2));
    
    for (uint32_t i=0;i<XHCI_WAIT_LIMIT;++i) {
        uint32_t sts=mmio_read32(g_xhci.op,XHCI_USBSTS);
        if (!(sts&XHCI_STS_HALTED) && !(sts&XHCI_STS_NOT_READY)) {
            (void)xhci_wait_microframes(160); 
            return 1;
        }
        xhci_pause();
    }
    g_xhci.status="xHCI: controller did not enter Running state";
    return 0;
}

static int port_reset(uint8_t port_id,uint8_t protocol_major,uint8_t *speed_out)
{
    uint32_t off=XHCI_PORTSC_BASE+((uint32_t)port_id-1u)*XHCI_PORTSC_STRIDE;
    uint32_t portsc=mmio_read32(g_xhci.op,off);
    uint32_t reset_mask,pls;

    if (!(portsc&XHCI_PORT_CCS)) return 0;
    if (protocol_major!=2u && protocol_major!=3u) return 0;

    if (!(portsc&XHCI_PORT_PP)) {
        mmio_write32(g_xhci.op,off,XHCI_PORT_PP);
        for (volatile uint32_t d=0;d<500000u;++d) xhci_pause();
        portsc=mmio_read32(g_xhci.op,off);
        if (!(portsc&XHCI_PORT_CCS)) return 0;
    }

    pls=(portsc>>5)&0xFu;
    if (protocol_major==3u) {
        uint8_t sp=(uint8_t)((portsc&XHCI_PORT_SPEED_MASK)>>XHCI_PORT_SPEED_SHIFT);
        if ((portsc&XHCI_PORT_PED) && pls==0u && sp!=0u) {
            /* SuperSpeed link already trained and enabled (U0): no reset. */
            *speed_out=sp;
            mmio_write32(g_xhci.op,off,(portsc&XHCI_PORT_PP)|XHCI_PORT_CHANGE_BITS);
            return 1;
        }
        /* Warm reset only for Inactive (6) / Compliance (10); else hot reset. */
        reset_mask=(pls==6u||pls==10u)?XHCI_PORT_WPR:XHCI_PORT_PR;
    } else {
        reset_mask=XHCI_PORT_PR;
    }

    mmio_write32(g_xhci.op,off,(portsc&XHCI_PORT_PP)|reset_mask);

    {
        uint32_t start=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
        for (uint32_t spin=0;spin<XHCI_PORT_WAIT_LIMIT;++spin) {
            uint32_t now;
            portsc=mmio_read32(g_xhci.op,off);
            if (!(portsc&XHCI_PORT_CCS)) return 0;
            if (!(portsc&XHCI_PORT_PR) && !(portsc&reset_mask)) {
                uint8_t speed=(uint8_t)((portsc&XHCI_PORT_SPEED_MASK)>>XHCI_PORT_SPEED_SHIFT);
                if ((portsc&XHCI_PORT_PED) && speed!=0u) {
                    *speed_out=speed;
                    /* Acknowledge PRC/WRC/CSC/... so they do not linger. */
                    mmio_write32(g_xhci.op,off,(portsc&XHCI_PORT_PP)|XHCI_PORT_CHANGE_BITS);
                    return 1;
                }
            }
            now=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
            if (((now-start)&XHCI_MFINDEX_MASK)>=8000u) return 0;
            xhci_pause();
        }
    }
    return 0;
}

static int allocate_device_contexts(void)
{
    g_xhci.device_context=(uint8_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.device_context_phys);
    g_xhci.input_context=(uint8_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.input_context_phys);
    g_xhci.ep0_ring=(xhci_trb_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.ep0_ring_phys);
    g_xhci.control_buffer=(uint8_t *)dma_alloc(XHCI_CONTROL_BUFFER_SIZE,XHCI_PAGE_SIZE,&g_xhci.control_buffer_phys);
    g_xhci.config_buffer=(uint8_t *)dma_alloc(XHCI_CONTROL_BUFFER_SIZE,XHCI_PAGE_SIZE,&g_xhci.config_buffer_phys);
    if (!g_xhci.device_context||!g_xhci.input_context||!g_xhci.ep0_ring||
        !g_xhci.control_buffer||!g_xhci.config_buffer) return 0;
    mmu_mark_mmio_uncached(g_xhci.device_context_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.input_context_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.ep0_ring_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.control_buffer_phys, XHCI_CONTROL_BUFFER_SIZE);
    mmu_mark_mmio_uncached(g_xhci.config_buffer_phys, XHCI_CONTROL_BUFFER_SIZE);
    zero_bytes(g_xhci.device_context,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.input_context,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.ep0_ring,XHCI_PAGE_SIZE);
    zero_bytes(g_xhci.control_buffer,XHCI_CONTROL_BUFFER_SIZE);
    zero_bytes(g_xhci.config_buffer,XHCI_CONTROL_BUFFER_SIZE);
    g_xhci.ep0_index=0; g_xhci.ep0_cycle=1;
    g_xhci.ep0_ring[XHCI_RING_TRBS-1u].d[0]=(uint32_t)g_xhci.ep0_ring_phys;
    g_xhci.ep0_ring[XHCI_RING_TRBS-1u].d[1]=(uint32_t)(g_xhci.ep0_ring_phys>>32);
    g_xhci.ep0_ring[XHCI_RING_TRBS-1u].d[2]=0;
    g_xhci.ep0_ring[XHCI_RING_TRBS-1u].d[3]=XHCI_TRB_CYCLE|XHCI_TRB_TOGGLE_CYCLE|
        (XHCI_TRB_LINK<<XHCI_TRB_TYPE_SHIFT);
    return 1;
}

/* Build the Input Context for Address Device.  The EP0 TR Dequeue Pointer is
   the CURRENT enqueue position of the software ring (not the ring base):
   after the BSR=1 pass the first control transfer already used TRBs, and the
   second Address Device re-initialises the endpoint from this context. */
static void build_address_input_context(uint8_t root_port,uint8_t speed,
    uint8_t speed_class,uint32_t route_string,uint8_t parent_hub_slot,
    uint8_t parent_hub_port,uint8_t parent_hub_speed,uint8_t upstream_tt_slot,
    uint8_t upstream_tt_port,uint16_t ep0_packet)
{
    uint32_t *in,*slot,*ep0;
    uint64_t deq;
    zero_bytes(g_xhci.input_context,XHCI_PAGE_SIZE);
    in=ctx_in(0); slot=ctx_in(1); ep0=ctx_in(2);
    deq=g_xhci.ep0_ring_phys+(uint64_t)g_xhci.ep0_index*sizeof(xhci_trb_t);
    in[0]=0;
    in[1]=(1u<<0)|(1u<<XHCI_EP0_ID);
    /* Slot DW0: route string, speed (PSIV), context entries = 1 */
    slot[0]=((uint32_t)speed<<20)|(1u<<27)|(route_string&0x000FFFFFu);
    /* Slot DW1: root hub port number */
    slot[1]=(uint32_t)root_port<<16;
    /* Slot DW2: for LS/FS devices behind a HS hub, the Transaction Translator
       hub slot id [7:0] and port number [15:8]. */
    slot[2]=0;
    if (speed_class<=XHCI_SPEED_FULL) {
        uint8_t tt_slot=upstream_tt_slot,tt_port=upstream_tt_port;
        if (!tt_slot && parent_hub_slot &&
            speed_class_for(g_xhci.protocol_major,parent_hub_speed)==XHCI_SPEED_HIGH) {
            tt_slot=parent_hub_slot;
            tt_port=parent_hub_port;
        }
        if (tt_slot) slot[2]=(uint32_t)tt_slot|((uint32_t)tt_port<<8);
    }
    ep0[0]=0;
    ep0[1]=((uint32_t)ep0_packet<<16)|(3u<<1)|(4u<<3);   /* CErr=3, Control */
    ep0[2]=(uint32_t)deq|(g_xhci.ep0_cycle?1u:0u);
    ep0[3]=(uint32_t)(deq>>32);
    ep0[4]=8u;
    __asm__ volatile("sfence" ::: "memory");
}

static int address_device_cmd(int bsr)
{
    uint32_t control=(XHCI_TRB_ADDRESS_DEVICE<<XHCI_TRB_TYPE_SHIFT)|
                     ((uint32_t)g_xhci.slot_id<<24)|(bsr?(1u<<9):0u);
    uint64_t phys=ring_push(g_xhci.command_ring,g_xhci.command_ring_phys,
        &g_xhci.command_index,&g_xhci.command_cycle,
        (uint32_t)g_xhci.input_context_phys,
        (uint32_t)(g_xhci.input_context_phys>>32),0,control);
    __asm__ volatile("sfence" ::: "memory");
    mmio_write32(g_xhci.doorbells,0,0);
    return wait_command(phys,0);
}

static uint8_t xhci_interval(uint8_t speed_class,uint8_t b_interval)
{
    uint32_t microframes;
    uint8_t exponent=0;
    if (!b_interval) b_interval=1;
    if (speed_class>=XHCI_SPEED_HIGH) {
        exponent=(uint8_t)(b_interval-1u);
    } else {
        /* FS/LS: xHCI specifies rounding DOWN to a power-of-two multiple
           of bInterval * 8 microframes (Intel xHCI 1.2, Table 6-12). */
        microframes=(uint32_t)b_interval*8u;
        while (exponent<15u && ((uint32_t)1u<<(exponent+1u))<=microframes)
            ++exponent;
    }
    if (exponent>15u) exponent=15u;
    return exponent;
}

static int find_keyboard_interface(const uint8_t *config,uint16_t total,
                                   uint8_t *config_value,uint8_t *iface,
                                   uint8_t *endpoint_id,uint8_t *packet_size,
                                   uint8_t *interval,uint8_t *boot_protocol,
                                   uint16_t *report_length)
{
    uint16_t pos=0, current_report=0;
    uint8_t current_iface=0,current_subclass=0,current_protocol=0;
    uint8_t priority=0xFFu,best=0xFFu;
    int current_hid=0,found=0;
    if (!config||total<9u||!config_value||!iface||!endpoint_id||
        !packet_size||!interval||!boot_protocol||!report_length) return 0;
    *config_value=config[5];
    while (pos+2u<=total) {
        uint8_t len=config[pos],type=config[pos+1u];
        if (len<2u||(uint32_t)pos+len>total) break;   /* keep what was found so far */
        if (type==4u&&len>=9u) {
            current_iface=config[pos+2u];
            current_subclass=config[pos+6u];
            current_protocol=config[pos+7u];
            current_hid=(config[pos+5u]==3u&&current_protocol!=2u);
            current_report=0;
            if (!current_hid) priority=0xFFu;
            else if (current_subclass==1u&&current_protocol==1u) priority=0u;
            else if (current_subclass==1u) priority=1u;
            else if (current_protocol==1u) priority=2u;
            else priority=3u;
        } else if (type==0x21u&&len>=9u&&current_hid) {
            uint8_t count=config[pos+5u];
            for (uint8_t d=0;d<count;++d) {
                uint16_t dp=(uint16_t)(pos+6u+d*3u);
                if ((uint32_t)dp+3u>(uint32_t)pos+len) break;
                if (config[dp]==0x22u) {
                    current_report=(uint16_t)config[dp+1u]|((uint16_t)config[dp+2u]<<8);
                    break;
                }
            }
        } else if (type==5u&&len>=7u&&current_hid&&priority<best) {
            uint8_t addr=config[pos+2u],attr=config[pos+3u]&3u;
            uint16_t maxp=(uint16_t)config[pos+4u]|((uint16_t)config[pos+5u]<<8);
            maxp&=0x07FFu;
            if ((addr&0x80u)&&attr==3u&&(addr&0x0Fu)&&maxp>=1u&&maxp<=64u) {
                best=priority;
                *iface=current_iface;
                *endpoint_id=(uint8_t)(((addr&0x0Fu)*2u)+1u);
                *packet_size=(uint8_t)maxp;
                *interval=config[pos+6u]?config[pos+6u]:1u;
                *boot_protocol=(uint8_t)(current_subclass==1u&&current_protocol==1u);
                *report_length=current_report;
                found=1;
            }
        }
        pos=(uint16_t)(pos+len);
    }
    return found;
}

static void hid_seg_add(xhci_kbd_layout_t *l,uint32_t off,uint32_t count,uint32_t base)
{
    if (l->nsegs>=HID_MAX_SEGS||off>0xFFFFu||count>0xFFFFu) return;
    l->seg_off[l->nsegs]=(uint16_t)off;
    l->seg_count[l->nsegs]=(uint16_t)count;
    l->seg_base[l->nsegs]=(uint16_t)base;
    ++l->nsegs;
}

/* Walk the HID report descriptor and describe the first keyboard report
   (Generic Desktop / Keyboard application collection, Keyboard-page inputs).
   Handles the boot layout, report IDs, NKRO bitmaps and mixed layouts. */
static int hid_parse_keyboard_layout(const uint8_t *desc,uint16_t length,
                                     xhci_kbd_layout_t *out)
{
    uint32_t usage_page=0,rsize=0,rcount=0,umin=0,umax=0;
    uint32_t usages[16];
    uint32_t nusages=0,depth=0,kbd_depth=0,nids=0,target_idx=8u;
    uint8_t ids[8];
    uint32_t bitpos[8];
    uint8_t cur_id=0,target_id=0;
    int have_min=0,have_max=0,have_target=0;
    uint16_t pos=0;

    if (!desc||!length||!out) return 0;
    for (unsigned int i=0;i<sizeof(*out);++i) ((uint8_t *)out)[i]=0;
    while (pos<length) {
        uint8_t prefix=desc[pos++],size_code,item_size,item_type,item_tag;
        uint32_t value=0;
        if (prefix==0xFEu) {                        /* long item: skip */
            uint8_t n;
            if ((uint32_t)pos+2u>length) break;
            n=desc[pos];
            pos=(uint16_t)(pos+2u);
            if ((uint32_t)pos+n>length) break;
            pos=(uint16_t)(pos+n);
            continue;
        }
        size_code=prefix&3u; item_size=(size_code==3u)?4u:size_code;
        item_type=(prefix>>2)&3u; item_tag=(prefix>>4)&0x0Fu;
        if ((uint32_t)pos+item_size>length) break;
        for (uint8_t i=0;i<item_size;++i) value|=(uint32_t)desc[pos+i]<<(8u*i);
        pos=(uint16_t)(pos+item_size);

        if (item_type==1u) {                        /* Global */
            if (item_tag==0u) usage_page=value&0xFFFFu;
            else if (item_tag==7u) rsize=value;
            else if (item_tag==8u) cur_id=(uint8_t)value;
            else if (item_tag==9u) rcount=value;
        } else if (item_type==2u) {                 /* Local */
            if (item_tag==0u) { if (nusages<16u) usages[nusages++]=value&0xFFFFu; }
            else if (item_tag==1u) { umin=value&0xFFFFu; have_min=1; }
            else if (item_tag==2u) { umax=value&0xFFFFu; have_max=1; }
        } else if (item_type==0u) {                 /* Main */
            if (item_tag==10u) {                    /* Collection */
                int is_kbd=(value==1u&&usage_page==1u&&nusages&&usages[nusages-1u]==6u);
                ++depth;
                if (is_kbd&&!kbd_depth) kbd_depth=depth;
            } else if (item_tag==12u) {             /* End Collection */
                if (depth) {
                    if (kbd_depth&&depth==kbd_depth) kbd_depth=0;
                    --depth;
                }
            } else if (item_tag==8u) {              /* Input */
                uint32_t idx=8u,bits=rsize*rcount;
                for (uint32_t i=0;i<nids;++i) if (ids[i]==cur_id) { idx=i; break; }
                if (idx==8u&&nids<8u) {
                    ids[nids]=cur_id; bitpos[nids]=0; idx=nids++;
                }
                if (idx<8u) {
                    uint32_t start=bitpos[idx];
                    int active=(kbd_depth&&depth>=kbd_depth);
                    if (active&&usage_page==7u&&!(value&1u)&&rsize&&rcount&&
                        (!have_target||cur_id==target_id)) {
                        int found=0;
                        if (value&2u) {             /* variable: 1-bit fields */
                            if (rsize==1u) {
                                if (nusages) {
                                    for (uint32_t i=0;i<rcount&&i<nusages;++i)
                                        hid_seg_add(out,start+i,1u,usages[i]);
                                    found=1;
                                } else if (have_min&&have_max&&umax>=umin) {
                                    hid_seg_add(out,start,rcount,umin);
                                    found=1;
                                }
                            }
                        } else if (rsize<=8u&&rcount<=255u&&!out->arr_count) {
                            out->arr_off=(uint16_t)start;
                            out->arr_size=(uint8_t)rsize;
                            out->arr_count=(uint8_t)rcount;
                            found=1;
                        }
                        if (found&&!have_target) {
                            have_target=1; target_id=cur_id; target_idx=idx;
                        }
                    }
                    bitpos[idx]=start+bits;
                }
            }
            nusages=0; have_min=have_max=0; umin=umax=0;
        }
    }
    if (!have_target||(!out->nsegs&&!out->arr_count)) return 0;
    out->valid=1;
    out->has_id=(target_id!=0u);
    out->report_id=target_id;
    out->report_bytes=(uint16_t)(((bitpos[target_idx]+7u)/8u)+(target_id?1u:0u));
    return 1;
}

static uint32_t hid_get_bits(const uint8_t *buf,uint32_t len,uint32_t off,uint32_t size)
{
    uint32_t v=0;
    for (uint32_t i=0;i<size;++i) {
        uint32_t bit=off+i,byte=bit>>3;
        if (byte>=len) return 0;
        v|=(uint32_t)((buf[byte]>>(bit&7u))&1u)<<i;
    }
    return v;
}

/* Convert a report in the described layout into the 8-byte boot format
   (modifiers, reserved, up to 6 key codes).  Returns 0 if the report does not
   belong to the keyboard (wrong Report ID / too short). */
static int hid_layout_to_boot(const xhci_kbd_layout_t *l,const uint8_t *buf,
                              uint32_t len,uint8_t out[8])
{
    const uint8_t *d=buf;
    uint32_t n=len,keys=0;
    for (unsigned int i=0;i<8u;++i) out[i]=0;
    if (!l->valid||len<l->report_bytes) return 0;
    if (l->has_id) {
        if (!len||buf[0]!=l->report_id) return 0;
        d=buf+1; n=len-1u;
    }
    for (uint32_t s=0;s<l->nsegs;++s) {
        for (uint32_t i=0;i<l->seg_count[s];++i) {
            uint32_t key;
            if (!hid_get_bits(d,n,(uint32_t)l->seg_off[s]+i,1u)) continue;
            key=(uint32_t)l->seg_base[s]+i;
            if (key>=0xE0u&&key<=0xE7u) out[0]|=(uint8_t)(1u<<(key-0xE0u));
            else if (key>=4u&&key<0xE0u&&keys<6u) out[2u+keys++]=(uint8_t)key;
        }
    }
    for (uint32_t j=0;j<l->arr_count;++j) {
        uint32_t key=hid_get_bits(d,n,(uint32_t)l->arr_off+j*l->arr_size,l->arr_size);
        if (key>=4u&&keys<6u) out[2u+keys++]=(uint8_t)key;
    }
    return 1;
}

static void dbg_hex16(uint16_t v)
{
    static const char digits[]="0123456789ABCDEF";
    char text[5];
    for (unsigned int i=0;i<4u;++i) text[i]=digits[(v>>(12u-4u*i))&0xFu];
    text[4]=0;
    print(text);
}

/* One diagnostic line per enumerated device: VID/PID and every interface as
   number:class/subclass/protocol (HID keyboard = 3/1/1, HID mouse = 3/1/2). */
static void dbg_dump_device(uint8_t root_port,uint16_t vid,uint16_t pid,
                            const uint8_t *cfg,uint16_t total)
{
    uint16_t pos=0;
    if (!g_xhci.diag) return;
    print("[XHCI] dev port "); print_int(root_port);
    print(" VID="); dbg_hex16(vid);
    print(" PID="); dbg_hex16(pid);
    print(" if");
    while (pos+2u<=total) {
        uint8_t len=cfg[pos],type=cfg[pos+1u];
        if (len<2u||(uint32_t)pos+len>total) break;
        if (type==4u&&len>=9u) {
            print(" "); print_int(cfg[pos+2u]);
            print(":"); print_int(cfg[pos+5u]);
            print("/"); print_int(cfg[pos+6u]);
            print("/"); print_int(cfg[pos+7u]);
        } else if (type==5u&&len>=7u) {
            print(" e"); print_int(cfg[pos+2u]);
            print("/"); print_int(cfg[pos+3u]);
            print("/"); print_int((int)((uint16_t)cfg[pos+4u]|((uint16_t)cfg[pos+5u]<<8)));
            print("/"); print_int(cfg[pos+6u]);
        }
        pos=(uint16_t)(pos+len);
    }
    print("\n");
}

static void kbd_ring_init(xhci_trb_t *ring,uint64_t phys)
{
    zero_bytes(ring,XHCI_PAGE_SIZE);
    ring[XHCI_RING_TRBS-1u].d[0]=(uint32_t)phys;
    ring[XHCI_RING_TRBS-1u].d[1]=(uint32_t)(phys>>32);
    ring[XHCI_RING_TRBS-1u].d[2]=0;
    ring[XHCI_RING_TRBS-1u].d[3]=XHCI_TRB_CYCLE|XHCI_TRB_TOGGLE_CYCLE|
        (XHCI_TRB_LINK<<XHCI_TRB_TYPE_SHIFT);
}

static int configure_keyboard_endpoint(void)
{
    uint32_t *in,*slot,*ep;
    volatile uint32_t *slot_out;
    uint64_t cmd_phys;
    uint32_t control,interval,dci=g_xhci.keyboard_endpoint_id;

    if (dci<2u||dci>31u) return 0;
    g_xhci.keyboard_ring=(xhci_trb_t *)dma_alloc(XHCI_PAGE_SIZE,64u,&g_xhci.keyboard_ring_phys);
    g_xhci.report_buffer=(uint8_t *)dma_alloc(XHCI_PAGE_SIZE,XHCI_PAGE_SIZE,&g_xhci.report_buffer_phys);
    if (!g_xhci.keyboard_ring||!g_xhci.report_buffer) return 0;
    mmu_mark_mmio_uncached(g_xhci.keyboard_ring_phys, XHCI_PAGE_SIZE);
    mmu_mark_mmio_uncached(g_xhci.report_buffer_phys, XHCI_PAGE_SIZE);
    kbd_ring_init(g_xhci.keyboard_ring,g_xhci.keyboard_ring_phys);
    g_xhci.keyboard_index=0; g_xhci.keyboard_cycle=1;
    zero_bytes(g_xhci.report_buffer,XHCI_PAGE_SIZE);

    zero_bytes(g_xhci.input_context,XHCI_PAGE_SIZE);
    in=ctx_in(0); slot=ctx_in(1); ep=ctx_in(1u+dci);
    slot_out=ctx_out(0);
    for (uint32_t i=0;i<g_xhci.context_size/4u;++i) slot[i]=slot_out[i];
    /* Context Entries must cover the highest DCI that is added. */
    slot[0]=(slot[0]&~(0x1Fu<<27))|(dci<<27);
    in[0]=0;
    in[1]=(1u<<0)|(1u<<dci);                         /* A0 (slot) + A<DCI> */
    interval=xhci_interval(g_xhci.speed_class,g_xhci.keyboard_interval);
    ep[0]=(interval<<16);
    ep[1]=((uint32_t)g_xhci.keyboard_packet_size<<16)|(3u<<1)|
          (XHCI_USB_EP_INTERRUPT_IN<<3);             /* CErr=3, Interrupt IN */
    ep[2]=(uint32_t)g_xhci.keyboard_ring_phys|1u;    /* DCS = 1 */
    ep[3]=(uint32_t)(g_xhci.keyboard_ring_phys>>32);
    ep[4]=(uint32_t)g_xhci.keyboard_packet_size|
          ((uint32_t)g_xhci.keyboard_packet_size<<16); /* avg TRB len | max ESIT payload */
    __asm__ volatile("sfence" ::: "memory");
    control=(XHCI_TRB_CONFIGURE_EP<<XHCI_TRB_TYPE_SHIFT)|((uint32_t)g_xhci.slot_id<<24);
    cmd_phys=ring_push(g_xhci.command_ring,g_xhci.command_ring_phys,
        &g_xhci.command_index,&g_xhci.command_cycle,
        (uint32_t)g_xhci.input_context_phys,(uint32_t)(g_xhci.input_context_phys>>32),0,control);
    __asm__ volatile("sfence" ::: "memory");
    mmio_write32(g_xhci.doorbells,0,0);
    return wait_command(cmd_phys,0);
}

/* A halted interrupt endpoint (STALL / transaction error / babble) stays dead
   until Reset Endpoint + Set TR Dequeue Pointer.  The ring is cleared and
   restarted from its base. */
static void keyboard_endpoint_recover(xhci_keyboard_t *k)
{
    uint32_t ep=k->endpoint_id,slot=k->slot_id;
    kbd_ring_init(k->ring,k->ring_phys);
    k->index=0; k->cycle=1;
    (void)command_submit(0,0,0,
        (XHCI_TRB_RESET_ENDPOINT<<XHCI_TRB_TYPE_SHIFT)|(ep<<16)|(slot<<24),0);
    (void)command_submit((uint32_t)k->ring_phys|1u,
        (uint32_t)(k->ring_phys>>32),0,
        (XHCI_TRB_SET_TR_DEQUEUE<<XHCI_TRB_TYPE_SHIFT)|(ep<<16)|(slot<<24),0);
    k->pending=0;
}

/* Move the freshly configured keyboard from the "working device" fields into
   the keyboards[] table.  Its slot, contexts and rings stay allocated. */
static int keyboard_stash(void)
{
    xhci_keyboard_t *k;
    if (g_xhci.keyboard_count>=XHCI_MAX_KEYBOARDS) return 0;
    k=&g_xhci.keyboards[g_xhci.keyboard_count++];
    k->ring=g_xhci.keyboard_ring;
    k->ring_phys=g_xhci.keyboard_ring_phys;
    k->report_buffer=g_xhci.report_buffer;
    k->report_phys=g_xhci.report_buffer_phys;
    k->index=g_xhci.keyboard_index;
    k->cycle=g_xhci.keyboard_cycle;
    k->slot_id=g_xhci.slot_id;
    k->endpoint_id=g_xhci.keyboard_endpoint_id;
    k->packet_size=g_xhci.keyboard_packet_size;
    k->report_offset=g_xhci.keyboard_report_offset;
    k->report_id=g_xhci.keyboard_report_id;
    k->pending=0; k->errors=0; k->need_recover=0; k->events=0;
    k->use_layout=g_xhci.keyboard_use_layout;
    k->layout=g_xhci.kbd_layout;
    k->queue_len=k->packet_size;
    if (k->use_layout&&k->layout.report_bytes>k->queue_len&&k->layout.report_bytes<=256u)
        k->queue_len=k->layout.report_bytes;
    k->used=1;
    g_xhci.ready=1;
    return 1;
}

/* Hub / MTT / Number of Ports are only honoured by Configure Endpoint
   (Evaluate Context ignores them). */
static int update_hub_context(uint8_t port_count,uint8_t multi_tt,uint8_t tt_think)
{
    uint32_t *in,*slot;
    volatile uint32_t *slot_out;
    uint64_t cmd_phys;
    uint32_t control;
    if (!port_count||port_count>127u) return 0;
    __asm__ volatile("mfence" ::: "memory");
    zero_bytes(g_xhci.input_context,XHCI_PAGE_SIZE);
    in=ctx_in(0); slot=ctx_in(1); slot_out=ctx_out(0);
    for (uint32_t i=0;i<g_xhci.context_size/4u;++i) slot[i]=slot_out[i];
    slot[0]|=(1u<<26);                                   /* Hub */
    if (multi_tt&&g_xhci.speed_class==XHCI_SPEED_HIGH) slot[0]|=(1u<<25);   /* MTT */
    slot[1]=(slot[1]&0x00FFFFFFu)|((uint32_t)port_count<<24);
    if (g_xhci.speed_class==XHCI_SPEED_HIGH)
        slot[2]=(slot[2]&~(3u<<16))|(((uint32_t)tt_think&3u)<<16);        /* TT Think Time */
    in[0]=0;
    in[1]=1u;                                            /* A0 only */
    __asm__ volatile("sfence" ::: "memory");
    control=(XHCI_TRB_CONFIGURE_EP<<XHCI_TRB_TYPE_SHIFT)|((uint32_t)g_xhci.slot_id<<24);
    cmd_phys=ring_push(g_xhci.command_ring,g_xhci.command_ring_phys,
        &g_xhci.command_index,&g_xhci.command_cycle,
        (uint32_t)g_xhci.input_context_phys,(uint32_t)(g_xhci.input_context_phys>>32),0,control);
    __asm__ volatile("sfence" ::: "memory");
    mmio_write32(g_xhci.doorbells,0,0);
    return wait_command(cmd_phys,0);
}

static int configuration_has_hub(const uint8_t *config,uint16_t length,uint8_t *multi_tt)
{
    uint16_t pos=0;
    int found=0;
    if (multi_tt) *multi_tt=0;
    while (pos+2u<=length) {
        uint8_t n=config[pos],type=config[pos+1u];
        if (n<2u||(uint32_t)pos+n>length) return 0;
        if (type==4u&&n>=9u&&config[pos+5u]==9u) {
            found=1;
            if (multi_tt&&config[pos+7u]==2u) *multi_tt=1;
        }
        pos=(uint16_t)(pos+n);
    }
    return found;
}

static void hub_delay(uint8_t units_2ms)
{
    uint32_t units=units_2ms?units_2ms:1u,ticks=units*16u;
    if (g_xhci.runtime) {
        uint32_t start=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
        for (uint32_t s=0;s<XHCI_MFINDEX_SPIN_LIMIT;++s) {
            uint32_t now=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
            if (((now-start)&XHCI_MFINDEX_MASK)>=ticks) return;
            xhci_pause();
        }
    }
    for (uint32_t n=0;n<units;++n)
        for (volatile uint32_t i=0;i<300000u;++i) xhci_pause();
}

static int hub_get_port_status(uint8_t port,uint16_t *status_out,uint16_t *change_out)
{
    uint8_t *b=g_xhci.control_buffer;
    b[0]=b[1]=b[2]=b[3]=0;
    if (!control_transfer(0xA3u,0u,0u,port,b,4u)) return 0;
    if (status_out) *status_out=(uint16_t)b[0]|((uint16_t)b[1]<<8);
    if (change_out) *change_out=(uint16_t)b[2]|((uint16_t)b[3]<<8);
    return 1;
}
static int hub_set_port_feature(uint8_t port,uint16_t feature)
{ return control_transfer(0x23u,3u,feature,port,0,0); }
static void hub_clear_port_feature(uint8_t port,uint16_t feature)
{ (void)control_transfer(0x23u,1u,feature,port,0,0); }

static int hub_reset_downstream_port(uint8_t port,uint8_t hub_speed,
    uint8_t power_good,uint8_t power_switching,uint8_t *device_speed_out)
{
    uint16_t ps=0,pc=0;
    uint8_t hub_class=speed_class_for(g_xhci.protocol_major,hub_speed);
    int ss_hub=hub_class>=XHCI_SPEED_SUPER;
    if (!hub_get_port_status(port,&ps,&pc)||!(ps&1u)) return 0;
    if (power_switching!=2u&&power_switching!=3u&&
        !(ps&(1u<<(ss_hub?9u:8u)))) {
        if (!hub_set_port_feature(port,8u)) return 0;
        hub_delay(power_good);
    }
    if (!hub_set_port_feature(port,ss_hub?28u:4u)) return 0;
    for (uint32_t a=0;a<XHCI_HUB_WAIT_LIMIT;++a) {
        if (!hub_get_port_status(port,&ps,&pc)||!(ps&1u)) return 0;
        if (!ss_hub) {
            if (!(ps&(1u<<4))&&(ps&(1u<<1))) {
                uint8_t cls=(ps&(1u<<9))?XHCI_SPEED_LOW:
                    ((ps&(1u<<10))?XHCI_SPEED_HIGH:XHCI_SPEED_FULL);
                *device_speed_out=speed_id_for(2u,cls);
                if (!*device_speed_out) return 0;
                hub_clear_port_feature(port,16u);
                hub_clear_port_feature(port,20u);
                return 1;
            }
        } else {
            uint8_t code=(uint8_t)((ps>>10)&7u);
            uint8_t cls=(code==0u)?XHCI_SPEED_SUPER:
                ((code==1u)?XHCI_SPEED_SUPER_PLUS:0u);
            if ((ps&(1u<<1))&&(((ps>>5)&0x0Fu)==0u)&&cls) {
                *device_speed_out=speed_id_for(3u,cls);
                if (!*device_speed_out) return 0;
                hub_clear_port_feature(port,16u);
                hub_clear_port_feature(port,29u);
                return 1;
            }
        }
        for (volatile uint32_t d=0;d<100000u;++d) xhci_pause();
    }
    return 0;
}

static int enumerate_usb_device(uint8_t root_port,uint8_t speed,
    uint32_t route_string,uint8_t parent_hub_slot,uint8_t parent_hub_port,
    uint8_t parent_hub_speed,uint8_t parent_hub_think,
    uint8_t upstream_tt_slot,uint8_t upstream_tt_port,
    uint8_t upstream_tt_think,uint8_t hub_depth);

static int enumerate_hub_children(uint8_t root_port,uint32_t route_string,
    uint8_t hub_depth,uint8_t port_count,uint8_t power_good,
    uint8_t power_switching,uint8_t tt_think,uint8_t upstream_tt_slot,
    uint8_t upstream_tt_port,uint8_t upstream_tt_think)
{
    xhci_device_snapshot_t hub;
    int found=0;
    if (!port_count||hub_depth>=XHCI_HUB_MAX_DEPTH) return 0;
    save_device_snapshot(&hub);
    for (uint16_t port=1u;port<=port_count;++port) {
        uint16_t ps=0,pc=0;
        uint8_t child_speed=0;
        restore_device_snapshot(&hub);
        if (!hub_get_port_status((uint8_t)port,&ps,&pc)) {
            ++g_xhci.hub_status_errors; save_device_snapshot(&hub); continue;
        }
        if (!(ps&1u)) { save_device_snapshot(&hub); continue; }
        ++g_xhci.hub_connected_ports;
        if (!hub_reset_downstream_port((uint8_t)port,hub.speed,power_good,
                                      power_switching,&child_speed)) {
            save_device_snapshot(&hub); continue;
        }
        ++g_xhci.hub_reset_ports;
        save_device_snapshot(&hub);
        {
            uint8_t tt_slot=upstream_tt_slot,tt_port=upstream_tt_port,tt_time=upstream_tt_think;
            if (hub.speed_class==XHCI_SPEED_HIGH) {
                tt_slot=hub.slot_id; tt_port=(uint8_t)port; tt_time=tt_think;
            }
        if (enumerate_usb_device(root_port,child_speed,
            route_string|((uint32_t)port<<(4u*(hub_depth+1u))),
            hub.slot_id,(uint8_t)port,hub.speed,tt_think,
            tt_slot,tt_port,tt_time,(uint8_t)(hub_depth+1u))) found=1;
        }
    }
    restore_device_snapshot(&hub);
    if (found) return 1;     /* the hub must stay configured for its keyboard(s) */
    disable_active_slot();
    return 0;
}

static int enumerate_usb_device(uint8_t root_port,uint8_t speed,
    uint32_t route_string,uint8_t parent_hub_slot,uint8_t parent_hub_port,
    uint8_t parent_hub_speed,uint8_t parent_hub_think,
    uint8_t upstream_tt_slot,uint8_t upstream_tt_port,
    uint8_t upstream_tt_think,uint8_t hub_depth)
{
    uint8_t *device_desc,*config_header;
    uint8_t device_class=0,config_value=0,iface=0,endpoint_id=0;
    uint8_t packet_size=0,interval=0,boot_protocol=0;
    uint8_t hub_multi_tt=0;
    uint8_t hub_desc_type,hub_ports,hub_power_good,hub_tt_think=0;
    uint8_t speed_class;
    int config_is_hub=0;
    uint16_t report_length=0,total_length,hub_chars,ep0_packet;
    uint16_t vid=0,pid=0;

    (void)parent_hub_think; (void)upstream_tt_think;
    clear_active_device_fields();
    g_xhci.port_id=root_port;
    g_xhci.speed=speed;
    if (root_port==0u||root_port>g_xhci.max_ports) goto fail_no_slot;
    g_xhci.protocol_major=g_xhci.port_protocol[root_port];
    speed_class=speed_class_for(g_xhci.protocol_major,speed);
    if (!speed_class) {
        dbg_value("[XHCI] Unknown Speed ID: ",speed);
        g_xhci.status="xHCI: PORTSC returned an unknown Protocol Speed ID";
        goto fail_no_slot;
    }
    g_xhci.speed_class=speed_class;

    g_xhci.status="xHCI: enabling USB device slot";
    if (!command_submit(0,0,0,XHCI_TRB_ENABLE_SLOT<<XHCI_TRB_TYPE_SHIFT,&g_xhci.slot_id)) {
        g_xhci.status="xHCI: Enable Slot command failed";
        goto fail_no_slot;
    }
    if (!g_xhci.slot_id||g_xhci.slot_id>g_xhci.max_slots) {
        g_xhci.status="xHCI: Enable Slot returned an invalid slot id";
        goto fail;
    }
    if (!allocate_device_contexts()) {
        g_xhci.status="xHCI: DMA allocation for USB device failed";
        goto fail;
    }
    g_xhci.dcbaa[g_xhci.slot_id]=g_xhci.device_context_phys;
    __asm__ volatile("sfence" ::: "memory");

    ep0_packet=(speed_class==XHCI_SPEED_HIGH)?64u:
               (speed_class>=XHCI_SPEED_SUPER)?512u:8u;

    /* 1. Address Device with BSR=1: slot enters Default state, no SET_ADDRESS
          is sent yet, so the first 8 bytes of the device descriptor can be
          read to learn the real EP0 max packet size. */
    build_address_input_context(root_port,speed,speed_class,route_string,
        parent_hub_slot,parent_hub_port,parent_hub_speed,
        upstream_tt_slot,upstream_tt_port,ep0_packet);
    g_xhci.status="xHCI: Address Device (BSR=1)";
    if (!address_device_cmd(1)) {
        g_xhci.status="xHCI: Address Device (BSR=1) failed";
        goto fail;
    }

    device_desc=g_xhci.control_buffer;
    config_header=g_xhci.control_buffer;
    g_xhci.status="xHCI: reading first 8 bytes of USB device descriptor";
    if (!control_transfer(0x80u,6u,0x0100u,0u,device_desc,8u)) {
        g_xhci.status="xHCI: GET_DEVICE_DESCRIPTOR(8) failed";
        goto fail;
    }
    if (speed_class>=XHCI_SPEED_SUPER) {
        if (device_desc[7]==9u) ep0_packet=512u;       /* 2^9 */
    } else if (speed_class!=XHCI_SPEED_HIGH) {
        uint8_t m=device_desc[7];
        if (m==8u||m==16u||m==32u||m==64u) ep0_packet=m;
    }

    /* 2. Address Device with BSR=0: SET_ADDRESS is sent.  The Input Context
          is rebuilt with the real EP0 packet size and the current EP0 ring
          position. */
    build_address_input_context(root_port,speed,speed_class,route_string,
        parent_hub_slot,parent_hub_port,parent_hub_speed,
        upstream_tt_slot,upstream_tt_port,ep0_packet);
    g_xhci.status="xHCI: Address Device (BSR=0)";
    if (!address_device_cmd(0)) {
        g_xhci.status="xHCI: Address Device (BSR=0) failed";
        goto fail;
    }
    (void)xhci_wait_microframes(160);                  /* SET_ADDRESS recovery */

    g_xhci.status="xHCI: reading USB device descriptor";
    if (!control_transfer(0x80u,6u,0x0100u,0u,device_desc,18u)) {
        g_xhci.status="xHCI: GET_DEVICE_DESCRIPTOR failed";
        goto fail;
    }
    device_class=device_desc[4];
    vid=(uint16_t)device_desc[8]|((uint16_t)device_desc[9]<<8);
    pid=(uint16_t)device_desc[10]|((uint16_t)device_desc[11]<<8);

    g_xhci.status="xHCI: reading USB configuration header";
    if (!control_transfer(0x80u,6u,0x0200u,0u,config_header,9u)) {
        g_xhci.status="xHCI: GET_CONFIGURATION header failed";
        goto fail;
    }
    total_length=(uint16_t)config_header[2]|((uint16_t)config_header[3]<<8);
    config_value=config_header[5];
    if (total_length<9u||total_length>XHCI_CONFIG_LIMIT||!config_value) {
        g_xhci.status="xHCI: invalid USB configuration descriptor";
        goto fail;
    }
    g_xhci.status="xHCI: reading USB configuration";
    if (!control_transfer(0x80u,6u,0x0200u,0u,g_xhci.config_buffer,total_length)) {
        g_xhci.status="xHCI: GET_CONFIGURATION failed";
        goto fail;
    }

    dbg_dump_device(root_port,vid,pid,g_xhci.config_buffer,total_length);
    config_is_hub=configuration_has_hub(g_xhci.config_buffer,total_length,&hub_multi_tt);
    if (device_class==9u||config_is_hub) {
        uint8_t *hub_desc=g_xhci.control_buffer;
        uint16_t desc_len=(speed_class>=XHCI_SPEED_SUPER)?12u:7u;
        ++g_xhci.hubs_seen;
        g_xhci.status="xHCI: USB hub detected; reading hub descriptor";
        for (uint16_t i=0;i<12u;++i) hub_desc[i]=0;
        if (hub_depth>=XHCI_HUB_MAX_DEPTH) {
            g_xhci.status="xHCI: USB hub nesting limit reached";
            goto fail;
        }
        if (!control_transfer(0x00u,9u,config_value,0u,0,0)) {
            g_xhci.status="xHCI: SET_CONFIGURATION failed for hub";
            goto fail;
        }
        hub_desc_type=(speed_class>=XHCI_SPEED_SUPER)?0x2Au:0x29u;
        if (!control_transfer(0xA0u,6u,(uint16_t)((uint16_t)hub_desc_type<<8),
                              0u,hub_desc,desc_len)) {
            g_xhci.status="xHCI: GET_DESCRIPTOR(HUB) failed";
            goto fail;
        }
        hub_ports=hub_desc[2];
        hub_power_good=hub_desc[5];
        hub_chars=(uint16_t)hub_desc[3]|((uint16_t)hub_desc[4]<<8);
        if (!hub_ports||hub_ports>15u) {
            g_xhci.status="xHCI: hub port count outside route-string range";
            goto fail;
        }
        if (speed_class==XHCI_SPEED_HIGH)
            hub_tt_think=(uint8_t)((hub_chars>>5)&3u);
        if (!update_hub_context(hub_ports,hub_multi_tt,hub_tt_think)) {
            g_xhci.status="xHCI: updating hub Slot Context failed";
            goto fail;
        }
        {
            uint8_t switching=(uint8_t)(hub_chars&3u);
            if (switching!=2u&&switching!=3u) {
                for (uint16_t p=1u;p<=hub_ports;++p)
                    (void)hub_set_port_feature((uint8_t)p,8u);
                hub_delay(hub_power_good);
            }
        }
        g_xhci.status="xHCI: scanning downstream hub ports";
        if (enumerate_hub_children(root_port,route_string,hub_depth,hub_ports,
                hub_power_good,(uint8_t)(hub_chars&3u),hub_tt_think,
                upstream_tt_slot,upstream_tt_port,upstream_tt_think)) return 1;
        g_xhci.status="xHCI: hub scanned; no supported keyboard below it";
        return 0;
    }

    g_xhci.status="xHCI: searching HID keyboard interface";
    if (!find_keyboard_interface(g_xhci.config_buffer,total_length,&config_value,
            &iface,&endpoint_id,&packet_size,&interval,&boot_protocol,&report_length)) {
        g_xhci.status="xHCI: USB device has no supported HID keyboard interface";
        goto fail;
    }
    if (g_xhci.diag) {
        print("[XHCI]   keyboard iface "); print_int(iface);
        print(" dci "); print_int(endpoint_id);
        print(" maxp "); print_int(packet_size);
        print(" boot "); print_int(boot_protocol); print("\n");
    }
    g_xhci.keyboard_interface=iface;
    g_xhci.keyboard_endpoint_id=endpoint_id;
    g_xhci.keyboard_packet_size=packet_size;
    g_xhci.keyboard_interval=interval;
    g_xhci.keyboard_boot_protocol=boot_protocol;
    g_xhci.keyboard_report_descriptor_length=report_length;
    g_xhci.keyboard_report_id=g_xhci.keyboard_report_offset=0;

    /* 3. Configure Endpoint BEFORE SET_CONFIGURATION (same order as Linux:
          the host controller is set up first, then the device is told to
          switch to the configuration). */
    g_xhci.status="xHCI: configuring keyboard interrupt endpoint";
    if (!configure_keyboard_endpoint()) {
        g_xhci.status="xHCI: Configure Endpoint failed for keyboard";
        goto fail;
    }
    g_xhci.status="xHCI: SET_CONFIGURATION";
    if (!control_transfer(0x00u,9u,config_value,0u,0,0)) {
        g_xhci.status="xHCI: SET_CONFIGURATION failed for HID device";
        goto fail;
    }
    if (boot_protocol) {
        if (!control_transfer(0x21u,0x0Bu,0u,iface,0,0)) {
            g_xhci.status="xHCI: SET_PROTOCOL(Boot) failed";
            goto fail;
        }
    } else {
        if (!report_length||report_length>XHCI_CONFIG_LIMIT) {
            g_xhci.status="xHCI: HID report descriptor length unsupported";
            goto fail;
        }
        if (!control_transfer(0x81u,6u,0x2200u,iface,g_xhci.config_buffer,report_length)) {
            g_xhci.status="xHCI: GET_DESCRIPTOR(HID Report) failed";
            goto fail;
        }
        if (!hid_parse_keyboard_layout(g_xhci.config_buffer,report_length,
                                       &g_xhci.kbd_layout)) {
            g_xhci.status="xHCI: HID report descriptor has no usable keyboard layout";
            goto fail;
        }
        g_xhci.keyboard_use_layout=1;
        g_xhci.keyboard_report_id=g_xhci.kbd_layout.report_id;
        g_xhci.keyboard_report_offset=0;
        if (g_xhci.diag) {
            print("[XHCI]   report layout: id "); print_int(g_xhci.kbd_layout.report_id);
            print(" bitmaps "); print_int(g_xhci.kbd_layout.nsegs);
            print(" keys "); print_int(g_xhci.kbd_layout.arr_count);
            print(" bytes "); print_int(g_xhci.kbd_layout.report_bytes);
            print("\n");
        }
    }
    (void)control_transfer(0x21u,0x0Au,0u,iface,0,0);   /* SET_IDLE(0); may STALL */
    g_xhci.keyboard_pending=0;
    if (!keyboard_stash()) {
        g_xhci.status="xHCI: too many USB keyboards";
        goto fail;
    }
    if (parent_hub_slot) g_xhci.keyboard_via_hub=1;
    /* Slot, contexts and rings now belong to the stashed keyboard: forget
       the working pointers WITHOUT freeing them. */
    clear_active_device_fields();
    return 1;

fail:
    g_xhci.last_error = g_xhci.status;
    if (g_xhci.diag) { print("[XHCI]   device rejected: "); print(g_xhci.status); print("\n"); }
    disable_active_slot();
    return 0;
fail_no_slot:
    clear_active_device_fields();
    return 0;
}

static int xhci_wait_microframes(uint32_t ticks)
{
    uint32_t start;
    if (!g_xhci.runtime||!ticks) return ticks==0u;
    start=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
    for (uint32_t i=0;i<XHCI_MFINDEX_SPIN_LIMIT;++i) {
        uint32_t now=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
        if (((now-start)&XHCI_MFINDEX_MASK)>=ticks) return 1;
        xhci_pause();
    }
    return 0;
}
static int xhci_any_root_port_connected(void)
{
    for (uint32_t p=1;p<=g_xhci.max_ports;++p) {
        uint32_t off=XHCI_PORTSC_BASE+(p-1u)*XHCI_PORTSC_STRIDE;
        if (mmio_read32(g_xhci.op,off)&XHCI_PORT_CCS) return 1;
    }
    return 0;
}
static int xhci_wait_for_root_connection(uint32_t max_ticks)
{
    uint32_t start,last=0;
    if (xhci_any_root_port_connected()) return 1;
    start=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
    for (uint32_t s=0;s<XHCI_MFINDEX_SPIN_LIMIT;++s) {
        uint32_t now=mmio_read32(g_xhci.runtime,0u)&XHCI_MFINDEX_MASK;
        uint32_t elapsed=(now-start)&XHCI_MFINDEX_MASK;
        if (elapsed-last>=16u) {
            if (xhci_any_root_port_connected()) return 1;
            last=elapsed;
        }
        if (elapsed>=max_ticks) return xhci_any_root_port_connected();
        xhci_pause();
    }
    return xhci_any_root_port_connected();
}
static void xhci_port_clear_changes(uint32_t off)
{
    uint32_t v=mmio_read32(g_xhci.op,off);
    if (!(v&XHCI_PORT_CHANGE_BITS)) return;
    mmio_write32(g_xhci.op,off,(v&XHCI_PORT_PP)|XHCI_PORT_CHANGE_BITS);
}



static void xhci_prepare_root_ports(uint32_t connect_wait)
{
    uint32_t powered=0, failed=0;
    for (uint32_t p=1;p<=g_xhci.max_ports;++p) {
        uint32_t off=XHCI_PORTSC_BASE+(p-1u)*XHCI_PORTSC_STRIDE;
        uint32_t v=mmio_read32(g_xhci.op,off);

        if (!(v&XHCI_PORT_PP)) {
            mmio_write32(g_xhci.op,off,(v&XHCI_PORT_CHANGE_BITS)|XHCI_PORT_PP);
            ++powered;
            (void)xhci_wait_microframes(160u);
            v=mmio_read32(g_xhci.op,off);
            if (!(v&XHCI_PORT_PP)) {
                mmio_write32(g_xhci.op,off,(v&XHCI_PORT_CHANGE_BITS));
                (void)xhci_wait_microframes(160u);
                mmio_write32(g_xhci.op,off,(v&XHCI_PORT_CHANGE_BITS)|XHCI_PORT_PP);
                (void)xhci_wait_microframes(160u);
                v=mmio_read32(g_xhci.op,off);
                if (!(v&XHCI_PORT_PP)) ++failed;
            }
        }
        xhci_port_clear_changes(off);
    }
    if (powered) (void)xhci_wait_microframes(1600u);
    g_xhci.status="xHCI: waiting for root-port connection detection";
    (void)xhci_wait_for_root_connection(connect_wait);
}

static int xhci_scan_root_ports(uint32_t connect_wait)
{
    g_xhci.root_connected_ports=0;
    g_xhci.root_reset_ports=0;
    xhci_prepare_root_ports(connect_wait);
    g_xhci.status="xHCI started; scanning protocol-defined root ports";
    for (uint32_t p=1;p<=g_xhci.max_ports&&p<=255u;++p) {
        uint32_t off=XHCI_PORTSC_BASE+(p-1u)*XHCI_PORTSC_STRIDE;
        uint32_t portsc=mmio_read32(g_xhci.op,off);
        uint8_t proto=g_xhci.port_protocol[p];
        uint8_t speed=0;
        if (!(portsc&XHCI_PORT_CCS)) continue;
        ++g_xhci.root_connected_ports;
        if (g_xhci.diag) {
            print("[XHCI] connected root port "); print_int(p);
            print(" (protocol USB"); print_int(proto); print(")\n");
            dbg_hex("[XHCI]   PORTSC=",portsc);
        }

        if (proto!=2u&&proto!=3u) {
            /* Some firmware does not publish a Supported Protocol entry for
               every port.  Try a USB2 reset first, then a USB3 warm reset. */
            if (port_reset((uint8_t)p,2u,&speed)) proto=2u;
            else if (port_reset((uint8_t)p,3u,&speed)) proto=3u;
            else continue;
        } else {
            g_xhci.protocol_major=proto;
            g_xhci.status=(proto==2u)?
                "xHCI: USB2 root port connected; issuing Port Reset":
                "xHCI: USB3 root port connected; issuing Warm Port Reset";
            if (!port_reset((uint8_t)p,proto,&speed)) {
                /* Ports that were just powered sometimes need a second try. */
                (void)xhci_wait_microframes(1600u);
                xhci_port_clear_changes(off);
                if (!port_reset((uint8_t)p,proto,&speed)) continue;
            }
        }
        if (speed >= 1u && speed <= 3u) {
            proto = 2u;
        }

        g_xhci.port_protocol[p]=proto;
        g_xhci.protocol_major=proto;
        ++g_xhci.root_reset_ports;
        g_xhci.status="xHCI: reset complete; speed valid; enumerating device";

        g_xhci.status="xHCI: reset complete; speed valid; enumerating device";
        
        (void)xhci_wait_microframes(400);
                    
        if (!enumerate_usb_device((uint8_t)p,speed,0u,0u,0u,0u,0u,0u,0u,0u,0u))
            xhci_port_clear_changes(off);
        /* keep scanning: another port may hold the real keyboard */
    }
    if (g_xhci.keyboard_count) {
        g_xhci.status=g_xhci.keyboard_via_hub?
            "USB HID keyboard ready behind USB hub":
            "USB HID keyboard ready on root port";
        return 1;
    }
    if (!g_xhci.root_connected_ports)
        g_xhci.status="xHCI running; no connected devices on protocol-defined root ports";
    else if (!g_xhci.root_reset_ports)
        g_xhci.status="xHCI: connected root port(s) were found but reset failed";
    else if (g_xhci.last_error)
        g_xhci.status = g_xhci.last_error;
    else if (!g_xhci.hubs_seen)
        g_xhci.status="xHCI: root devices enumerated, no supported HID keyboard found";
    else if (!g_xhci.hub_connected_ports&&g_xhci.hub_status_errors)
        g_xhci.status="xHCI: hub found, but GET_PORT_STATUS requests failed";
    else if (!g_xhci.hub_connected_ports)
        g_xhci.status="xHCI: hub found, but no downstream device is connected";
    else if (!g_xhci.hub_reset_ports)
        g_xhci.status="xHCI: hub downstream port reset failed";
    else
        g_xhci.status="xHCI: hub tree scanned, no supported HID keyboard found";
    return 0;
}

static int controller_try_device(const pci_dev_t *dev)
{
    uint64_t bar=0;
    int is64=0;
    if (!pci_enable_mem_busmaster(dev)) {
        g_xhci.status="xHCI PCI: could not enable memory/bus mastering";
        return 0;
    }
    if (!pci_set_power_d0(dev)) {
        g_xhci.status="xHCI PCI: failed to force D0 power state";
        return 0;
    }
    for (unsigned int i=0;i<6u;++i) {
        if (!pci_get_mmio_bar(dev,i,&bar,&is64)) continue;
        if (is64) ++i;
        break;
    }
    if (!bar||bar>=0x100000000ull) {
        g_xhci.status="xHCI PCI: MMIO BAR missing or above 4 GiB";
        return 0;
    }
    mmu_mark_mmio_uncached(bar,XHCI_MMIO_MAP_SIZE);

    {
        volatile uint8_t *probe=(volatile uint8_t *)(uintptr_t)bar;
        uint32_t caplen=*(volatile uint32_t *)(probe+XHCI_CAP_CAPLENGTH);
        if ((caplen&0xFFu)<0x20u||(caplen&0xFFu)==0xFFu) {
            g_xhci.status="xHCI PCI: BAR not readable (MMIO mapping missing)";
            return 0;
        }
    }

    zero_bytes(&g_xhci,sizeof(g_xhci));
    g_xhci.initialized=1;
    g_xhci.diag=1;
    if (!controller_start((volatile uint8_t *)(uintptr_t)bar)) return 0;
    g_xhci.controller_running=1;
    g_xhci.rescan_countdown=XHCI_RESCAN_INTERVAL;
    g_xhci.full_retry_countdown=XHCI_FULL_RETRY_INTERVAL;

    {
        uint32_t sts=mmio_read32(g_xhci.op,XHCI_USBSTS);
        if (sts&(1u<<2)) {   /* HSE */
            g_xhci.status="xHCI: Host System Error right after RUN";
            return 0;
        }
        if (sts&XHCI_STS_HALTED) {
            g_xhci.status="xHCI: controller halted right after RUN";
            return 0;
        }
    }

    return xhci_scan_root_ports(XHCI_ROOT_CONNECT_WAIT);
}

int xhci_init(void)
{
    pci_dev_t controllers[XHCI_MAX_CONTROLLERS];
    int count;
    if (g_xhci.ready) return 1;
    if (!pmm_is_ready()) {
        g_xhci.status="PMM is not initialized; xHCI DMA unavailable";
        return 0;
    }
    if (g_xhci.initialized) {
        if (g_xhci.controller_running) return 0;
        if (g_xhci.full_retry_countdown) { --g_xhci.full_retry_countdown; return 0; }
        g_xhci.initialized=0;
    }
    g_xhci.initialized=1;
    g_xhci.full_retry_countdown=XHCI_FULL_RETRY_INTERVAL;
    count=pci_scan_xhci(controllers,XHCI_MAX_CONTROLLERS);
    if (count<=0) {
        g_xhci.status="No PCI xHCI controller found";
        return 0;
    }
    for (int i=0;i<count;++i) {
        int ok;
        print("[XHCI] Trying controller at PCI ");
        print_int(controllers[i].bus);   print(":");
        print_int(controllers[i].slot);  print(".");
        print_int(controllers[i].func);  print("\n");

        ok=controller_try_device(&controllers[i]);
        g_xhci.diag=0;
        if (ok) return 1;
    }
    if (!g_xhci.status) g_xhci.status="xHCI controllers found but initialization failed";
    return 0;
}

static xhci_keyboard_t *keyboard_find(uint8_t slot,uint8_t ep)
{
    for (uint32_t i=0;i<g_xhci.keyboard_count;++i) {
        xhci_keyboard_t *k=&g_xhci.keyboards[i];
        if (k->used && k->slot_id==slot && k->endpoint_id==ep) return k;
    }
    return 0;
}

static void keyboard_handle_event(const xhci_trb_t *ev)
{
    xhci_keyboard_t *k;
    uint8_t slot,ep,cc;
    uint32_t residual;
    if (trb_type(ev->d[3])!=XHCI_EVENT_TRANSFER) return;
    slot=(uint8_t)(ev->d[3]>>24);
    ep=(uint8_t)((ev->d[3]>>16)&0x1Fu);
    cc=(uint8_t)(ev->d[2]>>24);
    residual=ev->d[2]&0x00FFFFFFu;
    k=keyboard_find(slot,ep);
    if (!k) return;
    ++k->events;
    ++g_xhci.dbg_xfer_events;
    g_xhci.last_xfer_cc=cc;
    if (cc==XHCI_CC_SUCCESS||cc==XHCI_CC_SHORT_PACKET) {
        uint32_t received=(residual<=k->queue_len)?
            (uint32_t)k->queue_len-residual:0u;
        uint8_t report[8];
        volatile uint8_t *src=(volatile uint8_t *)k->report_buffer;
        k->errors=0;
        __asm__ volatile("mfence" ::: "memory");
        if (k->use_layout) {
            uint8_t raw[256];
            if (received>sizeof(raw)) received=sizeof(raw);
            for (uint32_t i=0;i<received;++i) raw[i]=src[i];
            if (hid_layout_to_boot(&k->layout,raw,received,report)) {
                ++g_xhci.dbg_reports;
                keyboard_usb_hid_submit_boot_report(report);
            }
        } else if (received>=8u) {
            for (unsigned int i=0;i<8u;++i) report[i]=src[i];
            ++g_xhci.dbg_reports;
            keyboard_usb_hid_submit_boot_report(report);
        }
    } else {
        ++k->errors;
        ++g_xhci.dbg_xfer_errors;
        k->need_recover=1;
    }
    k->pending=0;
}

void xhci_poll(void)
{
    uint32_t alive=0;
    if (!g_xhci.ready) {
        if (!g_xhci.controller_running) return;
        if (g_xhci.rescan_countdown) { --g_xhci.rescan_countdown; return; }
        g_xhci.rescan_countdown=XHCI_RESCAN_INTERVAL;
        {
            uint32_t wait = (g_xhci.hub_connected_ports==0 && g_xhci.hubs_seen==0)
                                ? 24000u : 1600u;
            (void)xhci_scan_root_ports(wait);
        }
        return;
    }
    if (g_xhci.dbg_polls!=0xFFFFFFFFu) ++g_xhci.dbg_polls;
    for (;;) {
        xhci_trb_t ev;
        if (!event_pop(&ev)) break;
        keyboard_handle_event(&ev);
    }
    for (uint32_t i=0;i<g_xhci.keyboard_count;++i) {
        xhci_keyboard_t *k=&g_xhci.keyboards[i];
        if (!k->used) continue;
        if (k->need_recover) {
            k->need_recover=0;
            if (k->errors>8u) {
                /* Dead endpoint (unplugged?): stop polling it. */
                k->used=0; k->pending=0;
                (void)command_submit(0,0,0,
                    (XHCI_TRB_DISABLE_SLOT<<XHCI_TRB_TYPE_SHIFT)|((uint32_t)k->slot_id<<24),0);
                continue;
            }
            keyboard_endpoint_recover(k);
        }
        ++alive;
        if (!k->pending) {
            (void)ring_push(k->ring,k->ring_phys,&k->index,&k->cycle,
                (uint32_t)k->report_phys,(uint32_t)(k->report_phys>>32),k->queue_len,
                XHCI_TRB_IOC|XHCI_TRB_ISP|(XHCI_TRB_NORMAL<<XHCI_TRB_TYPE_SHIFT));
            __asm__ volatile("sfence" ::: "memory");
            mmio_write32(g_xhci.doorbells,(uint32_t)k->slot_id*4u,k->endpoint_id);
            k->pending=1;
        }
    }
    if (!alive) {
        g_xhci.status="xHCI: all keyboard endpoints failed; rescanning ports";
        g_xhci.ready=0;
        g_xhci.keyboard_count=0;
        g_xhci.rescan_countdown=1u;
    }
}

int xhci_keyboard_ready(void) { return g_xhci.ready!=0; }
static char g_status_buf[384];
static void sb_text(size_t *pos,const char *s)
{
    while (*s && *pos+1u<sizeof(g_status_buf)) g_status_buf[(*pos)++]=*s++;
    g_status_buf[*pos]=0;
}
static void sb_uint(size_t *pos,uint32_t v)
{
    char tmp[11];
    unsigned int n=0;
    if (!v) tmp[n++]='0';
    while (v&&n<sizeof(tmp)) { tmp[n++]=(char)('0'+(v%10u)); v/=10u; }
    while (n&&*pos+1u<sizeof(g_status_buf)) g_status_buf[(*pos)++]=tmp[--n];
    g_status_buf[*pos]=0;
}
const char *xhci_status_string(void)
{
    size_t pos=0;
    const char *s=g_xhci.status?g_xhci.status:"xHCI not initialized";
    if (!g_xhci.controller_running) return s;
    g_status_buf[0]=0;
    sb_text(&pos,s);
    sb_text(&pos," {kbd=");  sb_uint(&pos,g_xhci.keyboard_count);
    for (uint32_t i=0;i<g_xhci.keyboard_count;++i) {
        sb_text(&pos," [slot"); sb_uint(&pos,g_xhci.keyboards[i].slot_id);
        sb_text(&pos," ep");    sb_uint(&pos,g_xhci.keyboards[i].endpoint_id);
        sb_text(&pos," ev");    sb_uint(&pos,g_xhci.keyboards[i].events);
        sb_text(&pos,"]");
    }
    sb_text(&pos," polls="); sb_uint(&pos,g_xhci.dbg_polls);
    sb_text(&pos," ev=");    sb_uint(&pos,g_xhci.dbg_xfer_events);
    sb_text(&pos," err=");   sb_uint(&pos,g_xhci.dbg_xfer_errors);
    sb_text(&pos," rep=");   sb_uint(&pos,g_xhci.dbg_reports);
    sb_text(&pos," cc=");    sb_uint(&pos,g_xhci.last_xfer_cc);
    sb_text(&pos," cmdcc="); sb_uint(&pos,g_xhci.last_cmd_cc);
    sb_text(&pos,"}");
    return g_status_buf;
}
int xhci_bios_released(void) { return g_xhci.bios_released; }