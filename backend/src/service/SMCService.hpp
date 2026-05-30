#ifndef SMCService_hpp
#define SMCService_hpp

#include <IOKit/IOKitLib.h>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

// ---------------------------------------------------------------------------
// SMC kernel selector – must match the AppleSMC driver's dispatch table index
// ---------------------------------------------------------------------------
static const uint32_t kSMCKernelIndex = 2;

// SMC command codes sent in data8
enum SMCCmd : uint8_t {
    kSMCReadKey        = 5,
    kSMCGetKeyCount    = 7,
    kSMCGetKeyFromIndex = 8,
    kSMCGetKeyInfo     = 9,
};

// ---------------------------------------------------------------------------
// Wire structs – must exactly match the kernel ABI used by AppleSMC
// ---------------------------------------------------------------------------
struct SMCKeyData_vers_t {
    uint8_t  major, minor, build;
    uint8_t  reserved[1];
    uint16_t release;
};

struct SMCKeyData_pLimitData_t {
    uint16_t version, length;
    uint32_t cpuPLimit, gpuPLimit, memPLimit;
};

struct SMCKeyData_keyInfo_t {
    uint32_t dataSize;
    uint32_t dataType;
    uint8_t  dataAttributes;
};

struct SMCKeyData_t {
    uint32_t                  key;
    SMCKeyData_vers_t         vers;
    SMCKeyData_pLimitData_t   pLimitData;
    SMCKeyData_keyInfo_t      keyInfo;
    uint8_t                   result;
    uint8_t                   status;
    uint8_t                   data8;
    uint32_t                  data32;
    uint8_t                   bytes[32];
};

// ---------------------------------------------------------------------------
// SMCService – opens AppleSMC and dynamically finds the CPU temperature key
// ---------------------------------------------------------------------------
class SMCService {
private:
    io_connect_t m_conn  = 0;
    bool         m_open  = false;

    // Cached best CPU temperature key (empty = not found)
    std::string  m_cpuTempKey;

    // Pack 4-char ASCII string into a big-endian uint32 key
    static uint32_t toKey(const char* s) {
        return ((uint32_t)(uint8_t)s[0] << 24) |
               ((uint32_t)(uint8_t)s[1] << 16) |
               ((uint32_t)(uint8_t)s[2] <<  8) |
                (uint32_t)(uint8_t)s[3];
    }

    static std::string fromKey(uint32_t k) {
        std::string s(4, '\0');
        s[0] = (char)((k >> 24) & 0xFF);
        s[1] = (char)((k >> 16) & 0xFF);
        s[2] = (char)((k >>  8) & 0xFF);
        s[3] = (char)( k        & 0xFF);
        return s;
    }

    kern_return_t call(SMCKeyData_t* in, SMCKeyData_t* out) {
        size_t inSz  = sizeof(SMCKeyData_t);
        size_t outSz = sizeof(SMCKeyData_t);
        return IOConnectCallStructMethod(m_conn, kSMCKernelIndex, in, inSz, out, &outSz);
    }

    // Two-step SMC read: get key info, then read bytes
    bool readKey(const char* key, SMCKeyData_t& out) {
        if (!m_open) return false;

        // Step 1: get key info (data size + type)
        SMCKeyData_t inp = {};
        SMCKeyData_t ki  = {};
        inp.key   = toKey(key);
        inp.data8 = kSMCGetKeyInfo;
        if (call(&inp, &ki) != kIOReturnSuccess) return false;
        if (ki.keyInfo.dataSize == 0)            return false;

        // Step 2: read actual bytes
        SMCKeyData_t rd = {};
        rd.key              = toKey(key);
        rd.keyInfo.dataSize = ki.keyInfo.dataSize;
        rd.data8            = kSMCReadKey;
        if (call(&rd, &out) != kIOReturnSuccess) return false;

        // Copy back the key info so callers can inspect type/size
        out.keyInfo = ki.keyInfo;
        return true;
    }

    // Same but accepts std::string
    bool readKey(const std::string& key, SMCKeyData_t& out) {
        return readKey(key.c_str(), out);
    }

    // Decode sp78: signed 7.8 fixed-point, big-endian two bytes
    static double sp78(const uint8_t* b) {
        return (int8_t)b[0] + b[1] / 256.0;
    }

    // Decode flt: IEEE 754 single, little-endian four bytes (Apple Silicon)
    static double flt(const uint8_t* b) {
        float f;
        memcpy(&f, b, 4);
        return (double)f;
    }

    // Decode a raw SMC value to °C; returns -1 on invalid type or out-of-range
    double decodeTempKey(const SMCKeyData_t& out) {
        uint32_t dt = out.keyInfo.dataType;
        double temp = -1.0;
        if (dt == toKey("sp78")) {
            temp = sp78(out.bytes);
        } else if (dt == toKey("flt ")) {
            temp = flt(out.bytes);
        } else {
            // Unknown type — try flt then sp78 heuristic
            temp = flt(out.bytes);
            if (temp < 1.0 || temp >= 150.0) {
                temp = sp78(out.bytes);
            }
        }
        return (temp > 1.0 && temp < 150.0) ? temp : -1.0;
    }

    // Priority order for CPU temperature keys
    int keyPriority(const std::string& key) {
        // Lower score = higher priority
        // P-core cluster keys are most representative of chip temp
        static const struct { const char* key; int score; } kPrio[] = {
            {"TC0P", 0}, {"TC0D", 1},  // Intel package
            {"Tp09", 2}, {"Tp0P", 3},  // Apple Silicon P-core cluster
            {"Tp00", 4}, {"Tp04", 5}, {"Tp08", 6}, {"Tp0C", 7}, // Apple Silicon Tp variations
            {"Tp01", 8}, {"Tp05", 9}, {"Tp0D", 10}, {"Tp0H", 11},
            {"Tp0X", 12}, {"Tp0b", 13}, {"Tp0r", 14}, {"Tp0p", 15},
            {"Tf09",16}, {"Tf0D",17}, {"Tf0H",18}, {"Tf0J",19},
            {"Tf0P",20}, {"Tf0b",21}, {"Tf0r",22}, {"Tf0p",23},
        };
        for (auto& p : kPrio) {
            if (key == p.key) return p.score;
        }
        // If it starts with Tp (performance core), score is 30, else 40
        if (key.length() >= 2 && key[0] == 'T' && key[1] == 'p') return 30;
        return 99; // Unknown T* key
    }

    // Enumerate all SMC keys starting with 'T', test which return valid temps,
    // pick the highest-priority one as the CPU temperature key
    void discoverCpuTempKey() {
        if (!m_open) return;

        // Get total key count from #KEY
        SMCKeyData_t out = {};
        if (!readKey("#KEY", out)) return;
        
        // #KEY returns a ui32 (big-endian) containing the number of keys
        uint32_t keyCount = (out.bytes[0] << 24) | (out.bytes[1] << 16) | 
                            (out.bytes[2] << 8)  | out.bytes[3];

        // Sanity check keyCount (typically ~2000-5000)
        if (keyCount == 0 || keyCount > 100000) return;

        std::string bestKey;
        int bestScore = 100;

        for (uint32_t idx = 0; idx < keyCount; ++idx) {
            SMCKeyData_t ki = {}, ko = {};
            ki.data8  = kSMCGetKeyFromIndex;
            ki.data32 = idx;
            if (call(&ki, &ko) != kIOReturnSuccess) continue;

            std::string name = fromKey(ko.key);
            if (name.empty() || name[0] != 'T') continue;

            // Check if it returns a valid temperature
            SMCKeyData_t rd = {};
            if (!readKey(name, rd)) continue;
            double t = decodeTempKey(rd);
            if (t < 0) continue;

            int score = keyPriority(name);
            if (score < bestScore) {
                bestScore = score;
                bestKey   = name;
            }
        }

        m_cpuTempKey = bestKey;
        if (!bestKey.empty()) {
            std::cerr << "[SMC] CPU temperature key: " << bestKey << std::endl;
        } else {
            std::cerr << "[SMC] No CPU temperature key found (SMC open=" << m_open << ")" << std::endl;
        }
    }

public:
    SMCService() {
        io_service_t svc = IOServiceGetMatchingService(
            kIOMainPortDefault, IOServiceMatching("AppleSMC"));
        if (!svc) {
            std::cerr << "[SMC] AppleSMC service not found" << std::endl;
            return;
        }
        kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &m_conn);
        IOObjectRelease(svc);
        m_open = (kr == kIOReturnSuccess);
        if (!m_open) {
            std::cerr << "[SMC] IOServiceOpen failed: " << kr << std::endl;
            return;
        }
        discoverCpuTempKey();
    }

    ~SMCService() {
        if (m_open) IOServiceClose(m_conn);
    }

    // Returns CPU temperature in °C, or -1.0 if unavailable
    double getCpuTemperature() {
        if (!m_open || m_cpuTempKey.empty()) return -1.0;

        SMCKeyData_t out = {};
        if (!readKey(m_cpuTempKey, out)) return -1.0;
        return decodeTempKey(out);
    }

    bool isAvailable() const { return m_open && !m_cpuTempKey.empty(); }
};

#endif // SMCService_hpp
