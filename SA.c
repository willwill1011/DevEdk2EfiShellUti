/** @file
  This is a simple shell application
  Copyright (c) 2008 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Base.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>    // IoRead
#include <Library/ShellLib.h> // ShellPromptForResponse
#include <Library/DebugLib.h>

#include <Protocol/PciRootBridgeIo.h> // EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.Io.Read/Write
#include <Protocol/CpuIo2.h>          // EFI_CPU_IO2_PROTOCOL_GUID.Io.Read/Write

#if 1 // define constant value
#define CMOS_INDEX_IO_PORT 0x70
#define CMOS_DATA_IO_PORT 0x71

#define PCI_INDEX_IO_PORT 0xCF8
#define PCI_DATA_IO_PORT 0xCFC

#define SCAN_NULL 0x0000
#define SCAN_UP 0x0001
#define SCAN_DOWN 0x0002
#define SCAN_PAGEUP 0x0009
#define SCAN_PAGEDOWN 0x000A
#define SCAN_F2 0x000C
#define SCAN_F3 0x000D
#define SCAN_ESC 0x0017

#define CHAR_BACKSPACE 0x0008
#define CHAR_CARRIAGE_RETURN 0x000D
#define CHAR_UNICODE_1 0X0031
#define CHAR_UNICODE_2 0X0032
#define CHAR_UNICODE_3 0X0033
#define CHAR_UNICODE_4 0X0034
#define CHAR_UNICODE_5 0X0035
#define CHAR_UNICODE_6 0X0036
#define CHAR_UNICODE_7 0X0037
#define CHAR_UNICODE_8 0X0038
#define CHAR_UNICODE_9 0X0039

#define CHAR_UNICODE_n 0X006E
#define CHAR_UNICODE_y 0X0079

#define PciUtility 1
#define BDSUtility 2
#define CMOSUtility 3
#define MemUtility 4
#endif

#if 1 // const char string
CONST CHAR8 MemUtiMsgTop[] = "Max T. Memory Utility\r\nWhat you want ?\r\n  \
  1. Get Current Memory Allocation Map\r\n  \
  2. AllocatePages ().\r\n  \
  3. AllocatePool (). ---Not Ready Yet---\r\n  \
  4. Show Allocated Memory Information ---Not Ready Yet---\r\n  \
  5. Free All/Specific Allocated Memory ---Not Ready Yet---\r\n  \
  Press [ESC] to Exit.\r\n";

CONST CHAR8 AllocatePageMemTop[] = "Now Process Allocatepages\r\nSupported Allocate Type :\r\n--------------------------------------\r\n  \
  1. AllocateAnyPages\r\n  \
  2. AllocateMaxAddress ---Not Ready Yet---\r\n  \
  3. AllocateAddress ---Not Ready Yet---\r\n  \
  Press choose one, [ESC] back to Upper Level.\r\n";

CONST CHAR8 MainMenuMsgTop[] = "Utility List:\r\n1. PCI Utility\r\n2. BIOS Data Area Utility\r\n3. CMOS Utility\r\n4. Mem Utility\r\n[Esc] to Exit\r\n";

CONST CHAR8 SelMemTypeNum[] = "1.EfiReservedMemoryType\r\n2.EfiLoaderCode\r\n3.EfiLoaderData\r\n4.EfiBootServicesCode\r\n5.EfiBootServicesData\r\n\
6.EfiRuntimeServicesCode\r\n7.EfiRuntimeServicesData\r\n8.EfiConventionalMemory\r\n9.EfiUnusableMemory\r\n10.EfiACPIReclaimMemory\r\n\
11.EfiACPIMemoryNVS\r\n12.EfiMemoryMappedIO\r\n"; //13.EfiMemoryMappedIOPortSpace\r\n14.EfiPalCode\r\n15.EfiPersistentMemory";
#endif

extern EFI_SYSTEM_TABLE *gST;
extern EFI_BOOT_SERVICES *gBS;

//-----------------------------------structure--------------------------------------------------
// linked list for get memory map: add/remove features for allocatepool/page
struct node
{
  EFI_MEMORY_DESCRIPTOR *MemMap;
  struct node *next;
};
typedef struct node MemMapNode;

struct pageNode
{
  EFI_ALLOCATE_TYPE PAType;
  EFI_MEMORY_TYPE PAMemType;
  UINTN PAPage;
  EFI_PHYSICAL_ADDRESS PAPhyAddr;
  struct pageNode *next;
};
typedef struct pageNode PApageNode;

//--------------------------------Function Protype-----------------------------------------------
// 0. print a prompt menu and select 1 to 13 to continue
EFI_STATUS SelNum1to12Menu(IN CONST CHAR8 *, OUT UINT8 *);
EFI_STATUS SelNum1to5Menu(IN CONST CHAR8 *, OUT UINT8 *);
EFI_STATUS SelYorNMenu(IN CONST CHAR8 *, OUT UINT8 *);
EFI_STATUS PressAnyKeyToContinue();

EFI_STATUS MemMainProgram();
EFI_STATUS AllocatePageMemory();
EFI_STATUS AllocatePageMemoryAnyPages(IN EFI_ALLOCATE_TYPE allocateTypeSel);

EFI_STATUS Input2HexToUInt8(OUT UINT8 *OutputValue);
EFI_STATUS Input2HexToUInt64(OUT UINT64 *OutputValue);
EFI_STATUS Input8HexToUInt64(OUT UINT64 *OutputValue);

//--------------------------------Functions Below Required Processing-----------------------------------------------

//--------------------------------------To be deleted-----------------------------------------------------------------
// DWord 4 bytes x64 to Word 2 bytes x128
EFI_STATUS TransferDWordToWordTable(
    IN UINT32 *PciTable,
    OUT UINT16 *WordTable)
{
  for (UINT16 j = 0; j < 128; j++)
  {
    WordTable[2 * j] = (PciTable[j] & 0xffff0000);
    WordTable[2 * j + 1] = (PciTable[j] & 0x0000ffff) >> 16;
  }
  return EFI_SUCCESS;
}
EFI_STATUS PrintSupportedMemType()
{
  Print(L"Now Process Allocatepages. (AllocateAnyPages)\r\n \
  Supported Memory Type :\r\n  \
  --------------------------------------\r\n");
  for (UINT8 i = 1; i < 13; i++)
  {
    switch (i)
    {
    case EfiReservedMemoryType:
      Print(L"EfiReservedMemoryType\n");
      break;
    case EfiLoaderCode:
      Print(L"EfiLoaderCode\n");
      break;
    case EfiLoaderData:
      Print(L"EfiLoaderData\n");
      break;
    case EfiBootServicesCode:
      Print(L"EfiBootServicesCode\n");
      break;
    case EfiBootServicesData:
      Print(L"EfiBootServicesData\n");
      break;
    case EfiRuntimeServicesCode:
      Print(L"EfiRuntimeServicesCode\n");
      break;
    case EfiRuntimeServicesData:
      Print(L"EfiRuntimeServicesData\n");
      break;
    case EfiConventionalMemory:
      Print(L"EfiConventionalMemory\n");
      break;
    case EfiUnusableMemory:
      Print(L"EfiUnusableMemory\n");
      break;
    case EfiACPIReclaimMemory:
      Print(L"EfiACPIReclaimMemory\n");
      break;
    case EfiACPIMemoryNVS:
      Print(L"EfiACPIMemoryNVS\n");
      break;
    case EfiMemoryMappedIO:
      Print(L"EfiMemoryMappedIO\n");
      break;
    case EfiMemoryMappedIOPortSpace:
      Print(L"EfiMemoryMappedIOPortSpace\n");
      break;
    case EfiPalCode:
      Print(L"EfiPalCode\n");
      break;
    case EfiPersistentMemory:
      Print(L"EfiPersistentMemory\n");
      break;
    default:
      Print(L"i=%d\n", i);
      break;
    }
  }
  Print(L"Press choose one, [ESC] back to Upper Level.\r\n ");
  return EFI_SUCCESS;
}

//--------------------------------------PCI Utility-----------------------------------------------------------------

// memory type enum to string: return char*16
CHAR16 *
ConvertMemTypeToString(
    IN CONST EFI_MEMORY_TYPE MemTypeEnum)
{
  CHAR16 *RetVal;
  RetVal = NULL;
  switch (MemTypeEnum)
  {
  case EfiReservedMemoryType:
    StrnCatGrow(&RetVal, NULL, L"ReservedMem ", 12);
    break;
  case EfiLoaderCode:
    StrnCatGrow(&RetVal, NULL, L"LoaderCode  ", 12);
    break;
  case EfiLoaderData:
    StrnCatGrow(&RetVal, NULL, L"LoaderData  ", 12);
    break;
  case EfiBootServicesCode:
    StrnCatGrow(&RetVal, NULL, L"BootSvcCode ", 12);
    break;
  case EfiBootServicesData:
    StrnCatGrow(&RetVal, NULL, L"BootSvcData ", 12);
    break;
  case EfiRuntimeServicesCode:
    StrnCatGrow(&RetVal, NULL, L"RtimeSvcCode", 12);
    break;
  case EfiRuntimeServicesData:
    StrnCatGrow(&RetVal, NULL, L"RtimeSvcData", 12);
    break;
  case EfiConventionalMemory:
    StrnCatGrow(&RetVal, NULL, L"Conventional", 12);
    break;
  case EfiUnusableMemory:
    StrnCatGrow(&RetVal, NULL, L"Unusable    ", 12);
    break;
  case EfiACPIReclaimMemory:
    StrnCatGrow(&RetVal, NULL, L"ACPIReclaim ", 12);
    break;
  case EfiACPIMemoryNVS:
    StrnCatGrow(&RetVal, NULL, L"ACPIMemNVS  ", 12);
    break;
  case EfiMemoryMappedIO:
    StrnCatGrow(&RetVal, NULL, L"MemMappedIO ", 12);
    break;
  case EfiMemoryMappedIOPortSpace:
    StrnCatGrow(&RetVal, NULL, L"MemMapIOPS  ", 12);
    break;
  case EfiPalCode:
    StrnCatGrow(&RetVal, NULL, L"PalCode     ", 12);
    break;
  case EfiPersistentMemory:
    StrnCatGrow(&RetVal, NULL, L"Persistent  ", 12);
    break;
  default:
    StrnCatGrow(&RetVal, NULL, L"%x\n", MemTypeEnum);
  }
  return (RetVal);
}
// ami get system memory map works but mine does not -> need to find out ???
EFI_STATUS GetSystemMemoryMap(
    OUT EFI_MEMORY_DESCRIPTOR **MemMap,
    OUT UINTN *MemDescSize,
    OUT UINTN *MemEntriesCount)
{
  EFI_STATUS Status;
  UINTN MemMapSize, MemMapKey;
  UINT32 MemDescVer;
  *MemMap = NULL;

  MemMapSize = 0; // GetMemoryMap will return the size needed for the map
  Status = gBS->GetMemoryMap(&MemMapSize, *MemMap, &MemMapKey, MemDescSize, &MemDescVer);

  if (Status != EFI_BUFFER_TOO_SMALL)
  {
    return Status;
  }
  MemMapSize += EFI_PAGE_SIZE; // PAGE SIZE = 4KB
  Status = gBS->AllocatePool(EfiBootServicesData, MemMapSize, (VOID **)MemMap);
  ASSERT_EFI_ERROR(Status);
  Status = gBS->GetMemoryMap(&MemMapSize, *MemMap, &MemMapKey, MemDescSize, &MemDescVer);
  ASSERT_EFI_ERROR(Status);
  *MemEntriesCount = (UINT16)(MemMapSize / *MemDescSize);

  return EFI_SUCCESS;
}
// convert memory type from enum to string; error will return NULL
CHAR16 *
ConvertMemoryType(
    IN CONST EFI_MEMORY_TYPE Memory)
{
  CHAR16 *RetVal;
  RetVal = NULL;

  switch (Memory)
  {
  case EfiReservedMemoryType:
    StrnCatGrow(&RetVal, NULL, L"EfiReservedMemoryType", 0);
    break;
  case EfiLoaderCode:
    StrnCatGrow(&RetVal, NULL, L"EfiLoaderCode", 0);
    break;
  case EfiLoaderData:
    StrnCatGrow(&RetVal, NULL, L"EfiLoaderData", 0);
    break;
  case EfiBootServicesCode:
    StrnCatGrow(&RetVal, NULL, L"EfiBootServicesCode", 0);
    break;
  case EfiBootServicesData:
    StrnCatGrow(&RetVal, NULL, L"EfiBootServicesData", 0);
    break;
  case EfiRuntimeServicesCode:
    StrnCatGrow(&RetVal, NULL, L"EfiRuntimeServicesCode", 0);
    break;
  case EfiRuntimeServicesData:
    StrnCatGrow(&RetVal, NULL, L"EfiRuntimeServicesData", 0);
    break;
  case EfiConventionalMemory:
    StrnCatGrow(&RetVal, NULL, L"EfiConventionalMemory", 0);
    break;
  case EfiUnusableMemory:
    StrnCatGrow(&RetVal, NULL, L"EfiUnusableMemory", 0);
    break;
  case EfiACPIReclaimMemory:
    StrnCatGrow(&RetVal, NULL, L"EfiACPIReclaimMemory", 0);
    break;
  case EfiACPIMemoryNVS:
    StrnCatGrow(&RetVal, NULL, L"EfiACPIMemoryNVS", 0);
    break;
  case EfiMemoryMappedIO:
    StrnCatGrow(&RetVal, NULL, L"EfiMemoryMappedIO", 0);
    break;
  case EfiMemoryMappedIOPortSpace:
    StrnCatGrow(&RetVal, NULL, L"EfiMemoryMappedIOPortSpace", 0);
    break;
  case EfiPalCode:
    StrnCatGrow(&RetVal, NULL, L"EfiPalCode", 0);
    break;
  case EfiMaxMemoryType:
    StrnCatGrow(&RetVal, NULL, L"EfiMaxMemoryType", 0);
    break;
  default:
    //ASSERT(FALSE);
    StrnCatGrow(&RetVal, NULL, L"NotEfiMemType", 0);
    break;
  }
  return (RetVal);
}
//top bar display Bus Decvice Function Number
EFI_STATUS
PciAddrToBDF(
    IN UINT32 PciAddr,
    OUT UINT16 *B,
    OUT UINT8 *D,
    OUT UINT8 *F)
{
  *B = (0x00FF0000 & PciAddr) >> 16;
  *D = (0x0000F800 & PciAddr) >> 11;
  *F = (0x00000700 & PciAddr) >> 8;
  //Print(L"  ---PciAddr: %8x   Bus=%2x   Dev=%2x   Fun=%2x ---\n\n", PciAddr, B, D, F);
  return EFI_SUCCESS;
}
//top menu display
EFI_STATUS
PrintTopMenu(
    IN UINT32 *PciAddrArray,
    IN UINT8 ItemIndex,
    IN UINT32 NumberOfPci)
{
  UINT8 Dev, Fun;
  UINT16 Bus, DevID, VenID;
  UINT32 temp;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L" No#     VenID    DevID    Bus    Dev    Fun    Addr \n");
  //ItemIndex 0->20->40->...
  while ((0 <= ItemIndex) && (ItemIndex < NumberOfPci))
  {
    //Print(L"PciAddrArray[%d]=%08x\n", ItemIndex, PciAddrArray[ItemIndex]);
    //Addr |= ((Bus << 16) | (Dev << 11) | (Fun << 8)); //set the pci bus dev and fun
    PciAddrToBDF(PciAddrArray[ItemIndex], &Bus, &Dev, &Fun);
    IoWrite32(PCI_INDEX_IO_PORT, PciAddrArray[ItemIndex]);
    temp = IoRead32(PCI_DATA_IO_PORT);
    DevID = (temp & 0xffff0000) >> 16;
    VenID = (temp & 0x0000ffff);
    Print(L" %03d     %04x     %04x     %02x     %02x     %02x     %08x  \n", ItemIndex, VenID, DevID, Bus, Dev, Fun, PciAddrArray[ItemIndex]);
    ItemIndex++;
    if (ItemIndex % 20 == 0)
    {
      break;
    }
  }
  Print(L"Use <UP>/<DOWN> to Select PCI Device");
  Print(L"\nUse <Enter> to See Device Table");
  Print(L"\nUse <Esc> to Quit");
  return EFI_SUCCESS;
}
//scan pci bus
EFI_STATUS
PciBusScan(
    OUT UINT32 *PciAddrArray,
    OUT UINT32 *ptrNumberOfPci)
{
  UINT8 Dev, Fun, HeaderType;
  UINT16 Bus, VenID, DevID;
  UINT32 Addr, NumberOfPci = 0;
  //gST->ConOut->ClearScreen(gST->ConOut);
  //Print(L" No#    Bus    Dev    Fun    VenID    DevID    Addr \n");
  for (Bus = 0; Bus < 256; Bus++)
  {
    for (Dev = 0; Dev < 32; Dev++)
    {
      for (Fun = 0; Fun < 8; Fun++)
      {
        Addr = 0x80000000;                                //clean the address
        Addr |= ((Bus << 16) | (Dev << 11) | (Fun << 8)); //set the pci bus dev and fun
        IoWrite32(PCI_INDEX_IO_PORT, Addr);
        VenID = IoRead16(PCI_DATA_IO_PORT);
        DevID = IoRead16(PCI_DATA_IO_PORT + 2);
        IoWrite32(PCI_INDEX_IO_PORT, Addr + 0x0E);  // shift to 0x0C
        HeaderType = IoRead8(PCI_DATA_IO_PORT + 2); // read offset is 0E-0C=2
        // Header Type: 00-> single  80-> multi  FF-> device not exist
        //Print(L"BDF=%02x %02x %02x HT=%02x,DID=%04x,VID=%04x\n", Bus, Dev, Fun, HeaderType, DevID, VenID);
        if ((HeaderType == 0x00) && (VenID != 0xffff) && (DevID != 0xffff))
        {
          PciAddrArray[NumberOfPci] = Addr;
          Print(L" %03d     %02x     %02x     %02x     %04x     %04x     %08x  \n", NumberOfPci + 1, Bus, Dev, Fun, VenID, DevID, PciAddrArray[NumberOfPci]);
          NumberOfPci++;
        }
        else if ((HeaderType == 0x80) && (VenID != 0xffff) && (DevID != 0xffff))
        {
          //PciAddrArray[NumberOfPci] = Addr;
          //Print(L" %03d     %02x     %02x     %02x     %04x     %04x     %08x  \n", NumberOfPci + 1, Bus, Dev, Fun, VenID, DevID, PciAddrArray[NumberOfPci]);
          //NumberOfPci++;
          //continue;
        }
      }
    }
  }
  *ptrNumberOfPci = NumberOfPci;
  return EFI_SUCCESS;
}
//IoRead/Write32 to r/w pci device configuration space
EFI_STATUS
PortIoRead32(
    IN UINT32 PciAddr,
    OUT UINT32 *PciTable)
{
  for (INT16 i = 0; i < 64; i++)
  {
    IoWrite32(PCI_INDEX_IO_PORT, PciAddr);
    PciTable[i] = IoRead32(PCI_DATA_IO_PORT);
    PciAddr += 0x04;
  }
  return EFI_SUCCESS;
}
// DWord 1 byte x256 to DWord 4 bytes x64
EFI_STATUS
TransferByteToDWordTable(
    IN UINT8 *ByteTable,
    OUT UINT32 *DWordTable)
{
  for (UINT16 j = 0; j < 256; j += 4)
  {
    DWordTable[j / 4] = (((ByteTable[j + 3] & 0x000000ff) << 24) | ((ByteTable[j + 2] & 0x000000ff) << 16) | ((ByteTable[j + 1] & 0x000000ff) << 8) | (ByteTable[j] & 0x000000ff));
    //Print(L"ByteTable[%d]=%x  ", j, ByteTable[j]);
    //Print(L"DWordTable[j / 4]=%x \n", DWordTable[j / 4]);
    //gBS->Stall(1000000);
  }
  return EFI_SUCCESS;
}
//transfer pci table from DWord to Byte or Word (Default read in data is DWord)
EFI_STATUS
PrintTableByMode(
    IN UINT32 *PciTable,
    IN UINT8 Mode // 0: Byte 1: Word 2: DWord
)
{
  UINT8 i, k;
  UINT16 j;
  UINT8 ByteTable[256];
  UINT16 WordTable[128];
  switch (Mode)
  {
  case 0: // Print Byte Table
    //Transfer DWord To Byte Table
    for (j = 0; j < 64; j++)
    {
      ByteTable[4 * j] = (PciTable[j] & 0x000000ff);
      ByteTable[4 * j + 1] = (PciTable[j] & 0x0000ff00) >> 8;
      ByteTable[4 * j + 2] = (PciTable[j] & 0x00ff0000) >> 16;
      ByteTable[4 * j + 3] = (PciTable[j] & 0xff000000) >> 24;
    }
    for (i = 0; i < 16; i++)
    {
      //print empty row to align row index
      if (i == 0)
      {
        Print(L"   ");
      }
      Print(L"%02x ", i); //print column index: 00 01 .... 0F
    }
    Print(L"\n");
    k = 0;
    for (j = 0; j < 256; j++)
    {
      if (j % 16 == 0)
      {
        Print(L"%02x ", k++); //print row index: 00 01 ... 0F
        Print(L"%02x ", ByteTable[j]);
      }
      else if (j % 16 == 15)
      {
        Print(L"%02x \n", ByteTable[j]);
      }
      else
      {
        Print(L"%02x ", ByteTable[j]);
      }
    }
    break;
  case 1: // print Word Table
    // print row index
    Print(L"   0100 0302 0504 0706 0908 0B0A 0D0C 0F0E\n");
    // word to byte
    for (j = 0; j < 64; j++)
    {
      WordTable[2 * j] = PciTable[j] & 0x0000ffff;             // [0] 12378086 -> 8086
      WordTable[2 * j + 1] = (PciTable[j] & 0xffff0000) >> 16; // [1] 12378086 -> 1237
    }
    k = 0;
    for (j = 0; j < 128; j++)
    {
      if (j % 8 == 0)
      {
        Print(L"%02x ", k++);          //print row index: transpose(00 01 ... 0F)
        Print(L"%04x ", WordTable[j]); // 00 8086 1237 ...
      }
      else if (j % 8 == 7)
      {
        Print(L"%04x \n", WordTable[j]); // new line
      }
      else
      {
        Print(L"%04x ", WordTable[j]);
      }
    }
    break;
  case 2: // print DWord Table
    // print row index
    Print(L"   03020100 07060504 0B0A0908 0F0E0D0C\n");
    k = 0;
    for (j = 0; j < 64; j++)
    {
      if (j % 4 == 0)
      {
        Print(L"%02x ", k++);         //print row index: transpose(00 01 ... 0F)
        Print(L"%08x ", PciTable[j]); // 00 80861237 00000000 ...
      }
      else if (j % 4 == 3)
      {
        Print(L"%08x \n", PciTable[j]); // new line
      }
      else
      {
        Print(L"%08x ", PciTable[j]);
      }
    }
    break;
  }
  return EFI_SUCCESS;
}
//print selected pci table in sub menu and display by Mode
EFI_STATUS
PrintNextMenu(
    IN UINT32 PciAddr, // pci configuration space address
    IN UINT8 Mode      // 0: Byte 1: Word 2: DWord
)
{
  UINT32 PciTable[64];
  UINT16 B = 0;
  UINT8 D = 0, F = 0;
  gST->ConOut->ClearScreen(gST->ConOut);
  PortIoRead32(PciAddr, PciTable);
  PciAddrToBDF(PciAddr, &B, &D, &F);
  Print(L"  ---PciAddr: %8x   Bus=%2x   Dev=%2x   Fun=%2x ---\n", PciAddr, B, D, F);
  PrintTableByMode(PciTable, Mode);
  Print(L"\nUse <UP>/<DOWN> to Switch the Device");
  Print(L"\nUse <F2> to Switch Byte/Word/DWord Mode");
  Print(L"\nUse <F3> to Modify the Register");
  Print(L"\nUse <Esc> to Return Main Menu");
  return EFI_SUCCESS;
}
//replaced by checkHexValue
EFI_STATUS
Transfer2HexTo1Byte(
    IN UINT8 *OffsetF3,
    OUT UINT8 Offset,
    OUT BOOLEAN *OffsetErr)
{
  *OffsetErr = FALSE;
  //process OffsetF3[0] to HSB
  if ((0x30 <= OffsetF3[0]) && (0x39 >= OffsetF3[0])) // 0~9 check
  {
    OffsetF3[0] = (OffsetF3[0] - 0x30) << 4;
  }
  else if ((0x61 <= OffsetF3[0]) && (0x66 >= OffsetF3[0])) // a~f check
  {
    OffsetF3[0] = (OffsetF3[0] - 0x57) << 4;
  }
  else if ((0x41 <= OffsetF3[0]) && (0x46 >= OffsetF3[0])) // A~F check
  {
    OffsetF3[0] = (OffsetF3[0] - 0x37) << 4;
  }
  else
  {
    *OffsetErr = TRUE;
  }
  //process OffsetF3[1] to LSB
  if ((0x30 <= OffsetF3[1]) && (0x39 >= OffsetF3[1])) // 0~9 check
  {
    OffsetF3[1] = (OffsetF3[1] - 0x30);
  }
  else if ((0x61 <= OffsetF3[1]) && (0x66 >= OffsetF3[1])) // a~f check
  {
    OffsetF3[1] = (OffsetF3[1] - 0x57);
  }
  else if ((0x41 <= OffsetF3[1]) && (0x46 >= OffsetF3[1])) // A~F check
  {
    OffsetF3[1] = (OffsetF3[1] - 0x37);
  }
  else
  {
    *OffsetErr = TRUE;
  }
  Offset = OffsetF3[0] + OffsetF3[1];
  return EFI_SUCCESS;
}
//check input offset and value to match hex type and length
EFI_STATUS
CheckHexValue(
    IN CHAR16 *OffsetStr, // Input String
    IN UINT8 Mode,        // 0: Byte 1: Word 2: DWord Mode to check size of input string
    OUT BOOLEAN *Result)  //response check result
{
  UINT8 i;
  *Result = TRUE;
  switch (Mode % 3)
  {
  case 0:
    if (StrLen(OffsetStr) == 2)
    {
      for (i = 0; i < StrLen(OffsetStr); i++)
      {
        if (!ShellIsHexaDecimalDigitCharacter(OffsetStr[i]))
        {
          Print(L"%c is Not Hex Value!\n", OffsetStr[i]);
          *Result = FALSE;
          break;
        }
      }
    }
    else
    {
      Print(L"2 Bytes Required!\n");
      *Result = FALSE;
    }
    break;
  case 1:
    if (StrLen(OffsetStr) == 4)
    {
      for (i = 0; i < StrLen(OffsetStr); i++)
      {
        if (!ShellIsHexaDecimalDigitCharacter(OffsetStr[i]))
        {
          Print(L"%c is Not Hex Value!\n", OffsetStr[i]);
          *Result = FALSE;
          break;
        }
      }
    }
    else
    {
      Print(L"4 Bytes Required!\n");
      *Result = FALSE;
    }
    break;
  case 2:
    if (StrLen(OffsetStr) == 8)
    {
      for (i = 0; i < StrLen(OffsetStr); i++)
      {
        if (!ShellIsHexaDecimalDigitCharacter(OffsetStr[i]))
        {
          Print(L"%c is Not Hex Value!\n", OffsetStr[i]);
          *Result = FALSE;
          break;
        }
      }
    }
    else
    {
      Print(L"8 Bytes Required!\n");
      *Result = FALSE;
    }
    break;
  default:
    Print(L"Unknown error!\n");
    *Result = FALSE;
    break;
  }
  return EFI_SUCCESS;
}
//modify offset and value on pci table
EFI_STATUS
FunctionKey3(
    IN UINT32 PciAddr,
    IN UINT8 ModifyBWD, //Input Value according to FTwo Mode: Byte/Word/DWord)
    OUT UINT64 *ModifyOffset,
    OUT UINT64 *ModifyValue)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINT8 OffsetCount, ValueCount, ShiftOffset, BWDCount;
  CHAR16 OffsetArr[3] = {0}, ValueArr[8] = {0};
  BOOLEAN InputCheck = FALSE; //flag to top or sub menu
  OffsetCount = 0;
  ValueCount = 0;
  Print(L"\n Modify Offset: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (OffsetCount < 2))
      {
        OffsetArr[OffsetCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", OffsetArr[OffsetCount]);
        OffsetCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (OffsetCount == 2))
      {
        //Print(L"\n OffsetArr=%c %c", OffsetArr[0], OffsetArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (OffsetCount > 0) && (OffsetCount <= 2))
      {
        OffsetCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  Status = ShellConvertStringToUint64(OffsetArr, ModifyOffset, TRUE, TRUE);
  //Print(L"\n ModifyOffset=%x", *ModifyOffset);
  ShiftOffset = ((*ModifyOffset >> 2) << 2); //such as E5->E4
  if (*ModifyOffset != ShiftOffset)
  {
    Print(L"    shift offset to =0x%02x", ShiftOffset);
  }
  if (ModifyBWD == 0) //Byte -> ipnut 2 bytes
  {
    BWDCount = 2;
  }
  else if (ModifyBWD == 1) //Byte -> ipnut 4 bytes
  {
    BWDCount = 4;
  }
  else if (ModifyBWD == 2) //Byte -> ipnut 8 bytes
  {
    BWDCount = 8;
  }
  //Print(L"\n ModifyBWD = %x BWDCount=%d ModifyOffset - ShiftOffset=%x \n", ModifyBWD, BWDCount, (*ModifyOffset - ShiftOffset));
  Print(L"\n Modify Value: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < BWDCount))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount == BWDCount))
      {
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount <= BWDCount))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  ShellConvertStringToUint64(ValueArr, ModifyValue, TRUE, TRUE);
  //Print(L"\nModifyValue=%x", *ModifyValue);
  //gBS->Stall(15000000);
  IoWrite32(PCI_INDEX_IO_PORT, PciAddr + ShiftOffset); //write offset addr
  //process offset in io address: PCI_DATA_IO_PORT [cfc cfd cfe cff] = 8 bytes
  if (ModifyBWD == 0)
  {
    // i.e. offset addr E5 will shift to E4 in index io;
    //      value needs to write in data io with shift 1 (E5-E4=1) byte
    //      means write data in 0xcfd ( 0xcfc+1)
    IoWrite8(PCI_DATA_IO_PORT + (*ModifyOffset - ShiftOffset), *ModifyValue); //write value with diff offset
  }
  else if (ModifyBWD == 1)
  {
    // i.e. has 2 cases: E1E0 & E3E2
    //      E1E0: offset to E0 but write value to 0xcfc+0
    //      E2E3: offset to E0 but write value to 0xcfc+2
    if ((*ModifyOffset - ShiftOffset) > 1)
    {
      IoWrite16(PCI_DATA_IO_PORT + 2, *ModifyValue); //write value
    }
    else
    {
      IoWrite16(PCI_DATA_IO_PORT, *ModifyValue); //write value
    }
  }
  else if (ModifyBWD == 2)
  {
    IoWrite32(PCI_DATA_IO_PORT, *ModifyValue); //write value
  }
  //Print(L"\nModifyOffset=%x ShiftOffset=%x", *ModifyOffset, ShiftOffset);
  return EFI_SUCCESS;
}

//--------------------------------------BIOS Data Area and CMOS---------------------------------------

// Enable NMI
EFI_STATUS
CMOSEnableNMI()
{
  EFI_STATUS Status;
  EFI_CPU_IO2_PROTOCOL *IoDev;
  UINT8 RegBValue = 0;

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }
  IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegBValue);
  RegBValue |= 0x80; // Enable NMI
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegBValue);
  return EFI_SUCCESS;
}
// Disable NMI
EFI_STATUS
CMOSDisableNMI()
{
  EFI_STATUS Status;
  EFI_CPU_IO2_PROTOCOL *IoDev;
  UINT8 RegBValue = 0;

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }
  IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegBValue);
  RegBValue &= 0x7F;
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegBValue);
  return EFI_SUCCESS;
}
//read io port data to dword table
EFI_STATUS
ReadCMOSToDWordTable(OUT UINT32 *DWordTable)
{
  EFI_STATUS Status;
  EFI_CPU_IO2_PROTOCOL *IoDev;

  UINT16 i = 0;
  UINT8 Value, Data[256];

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }
  // Disable NMI
  CMOSDisableNMI();
  for (i = 0; i < 256; i++)
  {
    IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &i);
    IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &Value);
    Data[i] = Value;
    //Print(L" Data[%d]=%x  ", i, Data[i]);
  }
  // Enable NMI
  CMOSEnableNMI();

  //gBS->Stall(5000000);
  TransferByteToDWordTable(Data, DWordTable);

  return EFI_SUCCESS;
}
//modify offset and value on bios data area table
EFI_STATUS
BDSF3(
    IN UINT64 BDSAddress)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  EFI_CPU_IO2_PROTOCOL *IoDev;

  UINT8 OffsetCount = 0, ValueCount = 0;
  UINT32 Data[64];
  UINT64 ModifyOffset, ModifyValue;
  CHAR16 OffsetArr[3] = {0}, ValueArr[8] = {0};
  BOOLEAN InputCheck = FALSE;

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }

  Print(L"\n Modify Offset: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (OffsetCount < 2))
      {
        OffsetArr[OffsetCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", OffsetArr[OffsetCount]);
        OffsetCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (OffsetCount == 2))
      {
        //Print(L"\n OffsetArr=%c %c", OffsetArr[0], OffsetArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (OffsetCount > 0) && (OffsetCount <= 2))
      {
        OffsetCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  Status = ShellConvertStringToUint64(OffsetArr, &ModifyOffset, TRUE, TRUE);

  Print(L"\n Modify Value: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < 2))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount == 2))
      {
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount <= 2))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  ShellConvertStringToUint64(ValueArr, &ModifyValue, TRUE, TRUE);

  IoDev->Mem.Write(IoDev, EfiCpuIoWidthUint8, BDSAddress + ModifyOffset, 1, &ModifyValue);
  gST->ConOut->ClearScreen(gST->ConOut);
  IoDev->Mem.Read(IoDev, EfiCpuIoWidthUint32, BDSAddress, 64, &Data);
  Print(L"BDA Memory Address:0x400 - 0x4FF\n");
  PrintTableByMode(Data, 0);
  Print(L"\nModify Offset=0x%x Modify Value=0x%x", ModifyOffset, ModifyValue);
  Print(L"\nUse <F3> to Modify the Value");
  Print(L"\nUse <Esc> to Quit");
  return EFI_SUCCESS;
}
// pci utility program
EFI_STATUS
PciMainProgram()
{
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  UINT32 PciAddrArray[1000];                           //array for pci device addresses
  UINT32 GetNumberOfPci = 0;                           //number of all pci devices
  UINT32 PciAddr = 0x80000000;                         //pci configuration space address
  UINT8 rowPci = 0, row = 1;                           //cursor position
  UINT8 PageCount = 0;                                 //count page up/down to display 20 items per page
  UINT8 Byte = 0, FTwo = 0;                            //default display mode: Byte and circle variable: FTwo
  BOOLEAN FlagTopMenu = TRUE;                          //switch to top or next menu
  UINT64 GetModifyValue, GetModifyOffset;              //return modified offset and value from Function Key 3
  PciBusScan(PciAddrArray, &GetNumberOfPci);           //scan all pci devices
  PrintTopMenu(PciAddrArray, 0, GetNumberOfPci);       //list all single function pci device
  gST->ConOut->SetCursorPosition(gST->ConOut, 1, row); //set cursor default position
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      if ((FlagTopMenu) && (SCAN_ESC == Key.ScanCode)) //escape from top menu to terminal
      {
        gST->ConOut->ClearScreen(gST->ConOut); //clear screen
        break;
      }
      if ((FlagTopMenu) && (SCAN_UP == Key.ScanCode) && (row - 1 > 0)) //move cursor on top menu - <Up>
      {
        row--;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
      }
      if ((FlagTopMenu) && (SCAN_DOWN == Key.ScanCode) && (row < 20) && ((row + PageCount * 20) < GetNumberOfPci)) //move cursor on top menu - <Down>
      {
        row++;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
        //Print(L"--%d--GetNumberOfPci=%d-----\n", row, GetNumberOfPci);
      }
      if ((FlagTopMenu) && (SCAN_PAGEDOWN == Key.ScanCode) && (((PageCount + 1) * 20) < GetNumberOfPci)) //page down top menu - <Down>
      {
        PageCount++;
        PrintTopMenu(PciAddrArray, PageCount * 20, GetNumberOfPci);
        row = 1;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
      }
      if ((FlagTopMenu) && (SCAN_PAGEUP == Key.ScanCode) && (PageCount > 0)) //page up top menu - <Up>
      {
        PageCount--;
        PrintTopMenu(PciAddrArray, PageCount * 20, GetNumberOfPci);
        row = 1;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
      }
      if ((FlagTopMenu) && (CHAR_CARRIAGE_RETURN == Key.UnicodeChar)) //select pci device from top menu - <Enter>
      {
        //row += PageCount * 20;
        rowPci = row + PageCount * 20;
        PciAddr = PciAddrArray[rowPci - 1]; // row starts from 1; row - 1 for array index
        PrintNextMenu(PciAddr, Byte);
        //Go to Next Menu
        FlagTopMenu = FALSE;
      }
      if ((!FlagTopMenu) && (SCAN_ESC == Key.ScanCode)) //quit top menu - <Esc>
      {
        //Back to Top Menu
        PrintTopMenu(PciAddrArray, PageCount * 20, GetNumberOfPci);
        FlagTopMenu = TRUE;
        row = 1;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
        FTwo = 0;
      }
      if ((!FlagTopMenu) && (SCAN_UP == Key.ScanCode) && (row - 1 > 0)) //switch pci table from sub menu - <Up>
      {
        //Switch to <UP> Pci Table
        row--;
        PciAddr = PciAddrArray[row - 1 + PageCount * 20]; //index of array = row - 1
        PrintNextMenu(PciAddr, Byte);
        //Print(L"\n----row=%d----", row);
        FTwo = 0; // reset SCAN_F2 key to default Mode
      }
      if ((!FlagTopMenu) && (SCAN_DOWN == Key.ScanCode) && ((row + 1 + PageCount * 20) <= GetNumberOfPci)) //switch pci table from sub menu - <Down>
      {
        //TODO: press down key jump to next address which is out of the page size
        // 0 < r <= 20
        if ((PageCount == 0) && (row + 1 <= GetNumberOfPci))
        {

          //Switch to <DOWN> Pci Table
          row++;
          PciAddr = PciAddrArray[row - 1 + PageCount * 20]; //index of array = row - 1
          PrintNextMenu(PciAddr, Byte);
          //Print(L"\n----row=%d----", row);
        }
        else if ((PageCount > 0) && (row + 1 <= 20))
        {
          row++;
          PciAddr = PciAddrArray[row - 1 + PageCount * 20]; //index of array = row - 1
          PrintNextMenu(PciAddr, Byte);
        }
      }
      else if ((!FlagTopMenu) && (SCAN_DOWN == Key.ScanCode) && (row + 1 <= (GetNumberOfPci % 20))) //switch pci table from sub menu - <Down>
      {
        //Switch to <DOWN> Pci Table
        row++;
        PciAddr = PciAddrArray[row - 1 + PageCount * 20]; //index of array = row - 1
        PrintNextMenu(PciAddr, Byte);
        //Print(L"\n----row=%d----", row);
      }
      if ((!FlagTopMenu) && (SCAN_F2 == Key.ScanCode)) //switch display mode from sub menu - <F2>
      {
        //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
        FTwo++;
        PrintNextMenu(PciAddr, FTwo % 3);
      }
      if ((!FlagTopMenu) && (SCAN_F3 == Key.ScanCode)) //modify pci table from sub menu - <F3>
      {
        //TODO: ModifyOffset
        //  if FTwo%3=0 ignore
        //  if FTwo%3=1 word: offset  & 0x00ff
        //  if FTwo%3=1 dword: offset & 0x000000ff
        //Print(L"\n FTwo=%d", FTwo);
        FunctionKey3(PciAddr, FTwo % 3, &GetModifyOffset, &GetModifyValue);
        PrintNextMenu(PciAddr, FTwo % 3);                                                  //print table
        Print(L"\nWrite Value 0x%02x to Offset 0x%02x ", GetModifyValue, GetModifyOffset); //prompt input offset and value
      }                                                                                    //end if ((!FlagTopMenu) && (SCAN_F3 == Key.ScanCode))
    }                                                                                      //end if (Status == EFI_SUCCESS)
    gBS->Stall(150000);                                                                    //delay mini seconds
  }
  return EFI_SUCCESS;
}
// test
EFI_STATUS
TestPciRootBridgeIoProtocol()
{
  EFI_STATUS Status;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *IoDev;
  EFI_HANDLE *HandleBuffer;
  UINTN BufferSize;
  UINTN Index;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width;
  UINT32 Value;
  UINT64 Address;

  IoDev = NULL;
  HandleBuffer = NULL;
  BufferSize = 0;
  Width = EfiPciWidthFifoUint8;
  Value = 0;
  Address = 0x000000000000000;

  Status = gBS->LocateHandleBuffer(
      ByProtocol,
      &gEfiPciRootBridgeIoProtocolGuid,
      NULL,
      &BufferSize,
      &HandleBuffer);
  Print(L"HandleBuffer=%x BufferSize=%d \r\n", HandleBuffer, BufferSize);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }
  for (Index = 0; Index < BufferSize; Index++)
  {
    Status = gBS->HandleProtocol(
        HandleBuffer[Index],
        &gEfiPciRootBridgeIoProtocolGuid,
        (VOID *)&IoDev);

    Print(L"HandleBuffer[%d]=%x\r\n", Index, HandleBuffer[Index]);

    if (EFI_ERROR(Status))
    {
      continue;
    }
  }
  if (IoDev != NULL)
  {
    //IoDev->Io.Write(IoDev, Width, PCI_INDEX_IO_PORT, 0, &Address);
    //IoDev->Io.Read(IoDev, Width, PCI_DATA_IO_PORT, 0, &Value);

    IoDev->Pci.Read(IoDev, Width, Address, 1, &Value);

    Print(L"Value=%x \n", Value);
  }

  if (HandleBuffer != NULL)
  {
    gBS->FreePool(HandleBuffer);
  }
  return EFI_SUCCESS;
}
// Bios Data Area Main Program
EFI_STATUS
BDAMainProgram()
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  EFI_CPU_IO2_PROTOCOL *IoDev;

  UINT64 BDSAddress = 0x0000000000000400;
  UINT32 Data[64];
  BOOLEAN Go = TRUE;

  gST->ConOut->ClearScreen(gST->ConOut);

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }
  IoDev->Mem.Read(IoDev, EfiCpuIoWidthUint32, BDSAddress, 64, &Data);

  Print(L"BDA Memory Address:0x400 - 0x4FF\n");
  PrintTableByMode(Data, 0);
  Print(L"\nUse <F3> to Modify the Value");
  Print(L"\nUse <Esc> to Quit");
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.ScanCode)
      {
      case SCAN_F3:
        BDSF3(BDSAddress);
        break;
      case SCAN_ESC:
        Go = FALSE;
        break;
      }
    }
    gBS->Stall(100000);
  }
  return EFI_SUCCESS;
}
//modify offset and value on cmos
EFI_STATUS
CMOSF3()
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  EFI_CPU_IO2_PROTOCOL *IoDev;

  UINT8 OffsetCount = 0, ValueCount = 0;
  UINT64 ModifyOffset, ModifyValue; // modify offset and value
  //UINT8 RegB = 0x0B;                // Enable(0) or Disable(1) NMI on bit 7
  //UINT8 RegBValue = 0;
  CHAR16 OffsetArr[3] = {0}, ValueArr[8] = {0};
  BOOLEAN InputCheck = FALSE;

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
    return Status;
  }

  gST->ConOut->SetCursorPosition(gST->ConOut, 0, 22);
  Print(L"\n Modify Offset: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (OffsetCount < 2))
      {
        OffsetArr[OffsetCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", OffsetArr[OffsetCount]);
        OffsetCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (OffsetCount == 2))
      {
        //Print(L"\n OffsetArr=%c %c", OffsetArr[0], OffsetArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (OffsetCount > 0) && (OffsetCount <= 2))
      {
        OffsetCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  Status = ShellConvertStringToUint64(OffsetArr, &ModifyOffset, TRUE, TRUE);

  Print(L"\n Modify Value: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < 2))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount == 2))
      {
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount <= 2))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  ShellConvertStringToUint64(ValueArr, &ModifyValue, TRUE, TRUE);

  // Disable NMI
  CMOSDisableNMI();

  // Modify Value on Offset
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &ModifyOffset);
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &ModifyValue);

  // Enable NMI
  CMOSEnableNMI();

  //Print(L"\nModify Offset=0x%x Modify Value=0x%x", ModifyOffset, ModifyValue);
  //Print(L"\nUse <F3> to Modify the Value");
  //Print(L"\nUse <Esc> to Quit");
  return EFI_SUCCESS;
}
// CMOS Main Program
EFI_STATUS
CMOSMainProgram()
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;

  UINT32 Data[64]; // cmos table
  BOOLEAN Go = TRUE;

  // Read and Print CMOS Table
  ReadCMOSToDWordTable(Data);
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"CMOS Register List\n\n");
  PrintTableByMode(Data, 0);
  Print(L"\nUse <F3> to Modify the Value");
  Print(L"\nUse <Esc> to Quit");

  while (Go)
  {
    ReadCMOSToDWordTable(Data);
    Data[0] &= 0xff;
    Data[3] &= 0xff; // get Reg.C
    gST->ConOut->SetCursorPosition(gST->ConOut, 3, 1);
    Print(L"%02x\n", Data[0]);
    gST->ConOut->SetCursorPosition(gST->ConOut, 39, 1);
    Print(L"%02x\n", Data[3]);
    if (Data[0] == 0x00)
    {
      //gST->ConOut->ClearScreen(gST->ConOut);
      gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
      PrintTableByMode(Data, 0);
      Print(L"\nUse <F3> to Modify the Value");
      Print(L"\nUse <Esc> to Quit");
    }

    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.ScanCode)
      {
      case SCAN_F3:
        CMOSF3();
        ReadCMOSToDWordTable(Data);
        gST->ConOut->ClearScreen(gST->ConOut);
        Print(L"CMOS Register List\n\n");
        PrintTableByMode(Data, 0);
        Print(L"\nUse <F3> to Modify the Value");
        Print(L"\nUse <Esc> to Quit");
        break;

      case SCAN_ESC:
        Go = FALSE;
        break;

      default:
        break;
      }
    }
    gBS->Stall(500000);
    //gST->ConOut->ClearScreen(gST->ConOut);
  }
  return EFI_SUCCESS;
}

//--------------------------------------Memory Utility---------------------------------------

// Insert Linked List - MemMapNode
EFI_STATUS
InsertMemMapNodeLL(
    IN EFI_MEMORY_DESCRIPTOR *MemMap,
    IN OUT MemMapNode *prev)
{
  EFI_STATUS Status;
  MemMapNode *curr = NULL;
  // Create a memory space for curr
  Status = gBS->AllocatePool(EfiBootServicesData, sizeof(MemMapNode), (VOID **)&curr);
  if (Status == EFI_OUT_OF_RESOURCES)
  {
    Print(L"Out of resource (9) %x.\r\n", Status);
    return Status;
  }
  curr->MemMap = MemMap;
  curr->next = NULL;
  prev = curr;
  return EFI_SUCCESS;
}

// Get Current Memory Allocation Map
EFI_STATUS
GetCurrentMemoryAllocationMap()
{

  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  EFI_MEMORY_DESCRIPTOR *MemMap;
  EFI_MEMORY_DESCRIPTOR *mm;
  BOOLEAN Go = TRUE;
  CHAR16 *MemTypeString;
  UINT64 TotalMemSize = 0;
  UINTN MemDescSize;
  UINT8 count = 0;
  UINT8 i = 0;
  UINTN MemEntriesCount;
  UINT32 MemTypeVsPageArray[2][30];
  //  Type    0       1     2       3       ...    29
  //  Pages   76129   259   7062    43316   ...
  for (i = 0; i < 30; i++)
  {
    MemTypeVsPageArray[0][i] = i;
    MemTypeVsPageArray[1][i] = 0;
  }

  Status = GetSystemMemoryMap(&MemMap, &MemDescSize, &MemEntriesCount);
  if (Status == EFI_SUCCESS) //fail will back to while loop
  {
//debug: trace each record
#if 0
    // print memory map:
    for (mm = MemMap; count < MemEntriesCount; mm = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mm + MemDescSize), count++)
    {
      //Print(L"This fake error is ion line %d\n", __LINE__);

      Print(L"%02d: %016lx, %05x, ", count, mm->PhysicalStart, mm->NumberOfPages);
      switch (mm->Type)
      {
      case EfiReservedMemoryType:
        Print(L"EfiReservedMemoryType\n");
        break;
      case EfiLoaderCode:
        Print(L"EfiLoaderCode\n");
        break;
      case EfiLoaderData:
        Print(L"EfiLoaderData\n");
        break;
      case EfiBootServicesCode:
        Print(L"EfiBootServicesCode\n");
        break;
      case EfiBootServicesData:
        Print(L"EfiBootServicesData\n");
        break;
      case EfiRuntimeServicesCode:
        Print(L"EfiRuntimeServicesCode\n");
        break;
      case EfiRuntimeServicesData:
        Print(L"EfiRuntimeServicesData\n");
        break;
      case EfiConventionalMemory:
        Print(L"EfiConventionalMemory\n");
        break;
      case EfiUnusableMemory:
        Print(L"EfiUnusableMemory\n");
        break;
      case EfiACPIReclaimMemory:
        Print(L"EfiACPIReclaimMemory\n");
        break;
      case EfiACPIMemoryNVS:
        Print(L"EfiACPIMemoryNVS\n");
        break;
      case EfiMemoryMappedIO:
        Print(L"EfiMemoryMappedIO\n");
        break;
      case EfiMemoryMappedIOPortSpace:
        Print(L"EfiMemoryMappedIOPortSpace\n");
        break;
      case EfiPalCode:
        Print(L"EfiPalCode\n");
        break;
      case EfiPersistentMemory:
        Print(L"EfiPersistentMemory\n");
        break;
      default:
        Print(L"%x\n", mm->Type);
      }

      Print(L"Press Any Key to Continue. Press [ESC] to Skip Print...\n");
      BOOLEAN OneStep = TRUE;
      EFI_INPUT_KEY Key;
      while (OneStep)
      {
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
        if (Status == EFI_SUCCESS)                            //fail will back to while loop
        {
          switch (Key.ScanCode)
          {
          case SCAN_ESC:
            OneStep = FALSE;    // exit while loop
            return EFI_SUCCESS; // exit function
            break;

          default:
            OneStep = FALSE; // exit while loop and continue
            break;
          }
        }
        gBS->Stall(10000);
      }
    }
#endif
  }

  /*
  EFI_STATUS Status;

  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *ArrayMemMap = NULL;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DescriptorVer = 0;

  //Get Memory Map Size first
  Status = gBS->GetMemoryMap(&MemMapSize, ArrayMemMap, &MapKey, &DescriptorSize, &DescriptorVer);
  switch (Status)
  {
    //Expect EFI_BUFFER_TOO_SMALL to get mem map size
  case EFI_BUFFER_TOO_SMALL:
    //Print(L"MemMapSize=%d\r\n", MemMapSize);
    break;
  case EFI_INVALID_PARAMETER:
    Print(L"EFI_INVALID_PARAMETER\r\n");
    return Status;
    break;
  }
  Print(L"This fake error is ion line %d\n", __LINE__);
  //Allocate Returned Memory Map Size by GetMemoryMap()
  //EFI_MEMORY_TYPE MemType = EfiBootServicesData;
  MemMapSize += EFI_PAGE_SIZE; // PAGE SIZE = 4KB
  Status = gBS->AllocatePool(EfiBootServicesData, MemMapSize, (VOID **)&ArrayMemMap);
  if (EFI_ERROR(Status))
  {
    Print(L"AllocatePool fail %x.\r\n", Status);
    return Status;
  }
  Print(L"This fake error is ion line %d\n", __LINE__);
  //Get Memory Map Info - ArrayMemMap
  Status = gBS->GetMemoryMap(&MemMapSize, ArrayMemMap, &MapKey, &DescriptorSize, &DescriptorVer);
  if (EFI_ERROR(Status))
  {
    Print(L"GetMemoryMap fail %x.\r\n", Status);
    return Status;
  }
*/

  //Transfer array to linked list
  MemMapNode *head = NULL; // head of linked list
  MemMapNode *tail = NULL; // prev is before curr node
  MemMapNode *curr = NULL; // curr node is the target for process
  MemMapNode *temp = NULL; // temp node is the for printing elements
  head = tail;
  count = 0;

  //for (UINT8 i = 0; i < MemEntriesCount; i++) the following needs to study
  // create linked list
  for (mm = MemMap; count < MemEntriesCount; mm = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mm + MemDescSize), count++)
  {
    // Create a memory space for MemMapNode type
    Status = gBS->AllocatePool(EfiBootServicesData, sizeof(MemMapNode), (VOID **)&curr);
    if (Status == EFI_OUT_OF_RESOURCES)
    {
      Print(L"Out of resource (9) %x.\r\n", Status);
      return Status;
    }
    // Set up data and ptr
    curr->MemMap = mm;
    curr->next = NULL;
    tail->next = curr;
    tail = curr;
    if (count == 0) // set up head after tail is pointed to curr
    {
      head = tail; // head point is the first node of linked list
    }
  }

  // display get memory map
  temp = head;
  count = 0;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"Type         Start            End              # Pages          Attributes\n");
  while (temp->next != NULL)
  {
    // add up pages number by memory type
    for (i = 0; i < 30; i++)
    {
      if (temp->MemMap->Type == i)
      {
        MemTypeVsPageArray[1][i] += temp->MemMap->NumberOfPages;
        //Print(L"\n MemTypeVsPageArray[1][%d]=%d \n", i, MemTypeVsPageArray[1][i]);
      }
    }

    // print Memory Type String
    MemTypeString = ConvertMemTypeToString(temp->MemMap->Type);
    if (EFI_ERROR(Status))
    {
      Print(L"PrintMemTypeToString fail %x.\r\n", Status);
      return Status;
    }
    Print(L"%12s %016lx-%016lx %015lx %016lx\n", MemTypeString, temp->MemMap->PhysicalStart, temp->MemMap->PhysicalStart + (temp->MemMap->NumberOfPages * 4096) - 1, temp->MemMap->NumberOfPages, temp->MemMap->Attribute);
    temp = temp->next;
    count++; // used for display 24 items per page

    if ((count % 22) == 21) //count the number of rows: 0 1 2 3 4 ... 23->read key stroke
    {
      Print(L"Press Any Key to Continue. Press [ESC] to Skip Print...");
      while (Go)
      {
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
        if (Status == EFI_SUCCESS)                            //fail will back to while loop
        {
          switch (Key.ScanCode)
          {
          case SCAN_ESC:
            Go = FALSE;         // exit while loop
            return EFI_SUCCESS; // exit function
            break;

          default:
            Go = FALSE; // exit while loop and continue
            gST->ConOut->ClearScreen(gST->ConOut);
            break;
          }
        }
        gBS->Stall(10000);
      }
      Print(L"Type         Start            End              # Pages          Attributes\n");
    }
    Go = TRUE; // will come back next page
  }

  // work around
  // missed end node because while loop does not process the last node
  MemTypeString = ConvertMemTypeToString(temp->MemMap->Type);
  if (EFI_ERROR(Status))
  {
    Print(L"PrintMemTypeToString fail %x.\r\n", Status);
    return Status;
  }
  Print(L"%12s %016lx-%016lx %015lx %016lx\n", MemTypeString, temp->MemMap->PhysicalStart, temp->MemMap->PhysicalStart + (temp->MemMap->NumberOfPages * 4096) - 1, temp->MemMap->NumberOfPages, temp->MemMap->Attribute);
  // add up pages number by memory type
  for (i = 0; i < 30; i++)
  {
    if (temp->MemMap->Type == i)
    {
      MemTypeVsPageArray[1][i] += temp->MemMap->NumberOfPages;
    }
  }

  // display summary
  Print(L"\r\n");
  for (i = 0; i < 30; i++)
  {
    if (MemTypeVsPageArray[1][i] >= 1)
    {
      MemTypeString = ConvertMemTypeToString(MemTypeVsPageArray[0][i]);
      if (EFI_ERROR(Status))
      {
        Print(L"PrintMemTypeToString fail %x.\r\n", Status);
        return Status;
      }
      Print(L"%12s : %d Pages (%d) \n", MemTypeString, MemTypeVsPageArray[1][i], MemTypeVsPageArray[1][i] * 4096);
    }
    TotalMemSize += MemTypeVsPageArray[1][i];
  }
  Print(L"Total Memory: %d Pages (%d) \n", TotalMemSize, TotalMemSize * 4096);
  Print(L"Press Any Key to Continue........");

  // stop and press to exit
  Go = TRUE;
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      Go = FALSE;
      gST->ConOut->ClearScreen(gST->ConOut);
    }
    gBS->Stall(1000);
  }

  return EFI_SUCCESS;
}

// called by AllocatePageByMemType
EFI_STATUS
AllocateWantedPages()
{
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  BOOLEAN Go = TRUE;
  UINTN AllocatedPageNumbers = 0;
  //UINT64 *AllocatedPageNumbers64 = 0;
  EFI_PHYSICAL_ADDRESS AllocatedAddress = 0;
  CHAR16 *PromptInputString = NULL;
  StrnCatGrow(&PromptInputString, NULL, L"Input Value", 11);

  Print(L"\r\nHow many pages do you want to allocate :");
  // input number
  //Status = Input2HexToUInt8(PromptInputString, AllocatedPageNumbers64);

  // allocate page here
  // AllocateAnyPages EfiReservedMemoryType AllocatedPages AllocatedAddress

  Print(L"Allocate %d Pages at address %016lx", AllocatedPageNumbers, AllocatedAddress);
  // display 00: 00 00 00 00 00 00 00-00 00 00 00 00 00 *.................*
  Print(L"Do you want to fill memory with value ? (y/n) :");
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (Status == EFI_SUCCESS)
    {
      //Print(L"\r\nKey.UnicodeChar=%x", Key.UnicodeChar);
      switch (Key.UnicodeChar)
      {
      case 0x0079: // y
        Print(L"Input Value : 0x");
        // input number
        // copy number to memory
        // press any key to continue ----back to main menu
        break;
      case 0x006E: // n
                   // press any key to continue ----back to main menu
        break;
      }
    }
  }
  return EFI_SUCCESS;
}

// called by AllocatePageMemoryAnyPages
EFI_STATUS
AllocatePageByMemType(
    IN UINT8 ByMemType)
{
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  BOOLEAN Go = TRUE;
  CHAR16 *MemTypeString;

  MemTypeString = ConvertMemTypeToString(ByMemType);
  Print(L"Now Process Allocatepages. (AllocateAnyPages) (%s)\r\n \
  Do you want to allocate Pages for all available Memory ? (y/n) :\r\n  \
  --------------------------------------------------------------\r\n",
        MemTypeString);
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (Status == EFI_SUCCESS)
    {
      Print(L"\r\nKey.UnicodeChar=%x", Key.UnicodeChar);
      switch (Key.UnicodeChar)
      {
      case 0x0079: // y

        break;
      case 0x006E: // n
        AllocateWantedPages();
        /*
        Status = AllocateWantedPages();
        if (Status == EFI_SUCCESS)
        {
          Print(L"------EFI_SUCCESS------");
        }
        else
        {
          Print(L"------EFI_ERROR------");
        }
        */
        break;

      default:
        if (Key.ScanCode == SCAN_ESC)
        {
          Go = FALSE;
        }
        break;
      }
    }
    gBS->Stall(10000);
  }
  return EFI_SUCCESS;
}

// Allocate Pool Memory
EFI_STATUS
AllocatePoolMemory()
{

  return EFI_SUCCESS;
}

//------------------------------Main Funciton-------------------------------------------
/**
  as the real entry point for the application.
  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.
  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.
**/
EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
  UINT8 retSelection = 0;
  BOOLEAN Go = TRUE;
  gST->ConOut->ClearScreen(gST->ConOut);

  SelNum1to5Menu(MainMenuMsgTop, &retSelection);
  while (Go)
  {
    switch (retSelection)
    {
    case 1:
      PciMainProgram();
      SelNum1to5Menu(MainMenuMsgTop, &retSelection);
      break;
    case 2:
      BDAMainProgram();
      SelNum1to5Menu(MainMenuMsgTop, &retSelection);
      break;
    case 3:
      CMOSMainProgram();
      SelNum1to5Menu(MainMenuMsgTop, &retSelection);
      break;
    case 4:
      MemMainProgram();
      SelNum1to5Menu(MainMenuMsgTop, &retSelection);
      break;
    case 0:
      gST->ConOut->ClearScreen(gST->ConOut);
      Go = FALSE; // Exit while(Go) loop
      break;
    }
  }
  return EFI_SUCCESS;
}

//--------------------------------Function implement-----------------------------------------------

EFI_STATUS SelNum1to12Menu(IN CONST CHAR8 *promptString, OUT UINT8 *retSelNum)
{
  AsciiPrint("%a", promptString);
  Input2HexToUInt8(retSelNum);
  //Print(L"\r\n retSelNum=%d", *retSelNum);
  return EFI_SUCCESS;
}

EFI_STATUS SelNum1to5Menu(IN CONST CHAR8 *promptString, OUT UINT8 *retSelNum)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN Go = TRUE;
  gST->ConOut->ClearScreen(gST->ConOut);

  AsciiPrint("%a", promptString);
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.UnicodeChar)
      {
      case CHAR_UNICODE_1:
        *retSelNum = 1;
        Go = FALSE;
        break;
      case CHAR_UNICODE_2:
        *retSelNum = 2;
        Go = FALSE;
        break;
      case CHAR_UNICODE_3:
        *retSelNum = 3;
        Go = FALSE;
        break;
      case CHAR_UNICODE_4:
        *retSelNum = 4;
        Go = FALSE;
        break;
      case CHAR_UNICODE_5:
        *retSelNum = 5;
        Go = FALSE;
        break;
      default:
        if (Key.ScanCode == SCAN_ESC)
        {
          *retSelNum = 0;
          Go = FALSE;
        }
        break;
      }
    }
    gBS->Stall(10000);
  }
  return EFI_SUCCESS;
}

EFI_STATUS SelYorNMenu(IN CONST CHAR8 *promptString, OUT UINT8 *retSelNum)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN Go = TRUE;

  AsciiPrint("%a", promptString);
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.UnicodeChar)
      {
      case 0x0079: // y is 1
        *retSelNum = 1;
        Go = FALSE;
        break;
      case 0x006E: // n is 0
        *retSelNum = 0;
        Go = FALSE;
        break;
      }
    }
    gBS->Stall(10000);
  }
  return EFI_SUCCESS;
}

EFI_STATUS Input2HexToUInt8(OUT UINT8 *OutputValue)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN InputCheck = FALSE;
  UINT8 ValueCount = 0;
  CHAR16 ValueArr[8] = {0};

  Print(L"\r\nInput Value: ");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < 2))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount > 0))
      {
        //Print(L"\n ValueArr=%c %c", ValueArr[0], ValueArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount <= 2))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  //Status = ShellConvertStringToUint64(ValueArr, OutputValue, TRUE, TRUE);
  *OutputValue = StrDecimalToUintn(ValueArr);
  //Print(L"\r\n----OutputValue=%d", *OutputValue);
  return EFI_SUCCESS;
}

EFI_STATUS Input2HexToUInt64(OUT UINT64 *OutputValue)
{

  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN InputCheck = FALSE;
  UINT8 ValueCount = 0;
  CHAR16 ValueArr[8] = {0};

  Print(L"\r\nInput Value: 0x");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < 2))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount == 2))
      {
        //Print(L"\n ValueArr=%c %c", ValueArr[0], ValueArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount <= 2))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  Status = ShellConvertStringToUint64(ValueArr, OutputValue, TRUE, TRUE);
  //*OutputValue = StrDecimalToUintn(ValueArr);
  //Print(L"\r\n----OutputValue=%d", *OutputValue);
  return EFI_SUCCESS;
}

EFI_STATUS Input8HexToUInt64(OUT UINT64 *OutputValue)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN InputCheck = FALSE;
  UINT8 ValueCount = 0;
  CHAR16 ValueArr[8] = {0};

  Print(L"\r\nInput Value: ");
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (EFI_SUCCESS == Status)
    {
      //Print(L"\n\r Scancode [0x%4x],   UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      InputCheck = ShellIsHexaDecimalDigitCharacter(Key.UnicodeChar);
      if ((InputCheck) && (ValueCount < 8))
      {
        ValueArr[ValueCount] = (Key.UnicodeChar & 0x00ff);
        Print(L"%c", ValueArr[ValueCount]);
        ValueCount++;
      }
      if ((CHAR_CARRIAGE_RETURN == Key.UnicodeChar) && (ValueCount > 0))
      {
        //Print(L"\n ValueArr=%c %c", ValueArr[0], ValueArr[1]);
        break;
      }
      if ((CHAR_BACKSPACE == Key.UnicodeChar) && (ValueCount > 0) && (ValueCount < 8))
      {
        ValueCount--;
        Print(L"\b");
        Print(L" ");
        Print(L"\b");
      }
    }
    gBS->Stall(15000);
  }
  //Status = ShellConvertStringToUint64(ValueArr, OutputValue, TRUE, TRUE);
  *OutputValue = StrDecimalToUintn(ValueArr);
  //Print(L"\r\n----OutputValue=%d", *OutputValue);
  return EFI_SUCCESS;
}

EFI_STATUS PressAnyKeyToContinue()
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  BOOLEAN Go = TRUE;

  Print(L"\r\nPress Any Key To Continue....");
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.UnicodeChar)
      {
      default:
        Go = FALSE;
        break;
      }
    }
    gBS->Stall(10000);
  }
  return EFI_SUCCESS;
}

// Memory Utility Main Program
EFI_STATUS MemMainProgram()
{
  UINT8 retSelection = 0;
  BOOLEAN Go = TRUE;
  gST->ConOut->ClearScreen(gST->ConOut);

  SelNum1to5Menu(MemUtiMsgTop, &retSelection);
  //Print(L"---retSelection=%d----", retSelection);
  //gBS->Stall(100000000);
  while (Go)
  {
    switch (retSelection)
    {
    case 1:
      GetCurrentMemoryAllocationMap();
      SelNum1to5Menu(MemUtiMsgTop, &retSelection);
      break;
    case 2:
      AllocatePageMemory();
      SelNum1to5Menu(MemUtiMsgTop, &retSelection);
      break;
    case 3: // Not Ready Yet
      //AllocatePoolMemory();
      SelNum1to5Menu(MemUtiMsgTop, &retSelection);
      break;
    case 4: // Not Ready Yet
      //ShowAllocatedMemory();
      SelNum1to5Menu(MemUtiMsgTop, &retSelection);
      break;
    case 5: // Not Ready Yet
      //FreeAllocatedMemory();
      SelNum1to5Menu(MemUtiMsgTop, &retSelection);
      break;
    case 0:
      Go = FALSE;
      break;
    }
  }
  return EFI_SUCCESS;
}

// called by MemMainProgram
EFI_STATUS
AllocatePageMemory()
{
  UINT8 retSelection = 0;
  BOOLEAN Go = TRUE;
  EFI_ALLOCATE_TYPE allocateTypeSel;

  while (Go)
  {
    gST->ConOut->ClearScreen(gST->ConOut);
    SelNum1to5Menu(AllocatePageMemTop, &retSelection);
    allocateTypeSel = retSelection - 1; // enum start from 0, so after select number need to minus 1
    switch (retSelection)
    {
    case 1: // AllocateAnyPages
      AllocatePageMemoryAnyPages(allocateTypeSel);
      break;
    case 2: // AllocateMaxAddress
      //AllocateMaxAddress();
      SelNum1to5Menu(AllocatePageMemTop, &retSelection);
      break;
    case 3: // AllocateAddress
      //AllocateAddress();
      SelNum1to5Menu(AllocatePageMemTop, &retSelection);
      break;
    case 0:
      Go = FALSE;
      break;
    }
  }
  return EFI_SUCCESS;
}

// called by AllocatePageMemory
EFI_STATUS
AllocatePageMemoryAnyPages(IN EFI_ALLOCATE_TYPE allocateTypeSel)
{
  EFI_PHYSICAL_ADDRESS allocatePagesRetPhyAddr;
  EFI_STATUS Status;
  EFI_MEMORY_TYPE allocateMemTypeSel;
  CHAR16 *allocateMemTypeSelToString = NULL;
  UINT8 retSelNum = 0;
  UINT8 retYorN = 0;
  //UINT64 SetValue = 0;
  UINT64 allocatePagesSel = 0;
  //UINT64 allocatePagesSelBytes = 0;
  UINT64 count = 0;
  //UINT8 *ptr = NULL;
  BOOLEAN Go = TRUE;
  BOOLEAN NotImplementCustomizeAllocatedPages = TRUE;
  //Transfer array to linked list
  PApageNode *head = NULL; // head of linked list
  PApageNode *tail = NULL; // prev is before curr node
  PApageNode *curr = NULL; // curr node is the target for process
  PApageNode *temp = NULL; // temp node is the for printing elements
  head = tail;

  gST->ConOut->ClearScreen(gST->ConOut);

  // print supported memory type
  Print(L"Now Process Allocatepages ( AllocateAnyPages )\r\nSupported Memory Type :\r\n\
  ---------------------------------------------------\r\n");
  // print and select memory type from menu
  SelNum1to12Menu(SelMemTypeNum, &retSelNum);
  gST->ConOut->ClearScreen(gST->ConOut);

  // number -> memory type string
  allocateMemTypeSel = retSelNum - 1;
  //Print(L"\r\n allocateMemType=%d", allocateMemTypeSel);
  allocateMemTypeSelToString = ConvertMemoryType(allocateMemTypeSel);
  Print(L"\r\nNow Process Allocatepages ( AllocateAnyPages ) ( %s )\r\nDo you want to allocate Pages for all available Memory ? (y/n) :", allocateMemTypeSelToString);
  SelYorNMenu("", &retYorN);
  // y or n to allocate all memory
  switch (retYorN)
  {
    // y start from 4KiB -> 2KiB -> 1Kib -> 512B -> 256B -> 128B ... -> 4
  case 1:                    // all memory
    allocatePagesSel = 4096; // faster this process when running
    count = 0;
    while (Go)
    {
      Status = gBS->AllocatePages(allocateTypeSel, allocateMemTypeSel, allocatePagesSel, &allocatePagesRetPhyAddr);
      if (Status == EFI_SUCCESS)
      {
        // Create a memory space for new allocated page: EfiBootServicesData why??
        Status = gBS->AllocatePool(EfiBootServicesData, sizeof(PApageNode), (VOID **)&curr);
        if (Status == EFI_OUT_OF_RESOURCES)
        {
          Print(L"AllocatePool is Out of resource (9) %x.\r\n", Status);
          return Status;
        }
        // Set up data and ptr
        curr->PAType = allocateTypeSel;
        curr->PAMemType = allocateMemTypeSel;
        curr->PAPage = allocatePagesSel;
        curr->PAPhyAddr = allocatePagesRetPhyAddr;
        curr->next = NULL;
        tail->next = curr;
        tail = curr;

        if (count == 0) // set up head after tail is pointed to curr
        {
          head = tail; // head point is the first node of linked list
        }

        Print(L"\r\n%d Allocate %d Pages at address %016lx OK!", count, allocatePagesSel, allocatePagesRetPhyAddr);
        count++;
      }
      else if (Status == EFI_OUT_OF_RESOURCES)
      {
        //Print(L"\r\nEFI_OUT_OF_RESOURCES=%x", Status);
        Print(L"\r\nAllocate %d Pages Failed!! Try to Allocate %d Pages!", allocatePagesSel, allocatePagesSel / 2);
        allocatePagesSel = allocatePagesSel / 2;
        PressAnyKeyToContinue();
        if (allocatePagesSel == 4)
        {
          Print(L"\r\nAllocate %d Pages Failed!!", allocatePagesSel);
          Print(L"\r\nBack to previous menu!!");
          PressAnyKeyToContinue();
          Go = FALSE; // back to AllocatePageMemory
        }
      }
      else if (Status == EFI_INVALID_PARAMETER)
      {
        Print(L"\r\nEFI_INVALID_PARAMETER=%x", Status);
        PressAnyKeyToContinue();
        Go = FALSE;
      }
      else if (Status == EFI_NOT_FOUND)
      {
        Print(L"\r\nEFI_NOT_FOUND=%x", Status);
        PressAnyKeyToContinue();
        Go = FALSE;
      }
      else
      {
        Print(L"\r\nUnknown Status=%x", Status);
        PressAnyKeyToContinue();
        Go = FALSE;
      }
    }
    break;
  // n
  case 0: // customize pages
    Print(L"\r\nNot Ready Yet! ");

//--------------------------to be implemented-------------------------------------------
#if 0
    Print(L"\r\nHow many pages do you want to allocate : ");
    Input8HexToUInt64(&allocatePagesSel);
    //allocatePagesSel += SIZE_4KB;

    Status = gBS->AllocatePages(allocateTypeSel, allocateMemTypeSel, allocatePagesSel, &allocatePagesRetPhyAddr);
    if (Status == EFI_SUCCESS)
    {
      Print(L"\r\nAllocate %d Pages at address %016lx", allocatePagesSel, allocatePagesRetPhyAddr);
    }
    else
    {
      Print(L"\r\n%S Allocate %016lx Pages Error!!", allocatePagesRetPhyAddr);
    }



  //  1 page = 4KiB = 4096 bytes
  allocatePagesSelBytes = allocatePagesSel * 4096;

  // set memory value to 0
  gBS->SetMem((VOID *)allocatePagesRetPhyAddr, (UINTN)allocatePagesSelBytes, (UINT8)0);
  // show memory value
  ptr = (UINT8 *)allocatePagesRetPhyAddr;

  for (UINT8 i = 0; i < 64; i++)
  {
    if (i % 16 == 0)
    {
      Print(L"\r\n%02x:", i);
    }
    else if (i % 16 == 7)
    {
      Print(L" -");
    }
    else if (i % 16 == 15)
    {
      Print(L" %02x *................*", *ptr);
      ptr++;
    }
    else
    {
      Print(L" %02x", *ptr);
      ptr++;
    }
  }

  Print(L"\r\nDo you want to fill memory with value ?(y/n) :");
  Status = SelYorNMenu("", &retYorN);
  if (Status == EFI_SUCCESS)
  {
    //Print(L"\r\nPass");
  }
  else
  {
    Print(L"\r\n%S Error Code = %x !", __FUNCTION__, Status);
  }
  // fill value in memory
  switch (retYorN)
  {
  case 1: // y -> fill in value
    Input2HexToUInt64(&SetValue);
    //Print(L"SetValue=%02x", SetValue);
    //Print(L"\r\nallocatePagesSelBytes=%x", &allocatePagesRetPhyAddr);
    //Print(L"\r\nallocatePagesSelBytes=%x", allocatePagesRetPhyAddr);

    gBS->SetMem((VOID *)allocatePagesRetPhyAddr, (UINTN)allocatePagesSelBytes, (UINT8)SetValue);
    // show memory value
    ptr = (UINT8 *)allocatePagesRetPhyAddr;

    for (UINT8 i = 0; i < 64; i++)
    {
      if (i % 16 == 0)
      {
        Print(L"\r\n%02x:", i);
      }
      else if (i % 16 == 7)
      {
        Print(L" -");
      }
      else if (i % 16 == 15)
      {
        Print(L" %02x *................*", *ptr);
        ptr++;
      }
      else
      {
        Print(L" %02x", *ptr);
        ptr++;
      }
    }

    break;
  case 0: // n -> ignore
    break;
  }
#endif

    break;
  } // end of switch case: allocate all memory or not

  if (NotImplementCustomizeAllocatedPages)
  { // not implement function exit
    return EFI_SUCCESS;
  }
  Print(L"\r\n Free Allocated Memory");
  PressAnyKeyToContinue();
  temp = head;
  count = 0;
  while (temp->next != NULL)
  {
    Status = gBS->FreePages(temp->PAPhyAddr, temp->PAPage);
    if (Status != EFI_SUCCESS)
    {
      Print(L"\r\nFree Pages Failed!! %x", Status);
    }
    Print(L"\r\n %d Free Physical Addr= %016lx   Pages=%d", count, temp->PAPhyAddr, temp->PAPage);
    temp = temp->next;
    count++;
  }
  Status = gBS->FreePages(temp->PAPhyAddr, temp->PAPage);
  if (Status != EFI_SUCCESS)
  {
    Print(L"\r\nFree Pages Failed!! %x", Status);
  }
  Print(L"\r\n %d Free Physical Addr= %016lx   Pages=%d", count, temp->PAPhyAddr, temp->PAPage);
  PressAnyKeyToContinue();

  return EFI_SUCCESS;
}