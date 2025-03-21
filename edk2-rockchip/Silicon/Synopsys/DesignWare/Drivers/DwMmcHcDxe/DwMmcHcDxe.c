/** @file
  This driver is used to manage Designware SD/MMC host controller.

  It would expose EFI_SD_MMC_PASS_THRU_PROTOCOL for upper layer use.

  Copyright (c) 2015 - 2021, Intel Corporation. All rights reserved.
  Copyright (C) 2016 Marvell International Ltd. All rigths reserved.
  Copyright (C) 2018, Linaro Ltd. All rigths reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <Protocol/DevicePath.h>
#include <Protocol/PlatformDwMmc.h>

#include "DwMmcHcDxe.h"

//
// Driver Global Variables
//
EFI_DRIVER_BINDING_PROTOCOL  gDwMmcHcDriverBinding = {
  DwMmcHcDriverBindingSupported,
  DwMmcHcDriverBindingStart,
  DwMmcHcDriverBindingStop,
  0x10,
  NULL,
  NULL
};

//
// Template for Designware SD/MMC host controller private data.
//
DW_MMC_HC_PRIVATE_DATA  gDwMmcHcTemplate = {
  DW_MMC_HC_PRIVATE_SIGNATURE,                           // Signature
  NULL,                                                  // ControllerHandle
  0x0,                                                   // Mmio base address
  {                                 // PassThru
    sizeof (UINT32),
    DwMmcPassThruPassThru,
    DwMmcPassThruGetNextSlot,
    DwMmcPassThruBuildDevicePath,
    DwMmcPassThruGetSlotNumber,
    DwMmcPassThruResetDevice
  },
  NULL,                                                  // PlatformDwMmc
  0,                                                     // PreviousSlot
  NULL,                                                  // TimerEvent
  NULL,                                                  // ConnectEvent
                                                         // Queue
  INITIALIZE_LIST_HEAD_VARIABLE (gDwMmcHcTemplate.Queue),
  {                                 // Slot
    { 0,                                                 UnknownSlot,0, 0, 0 }
  },
  {                                 // Capability
    { 0 }
  },
  {                                 // MaxCurrent
    0
  },
  0                                 // ControllerVersion
};

SD_DEVICE_PATH  mSdDpTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_SD_DP,
    {
      (UINT8)(sizeof (SD_DEVICE_PATH)),
      (UINT8)((sizeof (SD_DEVICE_PATH)) >> 8)
    }
  },
  0
};

EMMC_DEVICE_PATH  mEmmcDpTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_EMMC_DP,
    {
      (UINT8)(sizeof (EMMC_DEVICE_PATH)),
      (UINT8)((sizeof (EMMC_DEVICE_PATH)) >> 8)
    }
  },
  0
};

//
// Prioritized function list to detect card type.
// User could add other card detection logic here.
//
DWMMC_CARD_TYPE_DETECT_ROUTINE  mCardTypeDetectRoutineTable[] = {
  EmmcIdentification,
  SdCardIdentification,
  NULL
};

/**
  The entry point for SD host controller driver, used to install this driver on the ImageHandle.

  @param[in]  ImageHandle   The firmware allocated handle for this driver image.
  @param[in]  SystemTable   Pointer to the EFI system table.

  @retval EFI_SUCCESS   Driver loaded.
  @retval other         Driver not loaded.

**/
EFI_STATUS
EFIAPI
InitializeDwMmcHcDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gDwMmcHcDriverBinding,
             ImageHandle,
             &gDwMmcHcComponentName,
             &gDwMmcHcComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Call back function when the timer event is signaled.

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.

**/
VOID
EFIAPI
ProcessAsyncTaskList (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DW_MMC_HC_PRIVATE_DATA               *Private;
  LIST_ENTRY                           *Link;
  DW_MMC_HC_TRB                        *Trb;
  EFI_STATUS                           Status;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  *Packet;
  BOOLEAN                              InfiniteWait;
  EFI_EVENT                            TrbEvent;

  Private = (DW_MMC_HC_PRIVATE_DATA *)Context;

  //
  // Check if the first entry in the async I/O queue is done or not.
  //
  Status = EFI_SUCCESS;
  Trb    = NULL;
  Link   = GetFirstNode (&Private->Queue);
  if (!IsNull (&Private->Queue, Link)) {
    Trb = DW_MMC_HC_TRB_FROM_THIS (Link);
    if (!Private->Slot[Trb->Slot].MediaPresent) {
      Status = EFI_NO_MEDIA;
      goto Done;
    }

    if (!Trb->Started) {
      //
      // Check whether the cmd/data line is ready for transfer.
      //
      Status = DwMmcCheckTrbEnv (Private, Trb);
      if (!EFI_ERROR (Status)) {
        Trb->Started = TRUE;
        Status       = DwMmcExecTrb (Private, Trb);
        if (EFI_ERROR (Status)) {
          goto Done;
        }
      } else {
        goto Done;
      }
    }

    Status = DwMmcCheckTrbResult (Private, Trb);
  }

Done:
  if ((Trb != NULL) && (Status == EFI_NOT_READY)) {
    Packet = Trb->Packet;
    if (Packet->Timeout == 0) {
      InfiniteWait = TRUE;
    } else {
      InfiniteWait = FALSE;
    }

    if ((!InfiniteWait) && (Trb->Timeout-- == 0)) {
      RemoveEntryList (Link);
      Trb->Packet->TransactionStatus = EFI_TIMEOUT;
      TrbEvent                       = Trb->Event;
      DwMmcFreeTrb (Trb);
      DEBUG ((
        DEBUG_VERBOSE,
        "ProcessAsyncTaskList(): Signal Event %p EFI_TIMEOUT\n",
        TrbEvent
        ));
      gBS->SignalEvent (TrbEvent);
      return;
    }
  }

  if ((Trb != NULL) && (Status != EFI_NOT_READY)) {
    RemoveEntryList (Link);
    Trb->Packet->TransactionStatus = Status;
    TrbEvent                       = Trb->Event;
    DwMmcFreeTrb (Trb);
    DEBUG ((
      DEBUG_VERBOSE,
      "ProcessAsyncTaskList(): Signal Event %p with %r\n",
      TrbEvent,
      Status
      ));
    gBS->SignalEvent (TrbEvent);
  }

  return;
}

/**
  Sd removable device enumeration callback function when the timer event is signaled.

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.

**/
VOID
EFIAPI
DwMmcHcEnumerateDevice (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DW_MMC_HC_PRIVATE_DATA          *Private;
  EFI_STATUS                      Status;
  BOOLEAN                         MediaPresent;
  BOOLEAN                         MediaChanged;
  UINT32                          RoutineNum;
  DWMMC_CARD_TYPE_DETECT_ROUTINE  *Routine;
  UINTN                           Index;
  LIST_ENTRY                      *Link;
  LIST_ENTRY                      *NextLink;
  DW_MMC_HC_TRB                   *Trb;
  EFI_TPL                         OldTpl;

  Private = (DW_MMC_HC_PRIVATE_DATA *)Context;

  if ((Private->Slot[0].Enable) &&
      (Private->Slot[0].SlotType == RemovableSlot))
  {
    Status = DwMmcHcCardDetect (
               Private->DevBase,
               Private->ControllerHandle,
               0,
               &MediaPresent
               );

    MediaChanged = Private->Slot[0].MediaPresent != MediaPresent;

    if (MediaChanged && !MediaPresent) {
      DEBUG ((
        DEBUG_INFO,
        "DwMmcHcEnumerateDevice: device disconnected at %p\n",
        Private->DevBase
        ));
      Private->Slot[0].MediaPresent = FALSE;
      //
      // Signal all async task events at the slot with EFI_NO_MEDIA status.
      //
      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      for (Link = GetFirstNode (&Private->Queue);
           !IsNull (&Private->Queue, Link);
           Link = NextLink)
      {
        NextLink = GetNextNode (&Private->Queue, Link);
        Trb      = DW_MMC_HC_TRB_FROM_THIS (Link);
        if (Trb->Slot == 0) {
          RemoveEntryList (Link);
          Trb->Packet->TransactionStatus = EFI_NO_MEDIA;
          gBS->SignalEvent (Trb->Event);
          DwMmcFreeTrb (Trb);
        }
      }

      gBS->RestoreTPL (OldTpl);
      //
      // Notify the upper layer the connect state change through
      // ReinstallProtocolInterface.
      //
      gBS->ReinstallProtocolInterface (
             Private->ControllerHandle,
             &gEfiSdMmcPassThruProtocolGuid,
             &Private->PassThru,
             &Private->PassThru
             );
    }

    if (MediaChanged && MediaPresent) {
      DEBUG ((
        DEBUG_INFO,
        "DwMmcHcEnumerateDevice: device connected at %p\n",
        Private->DevBase
        ));
      //
      // Initialize slot and start identification process for the new
      // attached device
      //
      Status = DwMmcHcInitHost (Private->DevBase, Private->Capability[0]);
      if (EFI_ERROR (Status)) {
        return;
      }

      //
      // Reset the specified slot of the SD/MMC Pci Host Controller
      //
      Status = DwMmcHcReset (Private->DevBase, Private->Capability[0]);
      if (EFI_ERROR (Status)) {
        return;
      }

      Private->Slot[0].MediaPresent = TRUE;
      RoutineNum                    = sizeof (mCardTypeDetectRoutineTable) /
                                      sizeof (DWMMC_CARD_TYPE_DETECT_ROUTINE);
      for (Index = 0; Index < RoutineNum; Index++) {
        Routine = &mCardTypeDetectRoutineTable[Index];
        if (*Routine != NULL) {
          Status = (*Routine)(Private);
          if (!EFI_ERROR (Status)) {
            break;
          }
        }
      }

      //
      // This card doesn't get initialized correctly.
      //
      if (Index == RoutineNum) {
        return;
      }

      //
      // Notify the upper layer the connect state change through
      // ReinstallProtocolInterface.
      //
      gBS->ReinstallProtocolInterface (
             Private->ControllerHandle,
             &gEfiSdMmcPassThruProtocolGuid,
             &Private->PassThru,
             &Private->PassThru
             );
    }
  }

  return;
}

/**
  Reset the specified SD/MMC host controller and enable all interrupts.

  @param[in] DevBase        The Mmio Device Base Address.

  @retval EFI_SUCCESS       The software reset executes successfully.
  @retval Others            The software reset fails.

**/
EFI_STATUS
DwMmcHcReset (
  IN UINTN               DevBase,
  IN DW_MMC_HC_SLOT_CAP  Capability
  )
{
  EFI_STATUS  Status;
  UINT32      BlkSize;

  //
  // Enable all interrupt after reset all.
  //
  Status = DwMmcHcEnableInterrupt (DevBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "DwMmcHcReset: enable interrupts fail: %r\n", Status));
    return Status;
  }

  Status = DwMmcHcInitTimeoutCtrl (DevBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BlkSize = DW_MMC_BLOCK_SIZE;
  MmioWrite32 (DevBase + DW_MMC_BLKSIZ, BlkSize);

  Status = DwMmcHcInitClockFreq (DevBase, Capability);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DwMmcHcSetBusWidth (DevBase, FALSE, 1);

  return Status;
}

/**
  Tests to see if this driver supports a given controller. If a child device
  is provided, it further tests to see if this driver supports creating a
  handle for the specified child device.

  This function checks to see if the driver specified by This supports the
  device specified by ControllerHandle. Drivers will typically use the device
  path attached to ControllerHandle and/or the services from the bus I/O
  abstraction attached to ControllerHandle to determine if the driver supports
  ControllerHandle. This function may be called many times during platform
  initialization. In order to reduce boot times, the tests performed by this
  function must be very small, and take as little time as possible to execute.
  This function must not change the state of any hardware devices, and this
  function must be aware that the device specified by ControllerHandle may
  already be managed by the same driver or a different driver. This function
  must match its calls to AllocatePages() with FreePages(), AllocatePool() with
  FreePool(), and OpenProtocol() with CloseProtocol(). Since ControllerHandle
  may have been previously started by the same driver, if a protocol is already
  in the opened state, then it must not be closed with CloseProtocol(). This is
  required to guarantee the state of ControllerHandle is not modified by this
  function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL
                                   instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This
                                   handle must support a protocol interface that
                                   supplies an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a
                                   device path.  This parameter is ignored by
                                   device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter
                                   is not NULL, then the bus driver must deter-
                                   mine if the bus controller specified by
                                   ControllerHandle and the child controller
                                   specified by RemainingDevicePath are both
                                   supported by this bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the
                                   driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed
                                   by the driver specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed
                                   by a different driver or an application that
                                   requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the
                                   driver specified by This.
**/
EFI_STATUS
EFIAPI
DwMmcHcDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  NON_DISCOVERABLE_DEVICE   *Dev;
  PLATFORM_DW_MMC_PROTOCOL  *PlatformDwMmc;

  ParentDevicePath = NULL;

  Status = gBS->LocateProtocol (
                  &gPlatformDwMmcProtocolGuid,
                  NULL,
                  (VOID **)&PlatformDwMmc
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID *)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    //
    // EFI_ALREADY_STARTED is also an error.
    //
    return Status;
  }

  //
  // Close the protocol because we don't use it here.
  //
  gBS->CloseProtocol (
         Controller,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  //
  // Now test the EdkiiNonDiscoverableDeviceProtocol.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Dev,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareGuid (Dev->Type, &gDwMmcHcNonDiscoverableDeviceGuid)) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
         Controller,
         &gEdkiiNonDiscoverableDeviceProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );
  return Status;
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service
  ConnectController().
  As a result, much of the error checking on the parameters to Start() has
  been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system
  behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a
     naturally aligned EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver
     specified by This must have been called with the same calling parameters,
     and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL
                                   instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This
                                   handle must support a protocol interface
                                   that supplies an I/O abstraction to the
                                   driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a
                                   device path.  This parameter is ignored by
                                   device drivers, and is optional for bus
                                   drivers.
                                   For a bus driver, if this parameter is NULL,
                                   then handles for all the children of
                                   Controller are created by this driver.
                                   If this parameter is not NULL and the first
                                   Device Path Node is not the End of Device
                                   Path Node, then only the handle for the
                                   child device specified by the first Device
                                   Path Node of RemainingDevicePath is created
                                   by this driver.
                                   If the first Device Path Node of
                                   RemainingDevicePath is the End of Device Path
                                   Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a
                                   device error. Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
DwMmcHcDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS              Status;
  DW_MMC_HC_PRIVATE_DATA  *Private;

  NON_DISCOVERABLE_DEVICE  *Dev;

  DWMMC_CARD_TYPE_DETECT_ROUTINE  *Routine;
  UINT8                           Index;
  UINT32                          RoutineNum;
  PLATFORM_DW_MMC_PROTOCOL        *PlatformDwMmc;

  Status = gBS->LocateProtocol (
                  &gPlatformDwMmcProtocolGuid,
                  NULL,
                  (VOID **)&PlatformDwMmc
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "err %d", __LINE__));
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Dev,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "err %d", __LINE__));
    return Status;
  }

  Private = AllocateCopyPool (sizeof (DW_MMC_HC_PRIVATE_DATA), &gDwMmcHcTemplate);
  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "err %d", __LINE__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Private->ControllerHandle = Controller;
  Private->DevBase          = Dev->Resources[0].AddrRangeMin;
  Private->PlatformDwMmc    = PlatformDwMmc;
  InitializeListHead (&Private->Queue);

  Status = Private->PlatformDwMmc->GetCapability (Controller, 0, &Private->Capability[0]);

  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (Private->Capability[0].BaseClkFreq == 0) {
    goto Done;
  }

  DumpCapabilityReg (0, &Private->Capability[0]);

  Status = DwMmcHcCardDetect (Private->DevBase, Controller, 0, &Private->Slot[0].MediaPresent);

  //
  // Initialize slot and start identification process for the new attached device
  //
  Status = DwMmcHcInitHost (Private->DevBase, Private->Capability[0]);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Reset HC
  //
  Status = DwMmcHcReset (Private->DevBase, Private->Capability[0]);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Private->Slot[0].SlotType = Private->Capability[0].SlotType;
  Private->Slot[0].CardType = Private->Capability[0].CardType;
  Private->Slot[0].Enable   = TRUE;

  RoutineNum = sizeof (mCardTypeDetectRoutineTable) / sizeof (DWMMC_CARD_TYPE_DETECT_ROUTINE);
  for (Index = 0; Index < RoutineNum; Index++) {
    Routine = &mCardTypeDetectRoutineTable[Index];
    if (*Routine != NULL) {
      Status = (*Routine)(Private);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
  }

  //
  // Start the asynchronous I/O monitor
  //
  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  ProcessAsyncTaskList,
                  Private,
                  &Private->TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = gBS->SetTimer (Private->TimerEvent, TimerPeriodic, DW_MMC_HC_ASYNC_TIMER);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Start the Sd removable device connection enumeration
  //
  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  DwMmcHcEnumerateDevice,
                  Private,
                  &Private->ConnectEvent
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = gBS->SetTimer (Private->ConnectEvent, TimerPeriodic, DW_MMC_HC_ENUM_TIMER);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiSdMmcPassThruProtocolGuid,
                  &(Private->PassThru),
                  NULL
                  );

  DEBUG ((DEBUG_INFO, "DwMmcHcDriverBindingStart: %r End on %x\n", Status, Controller));

Done:
  if (EFI_ERROR (Status)) {
    if ((Private != NULL) && (Private->TimerEvent != NULL)) {
      gBS->CloseEvent (Private->TimerEvent);
    }

    if ((Private != NULL) && (Private->ConnectEvent != NULL)) {
      gBS->CloseEvent (Private->ConnectEvent);
    }

    if (Private != NULL) {
      FreePool (Private);
    }
  }

  return Status;
}

/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service
  DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been
  moved into this common boot service. It is legal to call Stop() from other
  locations, but the following calling restrictions must be followed or the
  system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous
     call to this same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in
     this driver's Start() function, and the Start() function must have called
     OpenProtocol() on ControllerHandle with an Attribute of
     EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL
                                instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle
                                must support a bus specific I/O protocol for the
                                driver to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in
                                ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be
                                NULL if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device
                                error.

**/
EFI_STATUS
EFIAPI
DwMmcHcDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                     Status;
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru;
  DW_MMC_HC_PRIVATE_DATA         *Private;
  LIST_ENTRY                     *Link;
  LIST_ENTRY                     *NextLink;
  DW_MMC_HC_TRB                  *Trb;

  DEBUG ((DEBUG_INFO, "DwMmcHcDriverBindingStop: Start\n"));

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSdMmcPassThruProtocolGuid,
                  (VOID **)&PassThru,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (PassThru);
  //
  // Close Non-Blocking timer and free Task list.
  //
  if (Private->TimerEvent != NULL) {
    gBS->CloseEvent (Private->TimerEvent);
    Private->TimerEvent = NULL;
  }

  if (Private->ConnectEvent != NULL) {
    gBS->CloseEvent (Private->ConnectEvent);
    Private->ConnectEvent = NULL;
  }

  //
  // As the timer is closed, there is no needs to use TPL lock to
  // protect the critical region "queue".
  //
  for (Link = GetFirstNode (&Private->Queue);
       !IsNull (&Private->Queue, Link);
       Link = NextLink)
  {
    NextLink = GetNextNode (&Private->Queue, Link);
    RemoveEntryList (Link);
    Trb                            = DW_MMC_HC_TRB_FROM_THIS (Link);
    Trb->Packet->TransactionStatus = EFI_ABORTED;
    gBS->SignalEvent (Trb->Event);
    DwMmcFreeTrb (Trb);
  }

  //
  // Uninstall Block I/O protocol from the device handle
  //
  Status = gBS->UninstallProtocolInterface (
                  Controller,
                  &gEfiSdMmcPassThruProtocolGuid,
                  &(Private->PassThru)
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  FreePool (Private);

  DEBUG ((DEBUG_INFO, "DwMmcHcDriverBindingStop: End with %r\n", Status));

  return Status;
}

/**
  Sends SD command to an SD card that is attached to the SD controller.

  The PassThru() function sends the SD command specified by Packet to the SD
  card specified by Slot.

  If Packet is successfully sent to the SD card, then EFI_SUCCESS is returned.

  If a device error occurs while sending the Packet, then EFI_DEVICE_ERROR is
  returned.

  If Slot is not in a valid range for the SD controller, then
  EFI_INVALID_PARAMETER is returned.

  If Packet defines a data command but both InDataBuffer and OutDataBuffer are
  NULL, EFI_INVALID_PARAMETER is returned.

  @param[in]     This           A pointer to the EFI_SD_MMC_PASS_THRU_PROTOCOL
                                instance.
  @param[in]     Slot           The slot number of the SD card to send the
                                command to.
  @param[in,out] Packet         A pointer to the SD command data structure.
  @param[in]     Event          If Event is NULL, blocking I/O is performed. If
                                Event is not NULL, then nonblocking I/O is
                                performed, and Event will be signaled when the
                                Packet completes.

  @retval EFI_SUCCESS           The SD Command Packet was sent by the host.
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to send
                                the SD command Packet.
  @retval EFI_INVALID_PARAMETER Packet, Slot, or the contents of the Packet is
                                invalid.
  @retval EFI_INVALID_PARAMETER Packet defines a data command but both
                                InDataBuffer and OutDataBuffer are NULL.
  @retval EFI_NO_MEDIA          SD Device not present in the Slot.
  @retval EFI_UNSUPPORTED       The command described by the SD Command Packet
                                is not supported by the host controller.
  @retval EFI_BAD_BUFFER_SIZE   The InTransferLength or OutTransferLength
                                exceeds the limit supported by SD card
                                ( i.e. if the number of bytes exceed the Last
                                LBA).

**/
EFI_STATUS
EFIAPI
DwMmcPassThruPassThru (
  IN     EFI_SD_MMC_PASS_THRU_PROTOCOL        *This,
  IN     UINT8                                Slot,
  IN OUT EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  *Packet,
  IN     EFI_EVENT                            Event    OPTIONAL
  )
{
  EFI_STATUS              Status;
  DW_MMC_HC_PRIVATE_DATA  *Private;
  DW_MMC_HC_TRB           *Trb;
  EFI_TPL                 OldTpl;

  if ((This == NULL) || (Packet == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->SdMmcCmdBlk == NULL) || (Packet->SdMmcStatusBlk == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->OutDataBuffer == NULL) && (Packet->OutTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->InDataBuffer == NULL) && (Packet->InTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (This);

  if (!Private->Slot[Slot].Enable) {
    return EFI_INVALID_PARAMETER;
  }

  if (!Private->Slot[Slot].MediaPresent) {
    return EFI_NO_MEDIA;
  }

  Trb = DwMmcCreateTrb (Private, Slot, Packet, Event);
  if (Trb == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Immediately return for async I/O.
  //
  if (Event != NULL) {
    return EFI_SUCCESS;
  }

  //
  // Wait async I/O list is empty before execute sync I/O operation.
  //
  while (TRUE) {
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    if (IsListEmpty (&Private->Queue)) {
      gBS->RestoreTPL (OldTpl);
      break;
    }

    gBS->RestoreTPL (OldTpl);
  }

  Status = DwMmcWaitTrbEnv (Private, Trb);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = DwMmcExecTrb (Private, Trb);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = DwMmcWaitTrbResult (Private, Trb);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

Done:
  if (Trb != NULL) {
    DwMmcFreeTrb (Trb);
  }

  return Status;
}

/**
  Used to retrieve next slot numbers supported by the SD controller. The
  function returns information about all available slots (populated or
  not-populated).

  The GetNextSlot() function retrieves the next slot number on an SD controller.
  If on input Slot is 0xFF, then the slot number of the first slot on the SD
  controller is returned.

  If Slot is a slot number that was returned on a previous call to
  GetNextSlot(), then the slot number of the next slot on the SD controller is
  returned.

  If Slot is not 0xFF and Slot was not returned on a previous call to
  GetNextSlot(), EFI_INVALID_PARAMETER is returned.

  If Slot is the slot number of the last slot on the SD controller, then
  EFI_NOT_FOUND is returned.

  @param[in]     This           A pointer to the EFI_SD_MMMC_PASS_THRU_PROTOCOL
                                instance.
  @param[in,out] Slot           On input, a pointer to a slot number on the SD
                                controller.
                                On output, a pointer to the next slot number on
                                the SD controller.
                                An input value of 0xFF retrieves the first slot
                                number on the SD controller.

  @retval EFI_SUCCESS           The next slot number on the SD controller was
                                returned in Slot.
  @retval EFI_NOT_FOUND         There are no more slots on this SD controller.
  @retval EFI_INVALID_PARAMETER Slot is not 0xFF and Slot was not returned on a
                                previous call to GetNextSlot().

**/
EFI_STATUS
EFIAPI
DwMmcPassThruGetNextSlot (
  IN     EFI_SD_MMC_PASS_THRU_PROTOCOL  *This,
  IN OUT UINT8                          *Slot
  )
{
  DW_MMC_HC_PRIVATE_DATA  *Private;

  if ((This == NULL) || (Slot == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (This);

  if (*Slot == 0xFF) {
    if (Private->Slot[0].Enable) {
      *Slot                 = 0;
      Private->PreviousSlot = 0;
      return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
  } else if (*Slot == Private->PreviousSlot) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_INVALID_PARAMETER;
  }
}

/**
  Used to allocate and build a device path node for an SD card on the SD
  controller.

  The BuildDevicePath() function allocates and builds a single device node
  for the SD card specified by Slot.

  If the SD card specified by Slot is not present on the SD controller, then
  EFI_NOT_FOUND is returned.

  If DevicePath is NULL, then EFI_INVALID_PARAMETER is returned.

  If there are not enough resources to allocate the device path node, then
  EFI_OUT_OF_RESOURCES is returned.

  Otherwise, DevicePath is allocated with the boot service AllocatePool(),
  the contents of DevicePath are initialized to describe the SD card specified
  by Slot, and EFI_SUCCESS is returned.

  @param[in]     This           A pointer to the EFI_SD_MMMC_PASS_THRU_PROTOCOL
                                instance.
  @param[in]     Slot           Specifies the slot number of the SD card for
                                which a device path node is to be allocated and
                                built.
  @param[in,out] DevicePath     A pointer to a single device path node that
                                describes the SD card specified by Slot. This
                                function is responsible for allocating the
                                buffer DevicePath with the boot service
                                AllocatePool(). It is the caller's responsi-
                                bility to free DevicePath when the caller is
                                finished with DevicePath.

  @retval EFI_SUCCESS           The device path node that describes the SD card
                                specified by Slot was allocated and returned in
                                DevicePath.
  @retval EFI_NOT_FOUND         The SD card specified by Slot does not exist on
                                the SD controller.
  @retval EFI_INVALID_PARAMETER DevicePath is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources to allocate
                                DevicePath.

**/
EFI_STATUS
EFIAPI
DwMmcPassThruBuildDevicePath (
  IN     EFI_SD_MMC_PASS_THRU_PROTOCOL  *This,
  IN     UINT8                          Slot,
  IN OUT EFI_DEVICE_PATH_PROTOCOL       **DevicePath
  )
{
  DW_MMC_HC_PRIVATE_DATA  *Private;
  SD_DEVICE_PATH          *SdNode;
  EMMC_DEVICE_PATH        *EmmcNode;

  if ((This == NULL) || (DevicePath == NULL) || (Slot >= DW_MMC_HC_MAX_SLOT)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (This);

  if ((!Private->Slot[Slot].Enable) || (!Private->Slot[Slot].MediaPresent)) {
    return EFI_NOT_FOUND;
  }

  if (Private->Slot[Slot].CardType == SdCardType) {
    SdNode = AllocateCopyPool (sizeof (SD_DEVICE_PATH), &mSdDpTemplate);
    if (SdNode == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    SdNode->SlotNumber = Slot;

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)SdNode;
  } else if (Private->Slot[Slot].CardType == EmmcCardType) {
    EmmcNode = AllocateCopyPool (sizeof (EMMC_DEVICE_PATH), &mEmmcDpTemplate);
    if (EmmcNode == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    EmmcNode->SlotNumber = Slot;

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)EmmcNode;
  } else {
    //
    // Currently we only support SD and EMMC two device nodes.
    //
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  This function retrieves an SD card slot number based on the input device path.

  The GetSlotNumber() function retrieves slot number for the SD card specified
  by the DevicePath node. If DevicePath is NULL, EFI_INVALID_PARAMETER is
  returned.

  If DevicePath is not a device path node type that the SD Pass Thru driver
  supports, EFI_UNSUPPORTED is returned.

  @param[in]  This              A pointer to the EFI_SD_MMC_PASS_THRU_PROTOCOL
                                instance.
  @param[in]  DevicePath        A pointer to the device path node that describes
                                a SD card on the SD controller.
  @param[out] Slot              On return, points to the slot number of an SD
                                card on the SD controller.

  @retval EFI_SUCCESS           SD card slot number is returned in Slot.
  @retval EFI_INVALID_PARAMETER Slot or DevicePath is NULL.
  @retval EFI_UNSUPPORTED       DevicePath is not a device path node type that
                                the SD Pass Thru driver supports.

**/
EFI_STATUS
EFIAPI
DwMmcPassThruGetSlotNumber (
  IN  EFI_SD_MMC_PASS_THRU_PROTOCOL  *This,
  IN  EFI_DEVICE_PATH_PROTOCOL       *DevicePath,
  OUT UINT8                          *Slot
  )
{
  DW_MMC_HC_PRIVATE_DATA  *Private;
  SD_DEVICE_PATH          *SdNode;
  EMMC_DEVICE_PATH        *EmmcNode;
  UINT8                   SlotNumber;

  if ((This == NULL) || (DevicePath == NULL) || (Slot == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (This);

  //
  // Check whether the DevicePath belongs to SD_DEVICE_PATH or EMMC_DEVICE_PATH
  //
  if ((DevicePath->Type != MESSAGING_DEVICE_PATH) ||
      ((DevicePath->SubType != MSG_SD_DP) &&
       (DevicePath->SubType != MSG_EMMC_DP)) ||
      (DevicePathNodeLength (DevicePath) != sizeof (SD_DEVICE_PATH)) ||
      (DevicePathNodeLength (DevicePath) != sizeof (EMMC_DEVICE_PATH)))
  {
    return EFI_UNSUPPORTED;
  }

  if (DevicePath->SubType == MSG_SD_DP) {
    SdNode     = (SD_DEVICE_PATH *)DevicePath;
    SlotNumber = SdNode->SlotNumber;
  } else {
    EmmcNode   = (EMMC_DEVICE_PATH *)DevicePath;
    SlotNumber = EmmcNode->SlotNumber;
  }

  if (SlotNumber >= DW_MMC_HC_MAX_SLOT) {
    return EFI_NOT_FOUND;
  }

  if (Private->Slot[SlotNumber].Enable) {
    *Slot = SlotNumber;
    return EFI_SUCCESS;
  } else {
    return EFI_NOT_FOUND;
  }
}

/**
  Resets an SD card that is connected to the SD controller.

  The ResetDevice() function resets the SD card specified by Slot.

  If this SD controller does not support a device reset operation,
  EFI_UNSUPPORTED is returned.

  If Slot is not in a valid slot number for this SD controller,
  EFI_INVALID_PARAMETER is returned.

  If the device reset operation is completed, EFI_SUCCESS is returned.

  @param[in]  This              A pointer to the EFI_SD_MMC_PASS_THRU_PROTOCOL
                                instance.
  @param[in]  Slot              Specifies the slot number of the SD card to be
                                reset.

  @retval EFI_SUCCESS           The SD card specified by Slot was reset.
  @retval EFI_UNSUPPORTED       The SD controller does not support a device
                                reset operation.
  @retval EFI_INVALID_PARAMETER Slot number is invalid.
  @retval EFI_NO_MEDIA          SD Device not present in the Slot.
  @retval EFI_DEVICE_ERROR      The reset command failed due to a device error

**/
EFI_STATUS
EFIAPI
DwMmcPassThruResetDevice (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL  *This,
  IN UINT8                          Slot
  )
{
  DW_MMC_HC_PRIVATE_DATA  *Private;
  LIST_ENTRY              *Link;
  LIST_ENTRY              *NextLink;
  DW_MMC_HC_TRB           *Trb;
  EFI_TPL                 OldTpl;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = DW_MMC_HC_PRIVATE_FROM_THIS (This);

  if (!Private->Slot[Slot].Enable) {
    return EFI_INVALID_PARAMETER;
  }

  if (!Private->Slot[Slot].MediaPresent) {
    return EFI_NO_MEDIA;
  }

  //
  // Free all async I/O requests in the queue
  //
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  for (Link = GetFirstNode (&Private->Queue);
       !IsNull (&Private->Queue, Link);
       Link = NextLink)
  {
    NextLink = GetNextNode (&Private->Queue, Link);
    RemoveEntryList (Link);
    Trb                            = DW_MMC_HC_TRB_FROM_THIS (Link);
    Trb->Packet->TransactionStatus = EFI_ABORTED;
    gBS->SignalEvent (Trb->Event);
    DwMmcFreeTrb (Trb);
  }

  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;
}
