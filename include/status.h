#pragma once

#define XKLIB_IOMMU_NOT_PRESENT 0x80000000
#define XKLIB_IOMMU_NOT_LOADED 0x80000001
#define XKLIB_VIRTUALIZATION_FAILED 0x80000002
#define XKLIB_SETUP_FAILED 0x80000003
#define XKLIB_USER_INFO_INVALID 0x80000004
#define XKLIB_SPOOFER_FAILED 0x80000005
#define XKLIB_EPT_FAILED 0x80000006
#define XKLIB_LOAD_FAILED 0x80000007
#define XKLIB_DEBUGGER_DETECTED 0x80000008
#define XKLIB_HYPERVISOR_DETECTED 0x80000009
#define XKLIB_DETECTION(code) (SKLIB_DEBUGGER_DETECTED | (code << 8))

#define XKLIB_ENOMEM 0x80000100
#define XKLIB_EEXIST 0x80000101
#define XKLIB_ENOENT 0x80000102