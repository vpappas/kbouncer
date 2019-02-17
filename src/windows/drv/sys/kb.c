/*
 * Kernel Bouncer (kb) Windws Driver, based on the simple ioctl sample
 * (WDF version) from the WinDDK.
 */


#include <ntddk.h>
#include <string.h>

#include "kb.h"
#include "..\..\..\common\decode.h"

//XXX: SOURCES file does not accept sources from other directories .. that's
//why the .c is inlcuded here
#include "..\..\..\common\decode.c"


#define LOG(x, ...)
//XXX enable for debugging
//#define LOG(msg, ...)	\
//	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, msg, __VA_ARGS__)

#define NT_DEVICE_NAME      L"\\Device\\kb"
#define DOS_DEVICE_NAME     L"\\DosDevices\\kb"

/* MSR register values for Nehalem */
#define MSR_IA32_DS_AREA		0x00000600
#define MSR_LBR_SELECT			0x000001c8
#define MSR_IA32_DEBUGCTL		0x000001d9
#define MSR_LASTBRANCH_TOS		0x000001c9
#define MSR_IA32_LASTBRANCH_0_FROM_IP	0x00000680
#define MSR_IA32_LASTBRANCH_0_TO_IP	0x000006c0

/* IA32_DEBUGCTL bits for Nehalem: */
#define IA32_DEBUGCTL_LBR			(1UL <<  0)
#define IA32_DEBUGCTL_BTF			(1UL <<  1)
#define IA32_DEBUGCTL_TR			(1UL <<  6)
#define IA32_DEBUGCTL_BTS			(1UL <<  7)
#define IA32_DEBUGCTL_BTINT			(1UL <<  8)
#define IA32_DEBUGCTL_BTS_OFF_OS		(1UL <<  9)
#define IA32_DEBUGCTL_BTS_OFF_USR		(1UL << 10)
#define IA32_DEBUGCTL_FREEZE_LBRS_ON_PMI	(1UL << 11)
#define IA32_DEBUGCTL_FREEZE_PERFMON_ON_PMI	(1UL << 12)
#define IA32_DEBUGCTL_UNCORE_PMI_EN		(1UL << 13)
#define IA32_DEBUGCTL_FREEZE_WHILE_SMM_EN	(1UL << 14)

/* LBR_SELECT bits for Nehalem: */
#define LBR_SELECT_CPL_EQ_0		(1UL << 0)
#define LBR_SELECT_CPL_NEQ_0		(1UL << 1)
#define LBR_SELECT_JCC			(1UL << 2)
#define LBR_SELECT_NEAR_REL_CALL	(1UL << 3)
#define LBR_SELECT_NEAR_IND_CALL	(1UL << 4)
#define LBR_SELECT_NEAR_RET		(1UL << 5)
#define LBR_SELECT_NEAR_IND_JMP		(1UL << 6)
#define LBR_SELECT_NEAR_REL_JMP		(1UL << 7)
#define LBR_SELECT_FAR_BRANCH		(1UL << 8)


#define ITERATE_LBR(from, to, tos, i)	for (			\
	tos = __readmsr(MSR_LASTBRANCH_TOS) & (LBR_SZ - 1),	\
	from = __readmsr(MSR_IA32_LASTBRANCH_0_FROM_IP + tos) & (~0ULL>>1),\
	to = __readmsr(MSR_IA32_LASTBRANCH_0_TO_IP + tos),	\
	i=0; i < LBR_SZ; i++,					\
	tos = (tos + 1) % LBR_SZ,				\
	from = __readmsr(MSR_IA32_LASTBRANCH_0_FROM_IP + tos) & (~0ULL>>1),\
	to = __readmsr(MSR_IA32_LASTBRANCH_0_TO_IP + tos))	\

//XXX: all BTS related stuff were delete in revision 48


//
// Device driver routine declarations.
//

DRIVER_INITIALIZE DriverEntry;

__drv_dispatchType(IRP_MJ_CREATE)
	__drv_dispatchType(IRP_MJ_CLOSE)
	DRIVER_DISPATCH
	KBCreateClose;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
	DRIVER_DISPATCH
	KBDeviceControl;

DRIVER_UNLOAD KBUnloadDriver;


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, KBCreateClose)
#pragma alloc_text(PAGE, KBDeviceControl)
#pragma alloc_text(PAGE, KBUnloadDriver)
#endif // ALLOC_PRAGMA


NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT   DriverObject,
	__in PUNICODE_STRING      RegistryPath
	)
	/*++

	  Routine Description:
	  This routine is called by the kernel to initialize the driver.

	  It creates the device object, fills in the dispatch entry points and
	  completes the initialization.

	  Arguments:
	  DriverObject - a pointer to the object that represents this device
	  driver.

	  RegistryPath - a pointer to our Services key in the registry.

	  Return Value:
	  STATUS_SUCCESS if initialized; an error otherwise.

	  --*/

{
	NTSTATUS        ntStatus;
	UNICODE_STRING  ntUnicodeString;	// NT Device Name "\Device\kb"
	UNICODE_STRING  ntWin32NameString;	// Win32 Name "\DosDevices\kb"
	PDEVICE_OBJECT  deviceObject = NULL;	// ptr to device object

	UNREFERENCED_PARAMETER(RegistryPath);

	RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

	ntStatus = IoCreateDevice(
			DriverObject,	// Our Driver Object
			0,		// We don't use a device extension
			&ntUnicodeString,	// Device name "\Device\kb"
			FILE_DEVICE_UNKNOWN,	// Device type
			FILE_DEVICE_SECURE_OPEN,// Device characteristics
			FALSE,		// Not an exclusive device
			&deviceObject);	// Returned ptr to Device Object

	if (!NT_SUCCESS(ntStatus)) {
		LOG("kb.sys: Couldn't create the device object\n");
		return ntStatus;
	}

	//
	// Initialize the driver object with this driver's entry points.
	//
	DriverObject->MajorFunction[IRP_MJ_CREATE] = KBCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = KBCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KBDeviceControl;
	DriverObject->DriverUnload = KBUnloadDriver;

	//
	// Initialize a Unicode String containing the Win32 name
	// for our device.
	//
	RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

	//
	// Create a symbolic link between our device name  and the Win32 name
	//
	ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

	if (!NT_SUCCESS(ntStatus)) {
		//
		// Delete everything that this routine has allocated.
		//
		LOG("kb.sys: Couldn't create symbolic link\n");
		IoDeleteDevice(deviceObject);
	}

	return ntStatus;
}


NTSTATUS
KBCreateClose(
		PDEVICE_OBJECT DeviceObject,
		PIRP Irp
	     )
/*++

  Routine Description:

  This routine is called by the I/O system when the SIOCTL is opened or
  closed.

  No action is performed other than completing the request successfully.

  Arguments:

  DeviceObject - a pointer to the object that represents the device
  that I/O is to be done on.

  Irp - a pointer to the I/O Request Packet for this request.

  Return Value:

  NT status code

  --*/

{
	UNREFERENCED_PARAMETER(DeviceObject);

	PAGED_CODE();

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID
KBUnloadDriver(
		__in PDRIVER_OBJECT DriverObject
	      )
/*++

  Routine Description:

  This routine is called by the I/O system to unload the driver.

  Any resources previously allocated must be freed.

  Arguments:

  DriverObject - a pointer to the object that represents our driver.

  Return Value:

  None
  --*/

{
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
	UNICODE_STRING uniWin32NameString;

	PAGED_CODE();

	//
	// Create counted string version of our Win32 device name.
	//
	RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);


	//
	// Delete the link from our dev name to a name in the Win32 namespace.
	//
	IoDeleteSymbolicLink(&uniWin32NameString);

	if (deviceObject != NULL)
		IoDeleteDevice(deviceObject);
}

NTSTATUS
KBDeviceControl(
		PDEVICE_OBJECT DeviceObject,
		PIRP Irp
	       )
/*++

  Routine Description:

  This routine is called by the I/O system to perform a device I/O
  control function.

  Arguments:

  DeviceObject - a pointer to the object that represents the device
  that I/O is to be done on.

  Irp - a pointer to the I/O Request Packet for this request.

  Return Value:

  NT status code

  --*/

{
	PIO_STACK_LOCATION irpSp;		// Current stack location
	NTSTATUS ntStatus = STATUS_SUCCESS;	// Assume success
	ULONG inBufLength;			// Input buffer length
	ULONG outBufLength;			// Output buffer length
	__int64 from, to, tos;
//	CHAR c=0;
	int bad_br, faults=0;
	ULONG i;
	PBR pbr;

	UNREFERENCED_PARAMETER(DeviceObject);

	PAGED_CODE();

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

	//
	// Determine which I/O control code was specified.
	//
	switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
	{
		//
		// Enable Last Branch Recording. This option makes more
		// sense on the Nehalem and later chips that provide a
		// stack of at least 16 pairs and also support branch
		// filtering (LBR_SELECT).
		//
		case IOCTL_KB_ENABLE_LBR:

			LOG("kb.sys: Called IOCTL_KB_ENABLE_LBR\n");

			__writemsr(MSR_LBR_SELECT,
					LBR_SELECT_CPL_EQ_0	|
					//LBR_SELECT_CPL_NEQ_0	|
					LBR_SELECT_JCC		|
					LBR_SELECT_NEAR_REL_CALL|
					LBR_SELECT_NEAR_IND_CALL|
					//LBR_SELECT_NEAR_RET	|
					//LBR_SELECT_NEAR_IND_JMP	|
					LBR_SELECT_NEAR_REL_JMP	|
					LBR_SELECT_FAR_BRANCH);


			__writemsr(MSR_IA32_DEBUGCTL, IA32_DEBUGCTL_LBR);

			faults = 0;

			Irp->IoStatus.Information = 0;

			break;

		//
		// Dump the contents of the LBR stack. LBR must have been
		// enabled. 
		//
		case IOCTL_KB_DUMP_LBR:

			LOG("kb.sys: Called IOCTL_KB_DUMP_LBR\n");		

			if (Irp->AssociatedIrp.SystemBuffer == NULL) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			if (outBufLength < sizeof(BR) * LBR_SZ) {
				ntStatus = STATUS_BUFFER_TOO_SMALL;
				break;
			}


			pbr = (PBR) Irp->AssociatedIrp.SystemBuffer;

			ITERATE_LBR(from, to, tos, i) {
				LOG("%2d: %p -> %p\n", i, from, to);
				pbr[i].from = from;
				pbr[i].to = to;
			}

			Irp->IoStatus.Information = sizeof(BR) * LBR_SZ;

			break;

		//
		// Check the Last Branch Records. For each branch in LBR
		// check if the instruction previous to the destination is
		// a call. If not, stop and return the current branch.
		//
		case IOCTL_KB_CHECK_LBR:

			LOG("kb.sys: Called IOCTL_KB_CHECK_LBR\n");

			if (Irp->AssociatedIrp.SystemBuffer == NULL) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			if (outBufLength < sizeof(BR)) {
				ntStatus = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			bad_br = FALSE;
//#if 1 //null req
			ITERATE_LBR(from, to, tos, i) {
				try {
					ProbeForRead((PUCHAR)from, MAX_INSN, 1);
					ProbeForRead((PUCHAR)to-MAX_INSN,
							MAX_INSN, 1);
//#if 1 //lbr
					if ((bad_br=check_branch((PUCHAR)from,
							(PUCHAR)to)) != 0)
						break;
//#endif
//					c += *(PUCHAR)from;
//					c += *(PUCHAR)to;
				} except (EXCEPTION_EXECUTE_HANDLER) {
					faults += 1;
				}
			}
//#if 1 //lbr

			if (bad_br) {
				LOG("bad br %llx -> %llx\n", from, to);
				pbr = (PBR) Irp->AssociatedIrp.SystemBuffer;
				pbr->from = from;
				pbr->to = to;
				Irp->IoStatus.Information = sizeof(BR);
			}
			else
//#endif
				Irp->IoStatus.Information = 0;

			break;

		//
		// Disable Last Branch Recording.
		//
		case IOCTL_KB_DISABLE_LBR:

			LOG("kb.sys: Called IOCTL_KB_DISABLE_LBR\n");

			__writemsr(MSR_LBR_SELECT, 0);

			__writemsr(MSR_IA32_DEBUGCTL, 0);

			if (outBufLength >= sizeof(faults)) {
				*(int*)Irp->AssociatedIrp.SystemBuffer = faults;
				Irp->IoStatus.Information = sizeof(faults);
			}
			else
				Irp->IoStatus.Information = 0;

			break;

		//
		// The specified I/O control code is unrecognized.
		//
		default:

			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			LOG("kb.sys: ERROR: unrecognized IOCTL %x\n",
					irpSp->Parameters.
					DeviceIoControl.IoControlCode);
			break;
	}

	//
	// Finish the I/O operation by simply completing the packet and return
	// the same status as in the packet itself.
	//
	Irp->IoStatus.Status = ntStatus;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}
