//
// Device type           -- in the "User Defined" range."
//
#define KB_TYPE	60000

//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
#define IOCTL_KB_ENABLE_LBR \
    CTL_CODE( KB_TYPE, 0x900, METHOD_NEITHER, FILE_ANY_ACCESS  )

#define IOCTL_KB_DUMP_LBR \
    CTL_CODE( KB_TYPE, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_KB_CHECK_LBR \
    CTL_CODE( KB_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_KB_DISABLE_LBR \
    CTL_CODE( KB_TYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define DRIVER_FUNC_INSTALL	0x01
#define DRIVER_FUNC_REMOVE	0x02

#define DRIVER_NAME	"kb"

#define LBR_SZ		16 // Nehalem

typedef struct {
	__int64 from;
	__int64 to;
} BR, *PBR;
