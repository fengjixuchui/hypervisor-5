#include "std_include.hpp"
#include "logging.hpp"
#include "thread.hpp"

#define DOS_DEV_NAME L"\\DosDevices\\HelloDev"
#define DEV_NAME L"\\Device\\HelloDev"

#define HELLO_DRV_IOCTL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)

_Function_class_(DRIVER_DISPATCH)

NTSTATUS IrpNotImplementedHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

	UNREFERENCED_PARAMETER(DeviceObject);
	PAGED_CODE();

	// Complete the request
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_NOT_SUPPORTED;
}

_Function_class_(DRIVER_DISPATCH)

NTSTATUS IrpCreateCloseHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(DeviceObject);
	PAGED_CODE();

	// Complete the request
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

_Function_class_(DRIVER_DISPATCH)
VOID IrpUnloadHandler(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING DosDeviceName = {0};

	PAGED_CODE();

	RtlInitUnicodeString(&DosDeviceName, DOS_DEV_NAME);

	// Delete the symbolic link
	IoDeleteSymbolicLink(&DosDeviceName);

	// Delete the device
	IoDeleteDevice(DriverObject->DeviceObject);

	debug_log("[!] Hello Driver Unloaded\n");
}

_Function_class_(DRIVER_DISPATCH)

NTSTATUS IrpDeviceIoCtlHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	ULONG IoControlCode = 0;
	PIO_STACK_LOCATION IrpSp = NULL;
	NTSTATUS Status = STATUS_NOT_SUPPORTED;

	UNREFERENCED_PARAMETER(DeviceObject);
	PAGED_CODE();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	IoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

	if (IrpSp)
	{
		switch (IoControlCode)
		{
		case HELLO_DRV_IOCTL:
			debug_log("[< HelloDriver >] Hello from the Driver!\n");
			break;
		default:
			debug_log("[-] Invalid IOCTL Code: 0x%X\n", IoControlCode);
			Status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}
	}

	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = 0;

	// Complete the request
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}

NTSTATUS create_io_device(const PDRIVER_OBJECT DriverObject)
{
	UINT32 i = 0;
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	UNICODE_STRING DeviceName, DosDeviceName = {0};

	PAGED_CODE();

	RtlInitUnicodeString(&DeviceName, DEV_NAME);
	RtlInitUnicodeString(&DosDeviceName, DOS_DEV_NAME);

	debug_log("[*] In DriverEntry\n");

	// Create the device
	Status = IoCreateDevice(DriverObject,
	                        0,
	                        &DeviceName,
	                        FILE_DEVICE_UNKNOWN,
	                        FILE_DEVICE_SECURE_OPEN,
	                        FALSE,
	                        &DeviceObject);

	if (!NT_SUCCESS(Status))
	{
		if (DeviceObject)
		{
			// Delete the device
			IoDeleteDevice(DeviceObject);
		}

		debug_log("[-] Error Initializing HelloDriver\n");
		return Status;
	}

	// Assign the IRP handlers
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		// Disable the Compiler Warning: 28169
#pragma warning(push)
#pragma warning(disable : 28169)
		DriverObject->MajorFunction[i] = IrpNotImplementedHandler;
#pragma warning(pop)
	}

	// Assign the IRP handlers for Create, Close and Device Control
	DriverObject->MajorFunction[IRP_MJ_CREATE] = IrpCreateCloseHandler;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = IrpCreateCloseHandler;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpDeviceIoCtlHandler;

	// Set the flags
	DeviceObject->Flags |= DO_DIRECT_IO;
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	// Create the symbolic link
	Status = IoCreateSymbolicLink(&DosDeviceName, &DeviceName);

	// Show the banner
	debug_log("[!] HelloDriver Loaded\n");

	return Status;
}

_Function_class_(CALLBACK_FUNCTION)
VOID
PowerCallback(
	_In_opt_ PVOID CallbackContext,
	_In_opt_ PVOID Argument1,
	_In_opt_ PVOID Argument2
)
{
	UNREFERENCED_PARAMETER(CallbackContext);

	//
	// Ignore non-Sx changes
	//
	if (Argument1 != (PVOID)PO_CB_SYSTEM_STATE_LOCK)
	{
		return;
	}

	//
	// Check if this is S0->Sx, or Sx->S0
	//
	if (ARGUMENT_PRESENT(Argument2))
	{
		//
		// Reload the hypervisor
		//
		debug_log("Waking up!\n");
	}
	else
	{
		//
		// Unload the hypervisor
		//
		debug_log("Going to sleep!\n");
	}
}

PVOID g_PowerCallbackRegistration{nullptr};

NTSTATUS register_sleep_callback()
{
	PCALLBACK_OBJECT callbackObject;
	UNICODE_STRING callbackName =
		RTL_CONSTANT_STRING(L"\\Callback\\PowerState");
	OBJECT_ATTRIBUTES objectAttributes =
		RTL_CONSTANT_OBJECT_ATTRIBUTES(&callbackName,
		                               OBJ_CASE_INSENSITIVE |
		                               OBJ_KERNEL_HANDLE);

	auto status = ExCreateCallback(&callbackObject, &objectAttributes, FALSE, TRUE);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	//
	// Now register our routine with this callback
	//
	g_PowerCallbackRegistration = ExRegisterCallback(callbackObject,
	                                                 PowerCallback,
	                                                 NULL);

	//
	// Dereference it in both cases -- either it's registered, so that is now
	// taking a reference, and we'll unregister later, or it failed to register
	// so we failing now, and it's gone.
	//
	ObDereferenceObject(callbackObject);

	//
	// Fail if we couldn't register the power callback
	//
	if (g_PowerCallbackRegistration == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)

void unload(PDRIVER_OBJECT DriverObject)
{
	debug_log("Leaving World\n");
	IrpUnloadHandler(DriverObject);
	ExUnregisterCallback(g_PowerCallbackRegistration);
}

void throw_test()
{
	try
	{
		throw 1;
	}
	catch (...)
	{
		debug_log("Exception caught!\n");
	}
}

extern "C" NTSTATUS DriverEntry(const PDRIVER_OBJECT DriverObject, PUNICODE_STRING /*RegistryPath*/)
{
	DriverObject->DriverUnload = unload;
	debug_log("Hello World\n");

	volatile long i = 0;

	thread::dispatch_on_all_cores([&i]()
	{
		const auto index = thread::get_processor_index();
		while (i != index)
		{
		}

		debug_log("Hello from CPU %u/%u\n", thread::get_processor_index() + 1, thread::get_processor_count());
		InterlockedIncrement(&i);
	});

	debug_log("Final i = %i\n", i);

	throw_test();
	register_sleep_callback();

	return create_io_device(DriverObject);

	//return STATUS_SUCCESS;
}
