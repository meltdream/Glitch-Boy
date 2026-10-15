/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <new>
#include <freertos/FreeRTOS.h>
using namespace std;

#include "hci_transport.h"
#include "hci_server.h"
#include "../diag_config.h"

// PAIR_DEBUG keeps pairing traces separate from packet dumps.
#ifndef PAIR_DEBUG
#define PAIR_DEBUG 0
#endif
#if !DIAG_FLAGS_ENABLE
#undef PAIR_DEBUG
#define PAIR_DEBUG 0
#endif
// BT_TRACE=1 (build flag) enables only the pairing/connection trace.
#ifndef BT_TRACE
#define BT_TRACE 0
#endif
#if (SERIAL_DIAG_LOGS && PAIR_DEBUG) || BT_TRACE
#define PAIR_LOG(...) printf(__VA_ARGS__)
#else
#define PAIR_LOG(...) (void)0
#endif

// Limit each poll so Bluetooth bursts do not monopolize core 1.
#ifndef HCI_UPDATE_TX_BUDGET
#define HCI_UPDATE_TX_BUDGET 8
#endif
#ifndef HCI_UPDATE_RX_BUDGET
#define HCI_UPDATE_RX_BUDGET 24
#endif
#ifndef HCI_WII_DUPLICATE_PASS_INTERVAL
#define HCI_WII_DUPLICATE_PASS_INTERVAL 128
#endif
#ifndef HCI_WII_REPORT_MODE_QUENCH_THRESHOLD
#define HCI_WII_REPORT_MODE_QUENCH_THRESHOLD 32
#endif

static bool note_wii_duplicate(uint16_t& run, bool* quench)
{
    if (quench)
        *quench = false;
    if (run < 0xFFFF)
        ++run;
    if (quench && HCI_WII_REPORT_MODE_QUENCH_THRESHOLD > 0 &&
        run == HCI_WII_REPORT_MODE_QUENCH_THRESHOLD) {
        *quench = true;
    }
    if (HCI_WII_DUPLICATE_PASS_INTERVAL <= 0)
        return false;
    if (run >= HCI_WII_DUPLICATE_PASS_INTERVAL) {
        run = 0;
        return true;
    }
    return false;
}

#define HCI_GRP_LINK_CONT_CMDS             (0x01 << 10) // 0x0400
#define HCI_GRP_LINK_POLICY_CMDS           (0x02 << 10) // 0x0800
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10) // 0x0C00
#define HCI_GRP_INFO_PARAMS_CMDS           (0x04 << 10) // 0x1000

#define HCI_READ_BUFFER_SIZE               (0x0005 | HCI_GRP_INFO_PARAMS_CMDS)
#define HCI_READ_BD_ADDR                   (0x0009 | HCI_GRP_INFO_PARAMS_CMDS)

#define HCI_SET_EVENT_MASK                 (0x0001 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_READ_STORED_LINK_KEY           (0x000D | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_STORED_LINK_KEY          (0x0011 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_LOCAL_NAME               (0x0013 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_CLASS_OF_DEVICE          (0x0024 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_SCAN_ENABLE              (0x001A | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_AUTH_ENABLE              (0x0020 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_WRITE_SIMPLE_PAIRING_MODE      (0x0056 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)

#define HCI_ROLE_DISCOVERY                 (0x0009 | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_SWITCH_ROLE                    (0x000B | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_WRITE_LINK_POLICY              (0x000D | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_READ_DEFAULT_LINK_POLICY       (0x000E | HCI_GRP_LINK_POLICY_CMDS)
#define HCI_WRITE_DEFAULT_LINK_POLICY      (0x000F | HCI_GRP_LINK_POLICY_CMDS)

#define HCI_INQUIRY                        (0x0001 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_INQUIRY_CANCEL                 (0x0002 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_CREATE_CONNECTION              (0x0005 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_DISCONNECT                     (0x0006 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_CREATE_CONNECTION_CANCEL       (0x0008 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_ACCEPT_CONNECTION_REQUEST      (0x0009 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_REJECT_CONNECTION_REQUEST      (0x000A | HCI_GRP_LINK_CONT_CMDS)
#define HCI_LINK_KEY_REQUEST_REPLY         (0x000B | HCI_GRP_LINK_CONT_CMDS)
#define HCI_LINK_KEY_REQUEST_NEG_REPLY     (0x000C | HCI_GRP_LINK_CONT_CMDS)
#define HCI_PIN_CODE_REQUEST_REPLY         (0x000D | HCI_GRP_LINK_CONT_CMDS)
#define HCI_PIN_CODE_REQUEST_NEG_REPLY     (0x000E | HCI_GRP_LINK_CONT_CMDS)
#define HCI_AUTHENTICATION_REQUESTED       (0x0011 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_SET_CONNECTION_ENCRYPTION      (0x0013 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_REMOTE_NAME_REQUEST            (0x0019 | HCI_GRP_LINK_CONT_CMDS)
#define HCI_IO_CAPABILITY_REQUEST_REPLY    (0x002B | HCI_GRP_LINK_CONT_CMDS)
#define HCI_USER_CONFIRMATION_REQUEST_REPLY (0x002C | HCI_GRP_LINK_CONT_CMDS)
#define HCI_USER_PASSKEY_REQUEST_NEG_REPLY (0x002F | HCI_GRP_LINK_CONT_CMDS)


#define HCI_INQUIRY_COMP_EVT                0x01
#define HCI_INQUIRY_RESULT_EVT              0x02
#define HCI_CONNECTION_COMP_EVT             0x03
#define HCI_CONNECTION_REQUEST_EVT          0x04
#define HCI_DISCONNECTION_COMP_EVT          0x05
#define HCI_AUTHENTICATION_COMP_EVT         0x06
#define HCI_RMT_NAME_REQUEST_COMP_EVT       0x07
#define HCI_ENCRYPTION_CHANGE_EVT           0x08
#define HCI_CHANGE_CONN_LINK_KEY_EVT        0x09
#define HCI_MASTER_LINK_KEY_COMP_EVT        0x0A
#define HCI_READ_RMT_FEATURES_COMP_EVT      0x0B
#define HCI_READ_RMT_VERSION_COMP_EVT       0x0C
#define HCI_QOS_SETUP_COMP_EVT              0x0D
#define HCI_COMMAND_COMPLETE_EVT            0x0E
#define HCI_COMMAND_STATUS_EVT              0x0F

#define HCI_HARDWARE_ERROR_EVT              0x10
#define HCI_FLUSH_OCCURED_EVT               0x11
#define HCI_ROLE_CHANGE_EVT                 0x12
#define HCI_NUM_COMPL_DATA_PKTS_EVT         0x13
#define HCI_MODE_CHANGE_EVT                 0x14
#define HCI_RETURN_LINK_KEYS_EVT            0x15
#define HCI_PIN_CODE_REQUEST_EVT            0x16
#define HCI_LINK_KEY_REQUEST_EVT            0x17
#define HCI_LINK_KEY_NOTIFICATION_EVT       0x18
#define HCI_LOOPBACK_COMMAND_EVT            0x19
#define HCI_DATA_BUF_OVERFLOW_EVT           0x1A
#define HCI_MAX_SLOTS_CHANGED_EVT           0x1B
#define HCI_READ_CLOCK_OFF_COMP_EVT         0x1C
#define HCI_CONN_PKT_TYPE_CHANGE_EVT        0x1D
#define HCI_INQUIRY_RESULT_WITH_RSSI_EVT    0x22
#define HCI_EXTENDED_INQUIRY_RESULT_EVT     0x2F
#define HCI_IO_CAPABILITY_REQUEST_EVT       0x31
#define HCI_IO_CAPABILITY_RESPONSE_EVT      0x32
#define HCI_USER_CONFIRMATION_REQUEST_EVT   0x33
#define HCI_USER_PASSKEY_REQUEST_EVT        0x34
#define HCI_SIMPLE_PAIRING_COMPLETE_EVT     0x36

#define HCI_ROLE_MASTER     0x00
#define HCI_ROLE_SLAVE      0x01

const char* hci_cmd(int c)
{
    switch (c) {
        case HCI_READ_BUFFER_SIZE:  return "HCI_READ_BUFFER_SIZE";
        case HCI_READ_BD_ADDR:      return "HCI_READ_BD_ADDR";

        case HCI_SET_EVENT_MASK:         return "HCI_SET_EVENT_MASK";
        case HCI_RESET:                 return "HCI_RESET";
        case HCI_READ_STORED_LINK_KEY:  return "HCI_READ_STORED_LINK_KEY";
        case HCI_WRITE_STORED_LINK_KEY: return "HCI_WRITE_STORED_LINK_KEY";
        case HCI_WRITE_LOCAL_NAME:      return "HCI_WRITE_LOCAL_NAME";
        case HCI_WRITE_CLASS_OF_DEVICE: return "HCI_WRITE_CLASS_OF_DEVICE";
        case HCI_WRITE_SCAN_ENABLE:     return "HCI_WRITE_SCAN_ENABLE";
        case HCI_WRITE_AUTH_ENABLE:     return "HCI_WRITE_AUTH_ENABLE";
        case HCI_WRITE_SIMPLE_PAIRING_MODE: return "HCI_WRITE_SIMPLE_PAIRING_MODE";

        case HCI_ROLE_DISCOVERY:        return "HCI_ROLE_DISCOVERY";
        case HCI_SWITCH_ROLE:           return "HCI_SWITCH_ROLE";
        case HCI_WRITE_LINK_POLICY:     return "HCI_WRITE_LINK_POLICY";

        case HCI_INQUIRY:               return "HCI_INQUIRY";
        case HCI_INQUIRY_CANCEL:        return "HCI_INQUIRY_CANCEL";
        case HCI_CREATE_CONNECTION:     return "HCI_CREATE_CONNECTION";
        case HCI_DISCONNECT:            return "HCI_DISCONNECT";
        case HCI_CREATE_CONNECTION_CANCEL: return "HCI_CREATE_CONNECTION_CANCEL";
        case HCI_ACCEPT_CONNECTION_REQUEST:     return "HCI_ACCEPT_CONNECTION_REQUEST";
        case HCI_REJECT_CONNECTION_REQUEST:     return "HCI_REJECT_CONNECTION_REQUEST";
        case HCI_LINK_KEY_REQUEST_REPLY:        return "HCI_LINK_KEY_REQUEST_REPLY";
        case HCI_LINK_KEY_REQUEST_NEG_REPLY:    return "HCI_LINK_KEY_REQUEST_NEG_REPLY";
        case HCI_PIN_CODE_REQUEST_REPLY:        return "HCI_PIN_CODE_REQUEST_REPLY";
        case HCI_PIN_CODE_REQUEST_NEG_REPLY:    return "HCI_PIN_CODE_REQUEST_NEG_REPLY";
        case HCI_IO_CAPABILITY_REQUEST_REPLY:   return "HCI_IO_CAPABILITY_REQUEST_REPLY";
        case HCI_USER_CONFIRMATION_REQUEST_REPLY: return "HCI_USER_CONFIRMATION_REQUEST_REPLY";
        case HCI_USER_PASSKEY_REQUEST_NEG_REPLY: return "HCI_USER_PASSKEY_REQUEST_NEG_REPLY";
        case HCI_AUTHENTICATION_REQUESTED:      return "HCI_AUTHENTICATION_REQUESTED";
        case HCI_SET_CONNECTION_ENCRYPTION:     return "HCI_SET_CONNECTION_ENCRYPTION";
        case HCI_REMOTE_NAME_REQUEST:           return "HCI_REMOTE_NAME_REQUEST";
    }
    static char s[32];
    sprintf(s,"HCICMD:%04X",c);
    return s;
}

typedef struct {
    u8 status;
    u16 acl_plen;
    u8 sco_plen;
    u16 acl_pkts;
    u16 sco_pkts;
} __attribute__((packed)) hci_buffer_size;

typedef struct {
    u16 handle;
    u8 reason;
} __attribute__((packed)) hci_disconnect;

typedef struct  {
    u8     status;
    u16   handle;
    u8     reason;
} __attribute__((packed)) hci_disconn_complete;

typedef struct  {
    u8     status;
    u16   handle;
} __attribute__((packed)) hci_auth_complete;

typedef struct  {
    u8     status;
    u16   handle;
    u8     enabled;
} __attribute__((packed)) hci_encryption_change;

typedef struct {
    bdaddr_t bdaddr;
    u8 pscan_rep_mode;
    u8 reserved;
    u8 dev_class[3];
    u16 clock_offset;
    int8_t rssi;
} __attribute__((packed)) inquiry_info_with_rssi;

typedef struct  {
    u8    status;
    u16   handle;
    u8    role;
} __attribute__((packed)) hci_role_discovery_complete;

typedef struct  {
    u8     status;
    bdaddr_t     peer_addr;
    u8     role;
} __attribute__((packed)) hci_role_change_evt;

#define L2CAP_COMMAND_REJ       0x01
#define L2CAP_CONN_REQ          0x02
#define L2CAP_CONN_RSP          0x03
#define L2CAP_CONF_REQ          0x04
#define L2CAP_CONF_RSP          0x05
#define L2CAP_DISCONN_REQ       0x06
#define L2CAP_DISCONN_RSP       0x07
#define L2CAP_ECHO_REQ          0x08
#define L2CAP_ECHO_RSP          0x09
#define L2CAP_INFO_REQ          0x0a
#define L2CAP_INFO_RSP          0x0b
#define L2CAP_PSM_HID_INTERRUPT 0x0013

typedef struct
{
    u8     type;          // acl = 2
    u16    handle;
    u16    length;         // total
    u16    l2capLength;    // length -4
    u16    cid;            // Signaling packet CID = 1
    u8     data[0];
} __attribute__((packed)) l2cap_data;

typedef struct
{
    u8    cmd;            //
    u8    id;
    u16   cmdLength;        // total-8
    u16   params[5];      // Params
} __attribute__((packed)) l2cap_cmd;

const char* batostr(const bdaddr_t& a)
{
    static char buf[32];
    auto d = a.b;
    sprintf(buf,"%02X:%02X:%02X:%02X:%02X:%02X",d[5],d[4],d[3],d[2],d[1],d[0]);
    return buf;
}

// Largest packet any queue accepts. Must cover a full ACL packet at the MTU
// we advertise (672) plus HCI/L2CAP headers.
#define PACKET_CAPACITY 768

struct PacketBlock {
    uint16_t len;
    uint8_t data[1];
};

// Small static pool used when malloc fails. Internal heap can be nearly
// exhausted while an emulator runs; without this, HCI events (connection
// complete, link key request, ...) and gamepad reports were silently lost.
namespace packet_pool {
    enum { BLOCKS = 16, PAYLOAD = 80 };
    struct Slot {
        uint16_t len;
        uint8_t data[PAYLOAD];
    };
    static Slot _slots[BLOCKS];
    static uint32_t _used;      // bit i set = _slots[i] in use
    static portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    static PacketBlock* alloc(int len)
    {
        if (len > PAYLOAD)
            return 0;
        PacketBlock* b = 0;
        portENTER_CRITICAL(&_mux);
        for (int i = 0; i < BLOCKS; ++i) {
            if (!(_used & (1u << i))) {
                _used |= (1u << i);
                b = (PacketBlock*)&_slots[i];
                break;
            }
        }
        portEXIT_CRITICAL(&_mux);
        return b;
    }

    static bool owns(const PacketBlock* b)
    {
        return (const void*)b >= (const void*)&_slots[0] &&
               (const void*)b < (const void*)&_slots[BLOCKS];
    }

    static void release(PacketBlock* b)
    {
        const int i = (int)((Slot*)b - &_slots[0]);
        portENTER_CRITICAL(&_mux);
        _used &= ~(1u << i);
        portEXIT_CRITICAL(&_mux);
    }
}

static PacketBlock* packet_block_alloc(const uint8_t* data, int len)
{
    PacketBlock* block = (PacketBlock*)malloc(sizeof(PacketBlock) + (len > 0 ? (len - 1) : 0));
    if (!block)
        block = packet_pool::alloc(len);
    if (!block)
        return 0;
    block->len = (uint16_t)len;
    if (len > 0)
        memcpy(block->data, data, len);
    return block;
}

static void packet_block_free(PacketBlock* block)
{
    if (!block)
        return;
    if (packet_pool::owns(block))
        packet_pool::release(block);
    else
        free(block);
}

struct PacketView {
    const uint8_t* data;
    int len;
    PacketBlock* block;
};

// QSIZE must be a power of two.
template <int QSIZE>
class PacketQ
{
    uint32_t _read;
    uint32_t _write;
    PacketBlock* _slots[QSIZE];
    mutex _mutex;
public:
    PacketQ() : _read(0),_write(0)
    {
        for (int i = 0; i < QSIZE; ++i)
            _slots[i] = 0;
    }

    ~PacketQ()
    {
        PacketView packet = {};
        while (read(packet))
            release(packet);
    }

    bool empty()
    {
        return _read == _write;
    }

    static void release(PacketView& packet)
    {
        packet_block_free(packet.block);
        packet.data = 0;
        packet.len = 0;
        packet.block = 0;
    }

    bool read(PacketView& packet)
    {
        lock_guard<mutex> lock(_mutex);
        if (_read == _write)
            return false;
        PacketBlock* block = _slots[_read & (QSIZE - 1)];
        _slots[_read & (QSIZE - 1)] = 0;
        ++_read;
        if (!block)
            return false;
        packet.block = block;
        packet.len = block->len;
        packet.data = block->data;
        return true;
    }

    // drop_oldest: when full, discard the oldest packet instead of the new
    // one. Input channels want the latest report, not a backlog.
    bool write(const uint8_t* data, int len, bool drop_oldest = false)
    {
        if (len < 0 || len > PACKET_CAPACITY)
            return false;
        if (len > 0 && !data)
            return false;

        PacketBlock* block = packet_block_alloc(data, len);
        if (!block)
            return false;

        PacketBlock* dropped = 0;
        {
            lock_guard<mutex> lock(_mutex);
            if (_write - _read == QSIZE) {
                if (!drop_oldest) {
                    dropped = block;
                    block = 0;
                } else {
                    dropped = _slots[_read & (QSIZE - 1)];
                    _slots[_read & (QSIZE - 1)] = 0;
                    ++_read;
                }
            }
            if (block)
                _slots[_write++ & (QSIZE - 1)] = block;
        }
        packet_block_free(dropped);
        return block != 0;
    }

    // Keep only the given packet (used for Wii remotes, which stream
    // identical reports continuously).
    bool replace(const uint8_t* data, int len)
    {
        if (len < 0 || len > PACKET_CAPACITY)
            return false;
        if (len > 0 && !data)
            return false;

        {
            lock_guard<mutex> lock(_mutex);
            if (_read != _write) {
                PacketBlock* block = _slots[(_write - 1) & (QSIZE - 1)];
                if (block && block->len == (uint16_t)len) {
                    if (len > 0)
                        memcpy(block->data, data, len);
                    return true;
                }
            }
        }

        PacketBlock* block = packet_block_alloc(data, len);
        if (!block)
            return false;

        PacketBlock* old[QSIZE];
        int old_count = 0;
        {
            lock_guard<mutex> lock(_mutex);
            while (_read != _write) {
                PacketBlock* b = _slots[_read & (QSIZE - 1)];
                _slots[_read & (QSIZE - 1)] = 0;
                ++_read;
                if (b)
                    old[old_count++] = b;
            }
            _slots[_write++ & (QSIZE - 1)] = block;
        }
        for (int i = 0; i < old_count; ++i)
            packet_block_free(old[i]);
        return true;
    }
};

typedef PacketQ<64> HCIPacketQ;     // raw HCI traffic to/from the controller
typedef PacketQ<16> SocketPacketQ;  // per L2CAP channel

static void to_hex(char* dst, const uint8_t* src, int len)
{
    while (len--) {
        sprintf(dst,"%02X",*src++);
        dst += 2;
    }
}

static int nybble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

static bool from_hex(uint8_t* dst, const char* src, int len)
{
    while (len--) {
        int hi = nybble(src[0]);
        int lo = nybble(src[1]);
        if (hi < 0 || lo < 0)
            return false;
        *dst++ = (hi << 4) | lo;
        src += 2;
    }
    return true;
}

int read_link_key(const bdaddr_t* addr, uint8_t* key)
{
    char s[64] = {0};
    char ba[12+1] = {0};
    to_hex(ba,addr->b,6);
    if (sys_get_pref(ba,s,sizeof(s)-1)) {
        if (strlen(s) >= 32 && from_hex(key,s,16)) {
            PAIR_LOG("BT: link key found for %s\n", ba);
            return 0;
        }
        PAIR_LOG("BT: ignoring invalid stored link key for %s len=%lu\n",
            ba,
            (unsigned long)strlen(s));
    }
    PAIR_LOG("BT: no stored link key for %s\n", ba);
    return -1;
}

static void write_link_key(const bdaddr_t* addr, const uint8_t* key)
{
    char buf[64];
    char ba[12+1] = {0};
    to_hex(ba,addr->b,6);
    to_hex(buf,key,16);
    PAIR_LOG("BT: writing link key for %s\n", ba);
    sys_set_pref(ba,buf);
}

void forget_link_key(const bdaddr_t* addr)
{
    if (!addr)
        return;
    char ba[12+1] = {0};
    to_hex(ba,addr->b,6);
    PAIR_LOG("BT: forgetting stored link key for %s\n", ba);
    sys_set_pref(ba,"");
}

class BTDevice;
class L2CAPSocket {
public:
    int _state;
    uint16_t _psm;
    uint16_t _scid;
    uint16_t _dcid;
    SocketPacketQ _q;
    BTDevice* _device;
    int _signaling_flags;
    uint8_t _last_latest_payload[32];
    uint16_t _last_latest_len;
    bool _last_latest_valid;
    uint8_t _raw_latest_payload[32];
    uint16_t _raw_latest_len;
    bool _raw_latest_valid;
    uint64_t _wii_button_sig;
    bool _wii_button_sig_valid;
    uint16_t _wii_raw_duplicate_run;
    uint16_t _wii_button_duplicate_run;
    bool _wii_report_mode_quench_pending;
    uint8_t _wii_report_mode_quench_report;

    L2CAPSocket() : _state(0),_signaling_flags(0),_last_latest_len(0),_last_latest_valid(false),_raw_latest_len(0),_raw_latest_valid(false),_wii_button_sig(0),_wii_button_sig_valid(false),_wii_raw_duplicate_run(0),_wii_button_duplicate_run(0),_wii_report_mode_quench_pending(false),_wii_report_mode_quench_report(0) {};
    void set_state(int state);
    ~L2CAPSocket() {
        set_state(L2CAP_DELETED);
    }
};

// forward
int hci_write(const uint8_t* buf, int len);

// L2CAP MTU we advertise and can receive (PacketQ holds MTU + headers).
#define L2CAP_LOCAL_MTU 672
#define L2CAP_PSM_NOT_SUPPORTED 0x0002
#define L2CAP_NO_RESOURCES      0x0004
#define L2CAP_CONF_PENDING      0x0004

enum {
    SLAVE = 1,
    ACTIVE = 2,
    READING_LINK_KEY = 4,
};

class BTDevice {
public:
    enum {
        WII_DEVICE_CLASS = 0x042500,
        WII_TR_DEVICE_CLASS = 0x080500,
        WII_U_PRO_DEVICE_CLASS = 0x082500,

        WII_DEVICE_CLASS_CANONICAL = 0x002504,
        WII_TR_DEVICE_CLASS_CANONICAL = 0x000508,
        WII_U_PRO_DEVICE_CLASS_CANONICAL = 0x002508,
    };

    bdaddr_t _addr;
    string _name;
    uint8_t _major;
    uint8_t _minor;
    int _dev_class;  //
    int _handle;
    int _flags;

    // packet reassembly
    vector<uint8_t> _packet;
    int _packet_pos;
    int _packet_cid;

    uint8_t _txid;
    map<int,L2CAPSocket*> _sockets;

    // Paging hints from the last inquiry result; they make Create Connection
    // much faster and more reliable than paging blind.
    uint8_t _pscan_rep_mode;
    uint16_t _clock_offset;
    bool _page_info_valid;

    BTDevice() : _handle(-1),_flags(SLAVE),_packet_pos(0),_packet_cid(0),_txid(1),
                 _pscan_rep_mode(1),_clock_offset(0),_page_info_valid(false)
    {
    }

    // L2CAP signalling identifiers are 1..255; 0 is invalid.
    uint8_t next_txid()
    {
        if (_txid == 0)
            _txid = 1;
        return _txid++;
    }

    static bool is_wii_device_class(int dev_class)
    {
        return dev_class == WII_DEVICE_CLASS ||
               dev_class == WII_DEVICE_CLASS_CANONICAL ||
               dev_class == WII_TR_DEVICE_CLASS ||
               dev_class == WII_TR_DEVICE_CLASS_CANONICAL ||
               dev_class == WII_U_PRO_DEVICE_CLASS ||
               dev_class == WII_U_PRO_DEVICE_CLASS_CANONICAL;
    }

    bool is_wii()
    {
        if (_name.find("Nintendo") == 0)
            return true;
        if (is_wii_device_class(_dev_class))
            return true;
        return false;
    }

    bool is_wii_remote()
    {
        if (_name.find("Nintendo RVL-CNT-01") == 0)
            return true;
        return _dev_class == WII_DEVICE_CLASS ||
               _dev_class == WII_DEVICE_CLASS_CANONICAL ||
               _dev_class == WII_TR_DEVICE_CLASS ||
               _dev_class == WII_TR_DEVICE_CLASS_CANONICAL;
    }

    int l2_open(int scid, int psm, bool listen)
    {
        auto* s = new (std::nothrow) L2CAPSocket();
        if (!s) {
            PAIR_LOG("BT: L2CAP socket alloc failed scid=%d psm=%04X\n", scid, psm);
            return -1;
        }
        s->_psm = psm;
        s->_dcid = 0;
        s->_scid = scid;
        s->_device = this;
        try {
            _sockets[scid] = s;     // map node allocation can throw
        } catch (...) {
            s->_state = L2CAP_DELETED;  // no notification for a socket nobody saw
            delete s;
            return -1;
        }

        if (!listen) {
            int rc = connect(s->_psm,s->_scid);
            PAIR_LOG("BT: L2CAP open request psm=%04X scid=%d rc=%d\n", psm, scid, rc);
            s->set_state(L2CAP_OPENING);
        } else {
            PAIR_LOG("BT: L2CAP listening psm=%04X scid=%d\n", psm, scid);
            s->set_state(L2CAP_LISTENING);
        }
        return scid;
    }

    int l2_close(int scid, bool this_end)
    {
        auto s = get_socket(scid);
        if (!s)
            return -1;
        if (this_end && s->_state == L2CAP_OPEN)
            disconnect(s->_scid,s->_dcid);  // tell other end
        _sockets.erase(scid);
        delete s;   // was leaked on every close
        return 0;
    }

    L2CAPSocket* get_socket(int scid)
    {
        auto it = _sockets.find(scid);
        if (it == _sockets.end())
            return NULL;
        return it->second;
    }

    // this will happen before hid knows about it
    void hci_connected(const connection_info* ci)
    {
        _handle = ci->handle;
    }

    void set_role(uint8_t role)
    {
        _flags = role ? (_flags | SLAVE) : (_flags & ~SLAVE);
    }

    void disconnect_all()
    {
        // Mark the link dead first: deleting sockets notifies the HID layer,
        // which must not try to send on (or disconnect) a stale handle.
        _handle = -1;
        _flags = SLAVE;
        map<int,L2CAPSocket*> sockets;
        sockets.swap(_sockets);
        for (auto& p : sockets)
            delete p.second;
        _packet.clear();
        _packet_pos = 0;
    }

    // inbound l2cap packet. assemble and forward
    bool data(const l2cap_data* p, int packet_len)
    {
        if (!p || packet_len < 5)
            return false;

        int pt = (p->handle >> 12) & 3;
        const uint8_t* data = ((const uint8_t*)p) + 5;
        int len = p->length;
        if (len < 0 || len > packet_len - 5) {
            _packet.clear();
            _packet_pos = 0;
            return false;
        }

        if (pt == 0 || pt == 2) {  // start of a packet
            if (len < 4 || p->l2capLength > L2CAP_LOCAL_MTU) {
                // Never size the buffer from an unchecked length: a bogus
                // 64 KB value would throw bad_alloc and abort.
                _packet.clear();
                _packet_pos = 0;
                return false;
            }
            _packet_pos = 0;
            try {
                _packet.resize(p->l2capLength); // length of payload
            } catch (...) {
                _packet.clear();
                return false;
            }
            _packet_cid = p->cid;
            data += 4;
            len -= 4;                       // skip header
        } else if (_packet.empty()) {
            return false;
        }

        if (len < 0 || _packet_pos < 0 || _packet_pos + len > (int)_packet.size()) {
            _packet.clear();
            _packet_pos = 0;
            return false;
        }
        if (len > 0)
            memcpy(&_packet[_packet_pos],data,len);
        _packet_pos += len;

        // if packet is assembled, queue it.
        if (_packet_pos == (int)_packet.size()) {
            if (_packet_cid == 1) {
                if (_packet.size() < 4) {
                    _packet_pos = 0;
                    return false;
                }
                control((const l2cap_cmd*)&_packet[0], (int)_packet.size());
            } else {
                auto s = get_socket(_packet_cid);
                if (s) {
                    const uint8_t* payload = _packet.empty() ? 0 : &_packet[0];
                    bool queued = true;
                    if (is_wii_remote() && s->_psm == L2CAP_PSM_HID_INTERRUPT) {
                        if (_packet_pos <= (int)sizeof(s->_last_latest_payload) &&
                            s->_last_latest_valid &&
                            s->_last_latest_len == (uint16_t)_packet_pos &&
                            (0 == memcmp(s->_last_latest_payload, payload, _packet_pos))) {
                            queued = s->_q.replace(payload,_packet_pos);
                        } else {
                            if (_packet_pos <= (int)sizeof(s->_last_latest_payload)) {
                                if (_packet_pos > 0)
                                    memcpy(s->_last_latest_payload, payload, _packet_pos);
                                s->_last_latest_len = (uint16_t)_packet_pos;
                                s->_last_latest_valid = true;
                            } else {
                                s->_last_latest_valid = false;
                            }
                            queued = s->_q.replace(payload,_packet_pos);
                        }
                    } else {
                        // Interrupt channels carry input reports: keep the newest.
                        queued = s->_q.write(payload,_packet_pos,
                                             s->_psm == L2CAP_PSM_HID_INTERRUPT);
                    }
                    if (!queued) {
                        static uint32_t l2cap_queue_drop_count = 0;
                        l2cap_queue_drop_count++;
                        if ((l2cap_queue_drop_count & 0xFF) == 1) {
                            PAIR_LOG("HCI: l2cap queue drop cid=%d len=%d total=%lu\n",
                                _packet_cid,
                                _packet_pos,
                                (unsigned long)l2cap_queue_drop_count);
                        }
                    }
                } else {
                    _packet_pos = 0;
                    return false;   // did not have a socket for this data.
                }
            }
            _packet_pos = 0;
        }
        return true;
    }

    // wait until connection is completely open
    #define M(_x) (1 << ((_x) + 8))
    void check_open(L2CAPSocket* s)
    {
        if (!s)
            return;
        // Both directions must be configured. Outbound channels also need
        // our connection request answered. Inbound channels (a paired pad
        // reconnecting on its own) used to never reach L2CAP_OPEN.
        int mask = M(L2CAP_CONF_REQ) | M(L2CAP_CONF_RSP);
        if ((s->_signaling_flags & M(L2CAP_CONN_REQ)) == 0)
            mask |= M(L2CAP_CONN_RSP);
        if ((mask & s->_signaling_flags) == mask)
            s->set_state(L2CAP_OPEN);
    }

    void control(const l2cap_cmd* c, int len)
    {
        if (!c || len < 4 || c->cmdLength > len - 4 || (c->cmdLength & 1))
            return;

        const int param_count = c->cmdLength / 2;

        L2CAPSocket* s = 0;
        switch (c->cmd) {
            case L2CAP_ECHO_REQ:
                l2cap(L2CAP_ECHO_RSP,c->id,0,0);
                break;

            case L2CAP_CONN_RSP:
            {
                if (param_count < 3)
                    break;
                int status = c->params[2];
                s = get_socket(c->params[1]);
                if (s)
                    s->_signaling_flags |= M(c->cmd);
                if (status == 0) {    // good status
                    if (s) {
                        s->_dcid = c->params[0];
                        configure_request(s->_dcid);
                    }
                } else {
                    if (status != 1) {
                        if (s)
                            s->set_state(L2CAP_CLOSED);
                    }
                }
            }
                break;

            case L2CAP_CONF_RSP:
            {
                if (param_count < 1)
                    break;
                s = get_socket(c->params[0]);
                if (!s)
                    break;
                const int result = param_count > 2 ? c->params[2] : 0;
                if (result == L2CAP_CONF_PENDING)
                    break;      // final response follows
                if (result != 0) {
                    // The peer rejected our configuration; fail fast instead
                    // of treating the channel as open.
                    s->set_state(L2CAP_CLOSED);
                    break;
                }
                s->_signaling_flags |= M(c->cmd);
                check_open(s);
                break;
            }

            case L2CAP_CONF_REQ:
                if (param_count < 1)
                    break;
                s = get_socket(c->params[0]);
                if (s) {
                    // Accept the peer's options as-is. Echoing a guessed MTU
                    // (assuming the first option was MTU) could misreport it.
                    configure_response(c->id,s->_dcid);
                    if (param_count < 2 || !(c->params[1] & 1)) {   // no continuation
                        s->_signaling_flags |= M(c->cmd);
                        check_open(s);
                    }
                }
                break;

            case L2CAP_DISCONN_REQ:
            {
                if (param_count < 2)
                    break;
                l2_close(c->params[0],false); // scid
                uint16_t p[2] = {c->params[1],c->params[0]}; // dcid, scid
                l2cap(L2CAP_DISCONN_RSP,c->id, p, 2);
            }
                break;

            case L2CAP_DISCONN_RSP:
                if (param_count < 2)
                    break;
                ::l2_close(c->params[1]); // scid - already closed
                break;

            // active connection from peripheral
            case L2CAP_CONN_REQ:
            {
                if (param_count < 2)
                    break;
                int dcid = c->params[1];
                int psm = c->params[0];
                int rxid = c->id;
                _flags |= ACTIVE;   // inbound connection from peripheral

                L2CAPSocket* listener = 0;
                bool psm_in_use = false;
                for (auto& p : _sockets) {
                    auto* candidate = p.second;
                    if (candidate->_psm != psm)
                        continue;
                    if (candidate->_state == L2CAP_LISTENING) {
                        listener = candidate;
                        break;
                    }
                    psm_in_use = true;
                }
                if (!listener) {
                    // Answer instead of staying silent, which left the peer
                    // waiting for a timeout (e.g. a pad opening SDP to us).
                    // If our own channel on this PSM is in progress, say "no
                    // resources" so the pad keeps HID and answers ours.
                    u16 p[4] = { 0, (u16)dcid,
                                 (u16)(psm_in_use ? L2CAP_NO_RESOURCES : L2CAP_PSM_NOT_SUPPORTED), 0 };
                    l2cap(L2CAP_CONN_RSP,rxid,p,4);
                    break;
                }

                listener->_signaling_flags |= M(c->cmd);
                u16 p[4];
                p[0] = listener->_scid;    // this end
                p[1] = dcid;               // other end
                p[2] = 0;                  // ok
                p[3] = 0;                  // no further information
                l2cap(L2CAP_CONN_RSP,rxid,p,4);
                listener->_dcid = dcid;
                listener->set_state(L2CAP_OPENING);
                configure_request(listener->_dcid);    // ask to configure
            }

                break;

            case L2CAP_INFO_REQ:
            {
                if (param_count < 1)
                    break;
                u16 p[2] = {c->params[0],1};  // no info
                l2cap(L2CAP_INFO_RSP,c->id,p,2);
            }
                break;

            default:
                break;
        }
    }

    // l2cap data
    int send(const void* data = 0, int len = 0, int cid = 1)
    {
        if (_handle < 0 || len < 0 || len > L2CAP_LOCAL_MTU)
            return -1;
        // Small packets (signalling, output reports) are built on the stack;
        // this used to allocate a temporary vector every time. Large ones use
        // the heap so the emu thread (5 KB stack for NES) stays safe.
        enum { kStackPayload = 64 };
        uint8_t stack_buf[sizeof(l2cap_data) + kStackPayload];
        uint8_t* buf = stack_buf;
        if (len > kStackPayload) {
            buf = (uint8_t*)malloc(sizeof(l2cap_data) + len);
            if (!buf)
                return -1;
        }
        const int n = (int)sizeof(l2cap_data) + len;
        l2cap_data* d = (l2cap_data*)buf;
        d->type = 0x02;                 // acl
        d->handle = _handle | 0x2000;
        d->length = len + 4;            // includes l2cap header
        d->l2capLength = len;
        d->cid = cid;
        if (data && len > 0)
            memcpy(d->data,data,len);   // l2cap payload
        const int rc = hci_write(buf,n);    // send to outbound q.
        if (buf != stack_buf)
            free(buf);
        return rc;
    }

    int l2cap(uint8_t cmd, uint8_t id, const u16* params, int count)
    {
        l2cap_cmd b;
        b.cmd = cmd;
        b.id = id;
        b.cmdLength = count*2;
        for (int i = 0; i < count; i++)
            b.params[i] = params[i];
        return send(&b,4 + count*2);
    }

    int connect(u16 psm, u16 scid)
    {
        u16 p[2] = { psm, scid };
        return l2cap(L2CAP_CONN_REQ,next_txid(),p,2);
    }

    int disconnect(u16 scid, u16 dcid)
    {
        u16 p[2] = { dcid, scid };
        return l2cap(L2CAP_DISCONN_REQ,next_txid(),p,2);
    }

    int configure_request(u16 dcid)
    {
        // MTU option: what we can actually receive. 0xFFFF let pads send
        // packets PacketQ could not hold, so they were dropped.
        u16 p[4] = { dcid, 0, 0x0201, L2CAP_LOCAL_MTU };
        return l2cap(L2CAP_CONF_REQ,next_txid(),p,4);
    }

    int configure_response(u16 rxid, u16 dcid)
    {
        u16 p[3] = { dcid, 0, 0 };  // flags 0, result success, no options
        return l2cap(L2CAP_CONF_RSP,rxid,p,3);
    }
};
vector<BTDevice*> _devices;

// track socket state changes
void hci_socket_change(const bdaddr_t& addr, int scid, int state);
void L2CAPSocket::set_state(int state)
{
    if (_state == state)
        return;
    _state = state;
    hci_socket_change(_device->_addr,_scid,state);
}

BTDevice* get_device(const bdaddr_t* addr)
{
    for (auto d : _devices)
        if (memcmp(&d->_addr,addr,6) == 0)
            return d;
    return NULL;
}

BTDevice* get_device_by_handle(int handle)
{
    for (auto d : _devices) {
        if (d->_handle == handle)
            return d;
    }
    return NULL;
}

BTDevice* get_device_by_socket(int scid)
{
    for (auto d : _devices) {
        if (d->_sockets.find(scid) != d->_sockets.end())
            return d;
    }
    return NULL;
}

L2CAPSocket* get_socket(int scid)
{
    for (auto d : _devices) {
        auto s = d->get_socket(scid);
        if (s)
            return s;
    }
    return NULL;
}

static bool wii_report_mode_can_quench(uint8_t report)
{
    return report == 0x30 || report == 0x32 || report == 0x37;
}

static void mark_wii_report_mode_quench(L2CAPSocket* s, const uint8_t* payload, int len)
{
    if (!s || !payload || len < 2 || payload[0] != 0xA1)
        return;
    const uint8_t report = payload[1];
    if (!wii_report_mode_can_quench(report))
        return;
    s->_wii_report_mode_quench_report = report;
    s->_wii_report_mode_quench_pending = true;
}

// eww. inbound hid from vestigial connection. pass it on anyway

class HCI {
    enum StateMask {
        MASK_RESET = 1,
        MASK_READ_BUFFER_SIZE = 2,
        MASK_READ_BD_ADDR = 4,
        MASK_INITED = 8,
        MASK_INQUIRY = 16,
    };

    hci_handle _hci;
    int _state;
    int _inquiry_result_count;

    string _localname;
    bdaddr_t _localAddr;
    hci_buffer_size _buffer_size;
    bool _outbound_connection_pending;
    bdaddr_t _outbound_connection_addr;

    int _cid; // connection id
    HCIPacketQ _rx;
    HCIPacketQ _tx;

public:
    hci_callback _callback;
    void* _callback_ref;

    // create a new socked on this psm
    int l2_open(const bdaddr_t* addr, int psm, bool listen)
    {
        BTDevice* d = get_device(addr);
        if (!d)
            return -1;
        return d->l2_open(_cid++,psm,listen);
    }

    int l2_close(int scid, bool this_end = true)
    {
        BTDevice* d = get_device_by_socket(scid);
        if (!d)
            return -1;
        return d->l2_close(scid,this_end);
    }

    // recv from packet queue
    int l2_recv(int scid, uint8_t* dst, int len)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        PacketView d = {};
        if (!s->_q.read(d))
            return 0;
        len = min(len,d.len);
        if (len > 0)
            memcpy(dst,d.data,len);
        SocketPacketQ::release(d);
        return len;
    }

    int l2_send(int scid, const uint8_t* data, int len)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        if (s->_device->send(data,len,s->_dcid))
            return -1;
        return len; //
    }

    int l2_state(int scid)
    {
        auto* s = get_socket(scid);
        if (!s)
            return -1;
        return s->_state;
    }

    // initiate a connection to this device
    int connect(const bdaddr_t* addr)
    {
        auto *d = get_device(addr);
        if (!d)
            return -1;
        return create_connection(*d);
    }

    HCI(const char* localname) : _state(-1),_inquiry_result_count(0),_localname(localname ? localname : "esp_hid"),_outbound_connection_pending(false),_cid(0x40)
    {
        memset(&_outbound_connection_addr,0,sizeof(_outbound_connection_addr));
        _hci = hci_open();
        if (_hci) {
            hci_set_packet_handler(_hci,packet_,this);
            hci_set_ready_to_send_handler(_hci,ready_to_send_,this);
        }
    }

    ~HCI() {
        hci_close(_hci);
    }

    int init(hci_callback cb, void* ref)
    {
        _callback = cb;
        _callback_ref = ref;
        return 0;
    }

    int start_inquiry(int duration_secs)
    {
        if (!(_state & MASK_INITED))
            return -2;  // controller HCI init is still in progress
        if (_outbound_connection_pending)
            return -3;  // wait for the active page or its cancellation
        if (_state & MASK_INQUIRY)
            return 1;   // already running
        return inquiry(duration_secs);
    }

    int cancel_inquiry()
    {
        return inquiry_cancel();
    }

    void queue_wii_report_mode_quenches()
    {
        for (auto d : _devices) {
            if (!d || !d->is_wii_remote())
                continue;
            for (auto& p : d->_sockets) {
                auto* s = p.second;
                if (!s || s->_psm != L2CAP_PSM_HID_INTERRUPT ||
                    s->_state != L2CAP_OPEN ||
                    !s->_wii_report_mode_quench_pending ||
                    !wii_report_mode_can_quench(s->_wii_report_mode_quench_report) ||
                    s->_dcid == 0) {
                    continue;
                }

                uint8_t cmd[4] = { 0xA2, 0x12, 0x00, s->_wii_report_mode_quench_report };
                if (d->send(cmd, sizeof(cmd), s->_dcid) == 0) {
                    s->_wii_report_mode_quench_pending = false;
                }
            }
        }
    }

    int update()
    {
        queue_wii_report_mode_quenches();

        // send any pending
        PacketView buf = {};
        int tx_budget = HCI_UPDATE_TX_BUDGET;
        while (tx_budget-- > 0 && hci_send_available(_hci) && _tx.read(buf))
        {
            if (!buf.data || buf.len <= 0) {
                HCIPacketQ::release(buf);
                continue;
            }
            hci_send(_hci,buf.data,buf.len);
            HCIPacketQ::release(buf);
        }

        // handle any inbound.
        // hcl/acl ordering challenge. TODO.
        int rx_budget = HCI_UPDATE_RX_BUDGET;
        while (rx_budget-- > 0 && _rx.read(buf)) {
            if (!buf.data || buf.len <= 0) {
                HCIPacketQ::release(buf);
                continue;
            }
            switch (buf.data[0]) {
                case 0x2:
                    // The Wii duplicate filter used to run on the BT
                    // controller task, walking _devices/_sockets while this
                    // thread modified them. It runs here now.
                    if (buf.len >= 5 && !drop_duplicate_wii_interrupt_packet(buf.data, buf.len))
                        acl(buf.data,buf.len);
                    break;
                case 0x4:
                    if (buf.len >= 3 && buf.data[2] <= buf.len - 3)
                        hci(buf.data[1],buf.data + 3,buf.data[2]);
                    break;
                default:
                    break;
            }
            HCIPacketQ::release(buf);
        }
        return 0;
    }

    int write(const uint8_t* data, int len)
    {
        return _tx.write(data,len) ? 0 : -1; // send to outbound q.
    }

private:
    static void packet_(hci_handle h, const uint8_t* d, int len, void* ref)
    {
        ((HCI*)ref)->packet(d,len);
    }

    static void ready_to_send_(hci_handle h, void* ref)
    {
        ((HCI*)ref)->ready_to_send();
    }

    // inbound packet from hci, queue it. Runs on the BT controller task:
    // must not touch _devices or sockets, which the host thread owns.
    void packet(const uint8_t* data, int len)
    {
        if (!_rx.write(data,len)) {
            PAIR_LOG("HCI: rx queue drop len=%d\n", len);
        }
    }

    bool drop_duplicate_wii_interrupt_packet(const uint8_t* data, int len)
    {
        if (!data || len < (int)sizeof(l2cap_data))
            return false;
        const l2cap_data* p = (const l2cap_data*)data;
        if (p->type != 0x02)
            return false;

        const int pt = (p->handle >> 12) & 3;
        if (pt != 0 && pt != 2)
            return false;
        if (p->length < 4 || p->length > len - 5)
            return false;
        if (p->l2capLength != p->length - 4)
            return false;

        BTDevice* d = get_device_by_handle(p->handle & 0x0FFF);
        if (!d || !d->is_wii_remote())
            return false;
        L2CAPSocket* s = d->get_socket(p->cid);
        if (!s || s->_psm != L2CAP_PSM_HID_INTERRUPT)
            return false;

        const uint8_t* payload = p->data;
        const int payload_len = p->l2capLength;
        if (payload_len < 0 || payload_len > (int)sizeof(s->_raw_latest_payload)) {
            s->_raw_latest_valid = false;
            return false;
        }
        if (s->_raw_latest_valid &&
            s->_raw_latest_len == (uint16_t)payload_len &&
            (payload_len == 0 || memcmp(s->_raw_latest_payload, payload, payload_len) == 0)) {
            bool quench = false;
            if (note_wii_duplicate(s->_wii_raw_duplicate_run, &quench)) {
                return false;
            }
            if (quench)
                mark_wii_report_mode_quench(s, payload, payload_len);
            return true;
        }

        if (payload_len > 0)
            memcpy(s->_raw_latest_payload, payload, payload_len);
        s->_raw_latest_len = (uint16_t)payload_len;
        s->_raw_latest_valid = true;
        s->_wii_raw_duplicate_run = 0;
        s->_wii_report_mode_quench_pending = false;

        uint64_t sig = 0;
        if (wii_input_button_signature(payload, payload_len, &sig)) {
            if (s->_wii_button_sig_valid && s->_wii_button_sig == sig) {
                bool quench = false;
                if (note_wii_duplicate(s->_wii_button_duplicate_run, &quench)) {
                    return false;
                }
                if (quench)
                    mark_wii_report_mode_quench(s, payload, payload_len);
                return true;
            }
            s->_wii_button_sig = sig;
            s->_wii_button_sig_valid = true;
            s->_wii_button_duplicate_run = 0;
        } else {
            s->_wii_button_sig_valid = false;
            s->_wii_button_duplicate_run = 0;
        }
        return false;
    }

    static bool wii_input_button_signature(const uint8_t* payload, int len, uint64_t* sig)
    {
        if (!payload || len < 4 || !sig || payload[0] != 0xA1)
            return false;
        const uint8_t report = payload[1];
        const uint16_t common = (uint16_t)(((uint16_t)payload[2] << 8) | payload[3]);
        uint16_t ext_buttons = 0;
        switch (report) {
            case 0x30:
                break;
            case 0x32:
                if (len >= 10)
                    ext_buttons = (uint16_t)(((uint16_t)payload[8] << 8) | payload[9]);
                break;
            case 0x37:
                if (len >= 23)
                    ext_buttons = (uint16_t)(((uint16_t)payload[21] << 8) | payload[22]);
                break;
            default:
                return false;
        }
        *sig = ((uint64_t)report << 32) | ((uint64_t)common << 16) | (uint64_t)ext_buttons;
        return true;
    }

    void ready_to_send()
    {
        if (_state == -1) {
            cmd(HCI_RESET);
            _state = 0;
        }
    }

    // hci command
    int cmd(uint16_t c, const void* data = 0, int len = 0)
    {
        if (len < 0 || len > 255)
            return -1;
        uint8_t buf[4 + 255];
        if (data && len > 0)
            memcpy(&buf[4],data,len);
        buf[0] = 0x01;         // hci Command
        buf[1] = (uint8_t)c;
        buf[2] = (uint8_t)(c >> 8);
        buf[3] = len;
        int rc = write(buf,len + 4); // send to outbound q.
        if (rc)
            PAIR_LOG("BT: command enqueue failed %s len=%d rc=%d\n", hci_cmd(c), len, rc);
        return rc;
    }

    // look for devices
    int inquiry(uint8_t duration)
    {
        _inquiry_result_count = 0;
        uint8_t count = 0;
        uint8_t buf[5] = { 0x33, 0x8B, 0x9E, duration, count};
        int rc = cmd(HCI_INQUIRY,buf,sizeof(buf));
        if (rc == 0) {
            _state |= MASK_INQUIRY;
            PAIR_LOG("BT: inquiry start (%us)\n", duration);
        }
        return rc;
    }

    int inquiry_cancel()
    {
        if (!(_state & MASK_INQUIRY))
            return 0;
        _state &= ~MASK_INQUIRY;
        return cmd(HCI_INQUIRY_CANCEL,0,0);
    }

    int add_device(const bdaddr_t& addr, const uint8_t* dev_class, const inquiry_info* page = 0)
    {
        auto* d = get_device(&addr);
        if (!d) {
            d = new (std::nothrow) BTDevice();
            if (!d)
                return -1;
            d->_name = batostr(addr);
            d->_addr = addr;
            int dev_class_value = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
            int reversed_dev_class_value = (dev_class[2] << 16) | (dev_class[1] << 8) | dev_class[0];
            d->_major = (dev_class_value >> 8) & 0x1f;
            d->_minor = (dev_class_value >> 2) & 0x3f;
            if (d->_minor == 0)
                d->_minor = (reversed_dev_class_value >> 2) & 0x3f;
            d->_dev_class = dev_class_value;
            d->_handle = -1;        // not connected yet
            try {
                _devices.push_back(d);
            } catch (...) {
                delete d;
                return -1;
            }
            PAIR_LOG("BT: discovered %s class=%06X\n", batostr(addr), d->_dev_class);
        }
        if (page) {
            d->_pscan_rep_mode = page->pscan_rep_mode;
            d->_clock_offset = page->clock_offset;
            d->_page_info_valid = true;
        }
        return 0;
    }

    int inquiry_result(const inquiry_info* info)
    {
        return add_device(info->bdaddr,info->dev_class,info);
    }

    void emit_inquiry_result(const inquiry_info& info, bool has_rssi, int rssi)
    {
        inquiry_result(&info);
        _callback(CALLBACK_INQUIRY_RESULT,&info,sizeof(info),_callback_ref);
        ++_inquiry_result_count;

        const int dev_class = (info.dev_class[0] << 16) | (info.dev_class[1] << 8) | info.dev_class[2];
        if (has_rssi) {
            PAIR_LOG("BT: inquiry hit %s class=%06X rssi=%d\n",
                batostr(info.bdaddr), dev_class, rssi);
        } else {
            PAIR_LOG("BT: inquiry hit %s class=%06X\n",
                batostr(info.bdaddr), dev_class);
        }
    }

    void handle_inquiry_result_evt(const uint8_t* data, uint8_t len)
    {
        if (len < 1)
            return;

        int n = *data++;
        int remaining = len - 1;
        const int entry_size = (int)sizeof(inquiry_info);
        if (n <= 0 || remaining < (n * entry_size)) {
            PAIR_LOG("BT: malformed inquiry result event len=%u count=%d\n", (unsigned)len, n);
            return;
        }

        while (n--) {
            inquiry_info info = {};
            memcpy(&info, data, entry_size);
            emit_inquiry_result(info, false, 0);
            data += entry_size;
        }
    }

    void handle_inquiry_result_with_rssi_evt(const uint8_t* data, uint8_t len)
    {
        if (len < 1)
            return;

        int n = *data++;
        int remaining = len - 1;
        const int entry_size = (int)sizeof(inquiry_info_with_rssi);
        if (n <= 0 || remaining < (n * entry_size)) {
            PAIR_LOG("BT: malformed inquiry RSSI event len=%u count=%d\n", (unsigned)len, n);
            return;
        }

        while (n--) {
            inquiry_info_with_rssi rssi_info = {};
            memcpy(&rssi_info, data, entry_size);

            inquiry_info info = {};
            info.bdaddr = rssi_info.bdaddr;
            info.pscan_rep_mode = rssi_info.pscan_rep_mode;
            info.pscan_period_mode = rssi_info.reserved;
            info.pscan_mode = 0;
            memcpy(info.dev_class, rssi_info.dev_class, sizeof(info.dev_class));
            info.clock_offset = rssi_info.clock_offset;

            emit_inquiry_result(info, true, (int)rssi_info.rssi);
            data += entry_size;
        }
    }

    void handle_extended_inquiry_result_evt(const uint8_t* data, uint8_t len)
    {
        if (len < 1)
            return;

        int n = *data++;
        if (n <= 0) {
            PAIR_LOG("BT: malformed extended inquiry event len=%u count=%d\n", (unsigned)len, n);
            return;
        }

        int remaining = len - 1;
        int entry_size = remaining / n;
        if (entry_size < 14) {
            PAIR_LOG("BT: malformed extended inquiry payload len=%u count=%d entry=%d\n",
                (unsigned)len, n, entry_size);
            return;
        }

        while (n--) {
            inquiry_info info = {};
            info.bdaddr = *((const bdaddr_t*)data);
            info.pscan_rep_mode = data[6];
            info.pscan_period_mode = data[7];
            info.pscan_mode = 0;
            memcpy(info.dev_class, data + 8, sizeof(info.dev_class));
            info.clock_offset = (uint16_t)data[11] | ((uint16_t)data[12] << 8);
            int8_t rssi = (int8_t)data[13];

            emit_inquiry_result(info, true, (int)rssi);
            data += entry_size;
        }
    }

public:
    int remote_name_request(const bdaddr_t& addr)
    {
        u8 buf[6+4] = {0};
        memcpy(buf,addr.b,6);
        buf[7] = 1;
        PAIR_LOG("BT: requesting name for %s\n", batostr(addr));
        int rc = cmd(HCI_REMOTE_NAME_REQUEST,buf,sizeof(buf));
        PAIR_LOG("BT: name request queued for %s rc=%d\n", batostr(addr), rc);
        return rc;
    }

    int authentication_requested(const bdaddr_t& addr)
    {
        auto* d = get_device(&addr);
        if (d && d->_handle != -1) {
            PAIR_LOG("BT: requesting authentication for %s handle=%04X\n", batostr(addr), d->_handle);
            int rc = cmd(HCI_AUTHENTICATION_REQUESTED,&d->_handle,2);
            PAIR_LOG("BT: authentication request queued for %s rc=%d\n", batostr(addr), rc);
            return rc;
        }
        PAIR_LOG("BT: authentication request rejected locally for %s (missing handle)\n", batostr(addr));
        return -1;
    }

    int set_connection_encryption(const bdaddr_t& addr, bool enabled)
    {
        auto* d = get_device(&addr);
        if (d && d->_handle != -1) {
            uint8_t buf[3] = {
                (uint8_t)d->_handle,
                (uint8_t)(d->_handle >> 8),
                (uint8_t)(enabled ? 1 : 0),
            };
            PAIR_LOG("BT: requesting encryption for %s handle=%04X enabled=%u\n",
                batostr(addr), d->_handle, (unsigned)buf[2]);
            int rc = cmd(HCI_SET_CONNECTION_ENCRYPTION,buf,sizeof(buf));
            PAIR_LOG("BT: encryption request queued for %s rc=%d\n", batostr(addr), rc);
            return rc;
        }
        PAIR_LOG("BT: encryption request rejected locally for %s (missing handle)\n", batostr(addr));
        return -1;
    }

    int accept_connection_request(const bdaddr_t& addr)
    {
        uint8_t buf[7] = {0};
        memcpy(buf,&addr,6);
        buf[6] = 0;
        PAIR_LOG("BT: accepting inbound connection from %s\n", batostr(addr));
        int rc = cmd(HCI_ACCEPT_CONNECTION_REQUEST,buf,sizeof(buf));
        PAIR_LOG("BT: inbound connection accept queued for %s rc=%d\n", batostr(addr), rc);
        return rc;
    }

    bool outbound_connection_matches(const bdaddr_t& addr) const
    {
        return _outbound_connection_pending &&
            memcmp(&_outbound_connection_addr,&addr,sizeof(addr)) == 0;
    }

    void clear_outbound_connection_pending(const char* reason)
    {
        (void)reason;
        if (!_outbound_connection_pending)
            return;
        _outbound_connection_pending = false;
        memset(&_outbound_connection_addr,0,sizeof(_outbound_connection_addr));
    }

    int cancel_connection(const bdaddr_t& addr)
    {
        auto* d = get_device(&addr);
        if (!outbound_connection_matches(addr))
            return 0;
        int rc = cmd(HCI_CREATE_CONNECTION_CANCEL,addr.b,sizeof(addr.b));
        if (d && d->_handle == 0)
            d->_handle = -1;
        return rc;
    }

    // Tear down the baseband link (or cancel a pending page). Without this,
    // a handshake that failed in the host left the pad connected at the radio
    // level: it stopped searching and our next connect attempt was a no-op.
    int disconnect_device(const bdaddr_t& addr)
    {
        auto* d = get_device(&addr);
        if (!d)
            return -1;
        if (outbound_connection_matches(addr) && d->_handle <= 0)
            return cancel_connection(addr);
        if (d->_handle <= 0)
            return 0;
        PAIR_LOG("BT: disconnecting %s handle=%04X\n", d->_name.c_str(), d->_handle);
        return disconnect(d->_handle);
    }

    int reject_connection_request(const bdaddr_t& addr, uint8_t reason)
    {
        uint8_t buf[7] = {0};
        memcpy(buf,&addr,6);
        buf[6] = reason;
        PAIR_LOG("BT: rejecting inbound connection from %s reason=%u\n",
            batostr(addr), (unsigned)reason);
        return cmd(HCI_REJECT_CONNECTION_REQUEST,buf,sizeof(buf));
    }
private:
    BTDevice* remote_name_response(const remote_name_info* ri)
    {
        auto* d = get_device(&ri->bdaddr);
        if (d) {
            d->_name = ri->name;
            PAIR_LOG("BT: name resolved %s -> %s\n", batostr(ri->bdaddr), ri->name);
        }
        return d;
    }

    int create_connection(BTDevice& d)
    {
        if (d._handle != -1)
            return 0;
        if (_outbound_connection_pending)
            return -2;
        d._flags = 0;
        d._handle = 0;  // connection in progress

        u8 buf[6+7] = {0};
        memcpy(buf,d._addr.b,6);
        const uint16_t packet_types = 0xCC18;
        buf[6] = (uint8_t)packet_types;
        buf[7] = (uint8_t)(packet_types >> 8);
        buf[8] = 1;     // Page Repetition R1
        if (d._page_info_valid) {
            // Use what the inquiry reported: correct scan mode plus the
            // clock offset (bit 15 = valid) cut paging time and failures.
            const uint16_t offset = (uint16_t)((d._clock_offset & 0x7FFF) | 0x8000);
            buf[8] = d._pscan_rep_mode <= 2 ? d._pscan_rep_mode : 1;
            buf[10] = (uint8_t)offset;
            buf[11] = (uint8_t)(offset >> 8);
        }
        PAIR_LOG("BT: creating connection to %s\n", d._name.c_str());
        int rc = cmd(HCI_CREATE_CONNECTION,buf,sizeof(buf));
        if (rc == 0) {
            _outbound_connection_pending = true;
            _outbound_connection_addr = d._addr;
        } else {
            d._handle = -1;
        }
        PAIR_LOG("BT: connection request queued for %s rc=%d\n", d._name.c_str(), rc);
        return rc;
    }

    int switch_role(const bdaddr_t& addr, uint8_t role)
    {
        uint8_t buf[6+1];
        memcpy(buf,addr.b,6);
        buf[6] = role;
        return cmd(HCI_SWITCH_ROLE,buf,sizeof(buf));
    }

    int write_link_policy_settings(uint16_t handle, uint16_t policy)
    {
        uint8_t buf[4] = {
            (uint8_t)handle,
            (uint8_t)(handle >> 8),
            (uint8_t)policy,
            (uint8_t)(policy >> 8),
        };
        return cmd(HCI_WRITE_LINK_POLICY,buf,sizeof(buf));
    }


    // add device to list on inbound connections
    // should only allow connections to stuff we want?
    int connection_request(const connection_request_info* cr)
    {
        add_device(cr->bdaddr,cr->dev_class);   // why do we have devices on both sides?
        auto* d = get_device(&cr->bdaddr);
        if (!d)
            return -1;
        d->_flags |= SLAVE; // assume slave
        PAIR_LOG("BT: inbound connection request %s class=%06X\n", batostr(cr->bdaddr), d->_dev_class);
        return 0;
    }

    const char* _hci_errors[42] = {
        "No Error",
        "Unknown HCI Command",
        "No Connection",
        "Hardware Failure",
        "Page Timeout",
        "Authentication Failure",
        "Key Missing",
        "Memory Full",

        "Connection Timeout",
        "Max Number Of Connections",
        "Max Number Of SCO Connections To A Device",
        "ACL Connection Already Exists",
        "Command Disallowed",
        "Host Rejected Due To Limited Resources",
        "Host Rejected Due To Security Reasons",
        "Host Rejected Due To A Remote Device Only A Personal Device",

        "Host Timeout",
        "Unsupported Feature Or Parameter Value",
        "Invalid HCI Command Parameters",   // 0x12 was missing: names from 0x12 up were off by one
        "Other End Terminated Connection: User Ended Connection",
        "Other End Terminated Connection: Low Resources",
        "Other End Terminated Connection: About To Power Off",
        "Connection Terminated By Local Host",
        "Repeated Attempts",
        "Pairing Not Allowed",

        "Unknown LMP PDU",
        "Unsupported Remote Feature",
        "SCO Offset Rejected",
        "SCO Interval Rejected",
        "SCO Air Mode Rejected",
        "Invalid LMP Parameters",
        "Unspecified Error",
        "Unsupported LMP Parameter",

        "Role Change Not Allowed",
        "LMP Response Timeout",
        "LMP Error Transaction Collision",
        "LMP PDU Not Allowed",
        "Encryption Mode Not Acceptable",
        "Unit Key Used",
        "QoS Not Supported",
        "Instant Passed",

        "Pairing With Unit Key Not Supported",
    };

    const char* hci_status_str(int e)
    {
        const size_t count = sizeof(_hci_errors) / sizeof(_hci_errors[0]);
        if (e >= 0 && (size_t)e < count && _hci_errors[e])
            return _hci_errors[e];
        return "_hci_status_unrecognized";
    }

    void connection_complete(const connection_info* c)
    {
        if (outbound_connection_matches(c->bdaddr))
            clear_outbound_connection_pending(c->status == 0 ? "connected" : "connection-complete-failed");
        auto* d = get_device(&c->bdaddr);
        if (d) {
            if (c->status == 0) {
                PAIR_LOG("BT: connection complete %s handle=%04X\n", d->_name.c_str(), c->handle);
                d->hci_connected(c);
                _callback(CALLBACK_CONNECTION_COMPLETE,c,sizeof(connection_info),_callback_ref);
                d->set_role(HCI_ROLE_MASTER);   // connected as master

                if (d->is_wii_remote()) {
                    write_link_policy_settings((uint16_t)d->_handle,0x0000);
                }

            } else {
                PAIR_LOG("BT: connection failed %s status=%d (%s)\n", d->_name.c_str(), c->status, hci_status_str(c->status));
                d->_handle = -1;
                _callback(CALLBACK_CONNECTION_FAILED,c,sizeof(connection_info),_callback_ref);
            }
        }
    }

    // pressing the "sync" button on the back of the wiimote, then the PIN is the bluetooth address of the host backwards.
    int pincode_reply(const uint8_t* data)
    {
        auto* d = get_device((bdaddr_t*)data);
        uint8_t buf[6+1+16] = {0};
        memcpy(buf,data,6);
        if (d && d->is_wii()) {
            buf[6] = 6;
            memcpy(buf+7,_localAddr.b,6);   // sync button pressed
            PAIR_LOG("BT: Wii PIN reply for %s using local address\n", d->_name.c_str());
        } else {
            buf[6] = 4;
            buf[7] = '0';
            buf[8] = '0';
            buf[9] = '0';
            buf[10] = '0';
            PAIR_LOG("BT: legacy PIN reply for %s using 0000\n", d ? d->_name.c_str() : batostr(*(const bdaddr_t*)data));
            sys_msg("enter '0000' on keyboard to pair");
        }
        return cmd(HCI_PIN_CODE_REQUEST_REPLY,buf,sizeof(buf));
    }

    int io_capability_reply(const bdaddr_t* addr)
    {
        uint8_t buf[9] = {0};
        memcpy(buf, addr->b, 6);
        buf[6] = 0x03;  // NoInputNoOutput: safest host capability for a headless console.
        buf[7] = 0x00;  // OOB data not present.
        buf[8] = 0x04;  // General bonding, MITM not required.
        PAIR_LOG("BT: SSP IO capability reply to %s (NoInputNoOutput)\n", batostr(*addr));
        return cmd(HCI_IO_CAPABILITY_REQUEST_REPLY, buf, sizeof(buf));
    }

    int user_confirmation_reply(const bdaddr_t* addr)
    {
        uint8_t buf[6] = {0};
        memcpy(buf, addr->b, 6);
        PAIR_LOG("BT: SSP user confirmation accepted for %s\n", batostr(*addr));
        return cmd(HCI_USER_CONFIRMATION_REQUEST_REPLY, buf, sizeof(buf));
    }

    int user_passkey_request_neg_reply(const bdaddr_t* addr)
    {
        uint8_t buf[6] = {0};
        memcpy(buf, addr->b, 6);
        PAIR_LOG("BT: SSP passkey request rejected for %s (headless host)\n", batostr(*addr));
        return cmd(HCI_USER_PASSKEY_REQUEST_NEG_REPLY, buf, sizeof(buf));
    }

    int disconnect(int handle)
    {
        // Reason 0x13 (remote user terminated). The old value 0x01 is not a
        // legal disconnect reason, so the controller rejected the command.
        uint8_t buf[3] = { (uint8_t)handle, (uint8_t)(handle >> 8), 0x13 };
        return cmd(HCI_DISCONNECT,buf,sizeof(buf));
    }

    void hci(uint8_t evt, const uint8_t* data, uint8_t len)
    {
        switch (evt) {
            case HCI_INQUIRY_COMP_EVT:
                _state &= ~MASK_INQUIRY;
                PAIR_LOG("BT: inquiry complete (%d result%s)\n",
                    _inquiry_result_count,
                    _inquiry_result_count == 1 ? "" : "s");
                _callback(CALLBACK_INQUIRY_DONE,NULL,0,_callback_ref);
                break;

            case HCI_INQUIRY_RESULT_EVT:
                handle_inquiry_result_evt(data, len);
                break;

            case HCI_INQUIRY_RESULT_WITH_RSSI_EVT:
                handle_inquiry_result_with_rssi_evt(data, len);
                break;

            case HCI_EXTENDED_INQUIRY_RESULT_EVT:
                handle_extended_inquiry_result_evt(data, len);
                break;

            case HCI_CONNECTION_COMP_EVT:
                connection_complete((const connection_info*)data);
                dongle_bug();
                break;

            case HCI_AUTHENTICATION_COMP_EVT:
            {
                auto ac = (const hci_auth_complete*)data;
                auto* d = get_device_by_handle(ac->handle);
                if (d) {
                    PAIR_LOG("BT: authentication %s status=%d (%s)\n",
                        d->_name.c_str(), data[0], hci_status_str(data[0]));
                    authentication_complete_info aci = {d->_addr,data[0]};
                    _callback(CALLBACK_AUTHENTICATION_COMPLETE,&aci,sizeof(aci),_callback_ref);
                }
            }
                break;

            case HCI_ENCRYPTION_CHANGE_EVT:
            {
                const auto* ec = (const hci_encryption_change*)data;
                auto* d = get_device_by_handle(ec->handle);
                PAIR_LOG("BT: encryption change %s status=%u (%s) enabled=%u\n",
                    d ? d->_name.c_str() : "unknown-device",
                    (unsigned)ec->status,
                    hci_status_str(ec->status),
                    (unsigned)ec->enabled);
                if (d) {
                    encryption_change_info eci = {d->_addr,ec->status,ec->enabled};
                    _callback(CALLBACK_ENCRYPTION_CHANGE,&eci,sizeof(eci),_callback_ref);
                }
            }
                break;

            case HCI_CONNECTION_REQUEST_EVT:
            {
                auto* ci = (connection_request_info*)data;
                connection_request(ci);
                _callback(CALLBACK_CONNECTION_REQUEST,data,len,_callback_ref);
            }
                break;

            case HCI_DISCONNECTION_COMP_EVT:
            {
                const hci_disconn_complete* dc = (const hci_disconn_complete*)data;
                auto* d = get_device_by_handle(dc->handle);
                if (d) {
                    PAIR_LOG("BT: disconnected %s status=%u reason=%u (%s)\n",
                        d->_name.c_str(),
                        (unsigned)dc->status,
                        (unsigned)dc->reason,
                        hci_status_str(dc->reason));
                    d->disconnect_all();
                    _callback(CALLBACK_DISCONNECTION_COMP,d->_addr.b,6,_callback_ref);
                }
            }
                break;

            case HCI_RMT_NAME_REQUEST_COMP_EVT:
            {
                if (len >= (int)sizeof(remote_name_info)) {
                    const remote_name_info* ri = (const remote_name_info*)data;
                    PAIR_LOG("BT: name request complete for %s status=%d (%s)\n",
                        batostr(ri->bdaddr), ri->status, hci_status_str(ri->status));
                } else {
                    PAIR_LOG("BT: malformed name request complete event len=%u\n", (unsigned)len);
                    break;
                }
                remote_name_response((const remote_name_info*)data);
                _callback(CALLBACK_REMOTE_NAME,data,len,_callback_ref);
                break;
            }

            case HCI_PIN_CODE_REQUEST_EVT:
                pincode_reply(data);
                break;

            case HCI_IO_CAPABILITY_REQUEST_EVT:
                io_capability_reply((const bdaddr_t*)data);
                break;

            case HCI_IO_CAPABILITY_RESPONSE_EVT:
                if (len >= 9) {
                    PAIR_LOG("BT: SSP peer capability %s io=%u oob=%u auth=%u\n",
                        batostr(*(const bdaddr_t*)data),
                        (unsigned)data[6],
                        (unsigned)data[7],
                        (unsigned)data[8]);
                }
                break;

            case HCI_USER_CONFIRMATION_REQUEST_EVT:
                // Accept Just Works pairing on this headless host.
                user_confirmation_reply((const bdaddr_t*)data);
                break;

            case HCI_USER_PASSKEY_REQUEST_EVT:
                // There is no passkey input, so reject this request.
                user_passkey_request_neg_reply((const bdaddr_t*)data);
                break;

            case HCI_ROLE_CHANGE_EVT:
            {
                auto* rc = (const hci_role_change_evt*)data;
                auto* d = get_device(&rc->peer_addr);
                if (d)
                    d->set_role(rc->role);
            }
                break;

            // reject link keys, go with pins instead
            case HCI_LINK_KEY_REQUEST_EVT:
            {
                // Always answer: an unanswered request stalls the link until
                // the controller times out.
                const auto* a = (const bdaddr_t*)data;
                uint8_t buf[6+16];
                if (len >= 6 && read_link_key(a,buf+6) != -1) {
                    memcpy(buf,data,6);
                    PAIR_LOG("BT: replying to link key request for %s with stored key\n",
                        batostr(*a));
                    cmd(HCI_LINK_KEY_REQUEST_REPLY,buf,6+16);   // got a key!
                } else if (len >= 6) {
                    PAIR_LOG("BT: replying to link key request for %s with negative reply\n",
                        batostr(*a));
                    cmd(HCI_LINK_KEY_REQUEST_NEG_REPLY,data,6); // don't got a key!
                }
            }
                break;

            case HCI_COMMAND_COMPLETE_EVT:
            {
                if (len < 4) {
                    PAIR_LOG("BT: malformed command complete event len=%u\n", (unsigned)len);
                    break;
                }
                int c = data[1] | (data[2] << 8);
                int status = data[3];
                if (c == HCI_CREATE_CONNECTION_CANCEL) {
                    clear_outbound_connection_pending("cancel-complete");
                }
                switch (c) {
                    case HCI_RESET:
                    case HCI_SET_EVENT_MASK:
                    case HCI_READ_BUFFER_SIZE:
                    case HCI_READ_BD_ADDR:
                    case HCI_WRITE_LOCAL_NAME:
                    case HCI_WRITE_CLASS_OF_DEVICE:
                    case HCI_WRITE_SCAN_ENABLE:
                    case HCI_WRITE_SIMPLE_PAIRING_MODE:
                    case HCI_WRITE_AUTH_ENABLE:
                        PAIR_LOG("BT: init command complete %s status=%d (%s)\n",
                            hci_cmd(c), status, hci_status_str(status));
                        break;
                    default:
                        break;
                }
                switch (c) {
                    //  Init phase 0
                    case HCI_RESET:
                    {
                        // Enable the standard and SSP event groups.
                        uint8_t event_mask[8] = {
                            0xFF, 0xFF, 0xFF, 0xFF,
                            0xFF, 0xFF, 0xFF, 0xFF,
                        };
                        cmd(HCI_SET_EVENT_MASK,event_mask,sizeof(event_mask));
                        _state |= MASK_RESET;
                    }
                        break;

                    case HCI_SET_EVENT_MASK:
                        cmd(HCI_READ_BUFFER_SIZE);
                        break;

                    //  Init phase 1
                    case HCI_READ_BUFFER_SIZE:
                        cmd(HCI_READ_BD_ADDR);
                        _buffer_size = *((const hci_buffer_size*)(data+3));
                        _state |= MASK_READ_BUFFER_SIZE;
                        break;

                    //  Init phase 2
                    case HCI_READ_BD_ADDR:
                    {
                        _localAddr = *((bdaddr_t*)(data+4)); // Local Address
                        _state |= MASK_READ_BD_ADDR;

                        uint8_t name[248] = {0};
                        strncpy((char*)name, _localname.c_str(), sizeof(name) - 1);
                        cmd(HCI_WRITE_LOCAL_NAME,name,sizeof(name));
                    }
                        break;

                    //  Init phase 3
                    case HCI_WRITE_LOCAL_NAME:
                    {
                        //uint8_t d[3] = {0x04, 0x05, 0x00};    // what should we look like?
                        uint8_t d[3] = {0x07, 0x02, 0x0C};      // smartphone?
                        cmd(HCI_WRITE_CLASS_OF_DEVICE,d,sizeof(d));
                    }
                        break;

                    case HCI_WRITE_CLASS_OF_DEVICE:
                    {
                        uint8_t enable = 3;
                        cmd(HCI_WRITE_SCAN_ENABLE,&enable,sizeof(enable));
                    }
                        break;

                    case HCI_WRITE_SCAN_ENABLE:
                    {
                        uint8_t enable = 1;
                        cmd(HCI_WRITE_SIMPLE_PAIRING_MODE,&enable,1);
                    }
                        break;

                    case HCI_WRITE_SIMPLE_PAIRING_MODE:
                    {
                        uint8_t a = 0;                      // disable auth
                        cmd(HCI_WRITE_AUTH_ENABLE,&a,1);    // we will manually bond where appropriate
                    }
                        break;

                    case HCI_WRITE_AUTH_ENABLE:
                        _state |= MASK_INITED;
                        _callback(CALLBACK_READY,data+3,1,_callback_ref);
                        break;

                    case HCI_WRITE_LINK_POLICY:
                    {
                        int h = data[4] | (data[5] << 8);   // handle
                        cmd(HCI_ROLE_DISCOVERY,&h,2);       // check my role
                    }
                        break;

                    case HCI_ROLE_DISCOVERY:
                    {
                        auto* ri = (const hci_role_discovery_complete*)(data + 3);
                        auto* d = get_device_by_handle(ri->handle);
                        if (d)
                            d->set_role(ri->role);
                    }
                    break;

                    case HCI_READ_STORED_LINK_KEY:
                        break;
                }
            }
                 break;
            case HCI_HARDWARE_ERROR_EVT:
                PAIR_LOG("BT: hardware error event code=%u\n", len ? (unsigned)data[0] : 0);
                break;
            case HCI_COMMAND_STATUS_EVT:    // after starting inquiry/connect etc
            {
                if (len < 4)
                    break;
                int cmd = data[2] + (data[3] << 8);
                if (cmd == HCI_CREATE_CONNECTION) {
                    if (data[0] != 0) {
                        auto* d = _outbound_connection_pending ?
                            get_device(&_outbound_connection_addr) : 0;
                        clear_outbound_connection_pending("command-status-failed");
                        if (d) {
                            connection_info ci = {};
                            ci.status = data[0];
                            ci.bdaddr = d->_addr;
                            d->_handle = -1;
                            _callback(CALLBACK_CONNECTION_FAILED,&ci,sizeof(ci),_callback_ref);
                        }
                    }
                }
                switch (cmd) {
                    case HCI_INQUIRY:
                    case HCI_INQUIRY_CANCEL:
                    case HCI_CREATE_CONNECTION:
                    case HCI_ACCEPT_CONNECTION_REQUEST:
                    case HCI_AUTHENTICATION_REQUESTED:
                    case HCI_SET_CONNECTION_ENCRYPTION:
                    case HCI_REMOTE_NAME_REQUEST:
                        PAIR_LOG("BT: command status %s %04X status=%d (%s)\n",
                            hci_cmd(cmd),
                            cmd,
                            data[0],
                            hci_status_str(data[0]));
                        if (cmd == HCI_INQUIRY && data[0] != 0) {
                            _state &= ~MASK_INQUIRY;
                            _callback(CALLBACK_INQUIRY_DONE,NULL,0,_callback_ref);
                        }
                        break;
                    default:
                        break;
                }
            }
                break;
            case HCI_QOS_SETUP_COMP_EVT:
                break;
            case HCI_NUM_COMPL_DATA_PKTS_EVT:
                break;
            case HCI_MODE_CHANGE_EVT:
                break;

            case HCI_RETURN_LINK_KEYS_EVT:
                break;

            case HCI_LINK_KEY_NOTIFICATION_EVT:
                PAIR_LOG("BT: stored link key for %s\n", batostr(*(const bdaddr_t*)data));
                write_link_key((const bdaddr_t*)data,data+6);
                break;

            case HCI_SIMPLE_PAIRING_COMPLETE_EVT:
                if (len >= 7) {
                    PAIR_LOG("BT: SSP complete %s status=%d (%s)\n",
                        batostr(*(const bdaddr_t*)(data + 1)), data[0], hci_status_str(data[0]));
                } else {
                    PAIR_LOG("BT: SSP complete status=%d (%s)\n", data[0], hci_status_str(data[0]));
                }
                break;
            default:
                break;
        }
    }

    // USB dongle bug
    vector<uint8_t> _dongle_bug;
    void dongle_bug()
    {
        if (!_dongle_bug.empty()) {
            acl(&_dongle_bug[0],(int)_dongle_bug.size());
            _dongle_bug.clear();
        }
    }

    void acl(const uint8_t* data, int len)
    {
        if (!data || len < 5)
            return;
        l2cap_data* p = (l2cap_data*)data;
        if (p->length > len - 5)
            return;
        int h = p->handle & 0x0FFF;
        for (auto d : _devices) {
            if (h == d->_handle) {
                d->data(p,len);
                return;
            }
        }

        // interesting bug on libusb/simulator where ACL open packet arrives before hci connection complete
        // hci arrives on libusb_fill_interrupt_transfer, acl on libusb_fill_bulk_transfer but acl gets ahead

        _dongle_bug.resize(len);
        memcpy(&_dongle_bug[0],data,len);
    }
};
HCI* _hci = 0;

void hci_socket_change(const bdaddr_t& addr, int scid, int state)
{
    if (!_hci || !_hci->_callback)
        return;
    socket_state s = { addr, scid, state };
    _hci->_callback(CALLBACK_L2CAP_SOCKET_STATE_CHANGED,&s,sizeof(s),_hci->_callback_ref);
}

//==================================================================
//==================================================================

int l2_open(const bdaddr_t* addr, int psm, bool listen)
{
    return _hci->l2_open(addr,psm,listen);
}

int l2_send(int s, const uint8_t* data, int len)
{
    return _hci->l2_send(s, data, len);
}

int l2_recv(int s, uint8_t* data, int len)
{
    return _hci->l2_recv(s, data, len);
}

int l2_close(int s)
{
    return _hci->l2_close(s);
}

int l2_state(int s)
{
    return _hci->l2_state(s);
}

int hci_start_inquiry(int secs)
{
    return _hci->start_inquiry(secs);
}

int hci_cancel_inquiry()
{
    return _hci->cancel_inquiry();
}

int hci_cancel_connection(const bdaddr_t* addr)
{
    return _hci->cancel_connection(*addr);
}

int hci_update()
{
    return _hci->update();
}

// Tear down the HCI layer: devices (and their sockets) first, while _hci is
// still valid for socket-state callbacks, then the transport.
void hci_shutdown()
{
    for (auto* d : _devices) {
        d->disconnect_all();
        delete d;
    }
    _devices.clear();
    HCI* h = _hci;
    _hci = 0;
    delete h;
}

int hci_init(hci_callback cb, void* ref, const char* local_name)
{
    hci_shutdown();     // devices from a previous instance hold dead handles
    _hci = new HCI(local_name);
    return _hci->init(cb,ref);
}

int hci_accept_connection_request(const bdaddr_t* addr)
{
    return _hci->accept_connection_request(*addr);
}

int hci_reject_connection_request(const bdaddr_t* addr, uint8_t reason)
{
    return _hci->reject_connection_request(*addr,reason);
}

int hci_authentication_requested(const bdaddr_t* addr)
{
    return _hci->authentication_requested(*addr);
}

int hci_set_connection_encryption(const bdaddr_t* addr, bool enabled)
{
    return _hci->set_connection_encryption(*addr,enabled);
}

int hci_remote_name_request(const bdaddr_t* addr)
{
    return _hci->remote_name_request(*addr);
}

int hci_connect(const bdaddr_t* addr)
{
    return _hci->connect(addr);
}

int hci_write(const uint8_t* buf, int len)
{
    return _hci->write(buf,len); // send to outbound q.
}

int hci_disconnect_device(const bdaddr_t* addr)
{
    if (!_hci || !addr)
        return -1;
    return _hci->disconnect_device(*addr);
}
