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

#include <Protocol/PciRootBridgeIo.h> // EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.Io.Read/Write
#include <Protocol/CpuIo2.h>          // EFI_CPU_IO2_PROTOCOL_GUID.Io.Read/Write

extern EFI_SYSTEM_TABLE *gST;

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

#define PciUtility 1
#define BDSUtility 2
#define CMOSUtility 3

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
//replaced by PrintTableByMode
EFI_STATUS
TransferDWordToWordTable(
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
    //Transfer Word To Byte Table
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
// not used
EFI_STATUS
PrintMainMenu()
{
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  UINT8 row = 1;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L" Utility List: \n 1. PCI Utility \n 2. BIOS Data Area Utility \n 3. CMOS Utility\n");
  Print(L"\n [Esc] to Exit \n");
  gST->ConOut->SetCursorPosition(gST->ConOut, 1, row); //set cursor default position
  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      if (SCAN_ESC == Key.ScanCode) //escape from top menu to terminal
      {
        gST->ConOut->ClearScreen(gST->ConOut);
        break;
      }
      if ((SCAN_UP == Key.ScanCode) && (row - 1 > 0)) //move cursor on top menu - <Up>
      {
        row--;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
      }
      if ((SCAN_DOWN == Key.ScanCode) && (row < 3)) //move cursor on top menu - <Down>
      {
        row++;
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
      }
      if (CHAR_CARRIAGE_RETURN == Key.UnicodeChar) //select pci device from top menu - <Enter>
      {
        switch (row)
        {
        case PciUtility:
          PciMainProgram();
          break;

        case BDSUtility:
          break;

        case CMOSUtility:
          break;
        }
        gST->ConOut->ClearScreen(gST->ConOut);
        Print(L" Utility List: \n 1. PCI Utility \n 2. BIOS Data Area Utility \n 3. CMOS Utility\n");
        Print(L"\n [Esc] to Exit \n");
        gST->ConOut->SetCursorPosition(gST->ConOut, 1, row); //set cursor default position
      }
    }
    gBS->Stall(15000);
  }
  return EFI_SUCCESS;
}
// prompt words on menu
EFI_STATUS
PromptMainMenu()
{
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L" Utility List: \n 1. PCI Utility \n 2. BIOS Data Area Utility \n 3. CMOS Utility\n");
  Print(L"\n [Esc] to Exit \n");
  gST->ConOut->SetCursorPosition(gST->ConOut, 1, 1); //set cursor default position
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
// CMOS Main Program
EFI_STATUS
CMOSMainProgram()
{
  EFI_STATUS Status;
  //EFI_INPUT_KEY Key;
  EFI_CPU_IO2_PROTOCOL *IoDev;

  //UINT8 RegB = 0;
  UINT16 i = 0;
  UINT16 j, k;
  //UINT8 RegBValue;
  UINT8 Value, Data[256];
  //BOOLEAN Go = TRUE;

  gST->ConOut->ClearScreen(gST->ConOut);

  Status = gBS->LocateProtocol(&gEfiCpuIo2ProtocolGuid, NULL, (VOID **)&IoDev);
  if (EFI_ERROR(Status))
  {
    Print(L"LocateHandleBuffer fail.\r\n");
  }

  while (1)
  {
    for (i = 0; i < 256; i++)
    {
      IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &i);
      IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &Value);
      Data[i] = Value;
      //Print(L" Data[%d]=%x ", i, Data[i]);
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
        Print(L"%02x ", Data[j]);
      }
      else if (j % 16 == 15)
      {
        Print(L"%02x \n", Data[j]);
      }
      else
      {
        Print(L"%02x ", Data[j]);
      }
    }
    gBS->Stall(1000000);
    gST->ConOut->ClearScreen(gST->ConOut);
  }
  /*
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegB);
  IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &RegBValue);
  Print(L"\n RegBValue=%x\n", RegBValue);
  RegBValue = RegBValue | 0x80; // Disable NMI
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_INDEX_IO_PORT, 1, &RegB);
  IoDev->Io.Write(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &RegBValue);
  IoDev->Io.Read(IoDev, EfiCpuIoWidthUint8, CMOS_DATA_IO_PORT, 1, &RegBValue);
  Print(L"\n After Disable NMI RegBValue=%x\n", RegBValue);
  
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

  */
  return EFI_SUCCESS;
}

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
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  UINT8 row = 1;
  BOOLEAN Go = TRUE;

  PromptMainMenu();
  while (Go)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      switch (Key.ScanCode)
      {
      case SCAN_ESC:
        gST->ConOut->ClearScreen(gST->ConOut);
        Go = FALSE; // Exit while(Go) loop
        break;
      case SCAN_UP:
        if (row - 1 > 0)
        {
          row--;
          gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
        }
        break;
      case SCAN_DOWN:
        if (row < 3)
        {
          row++;
          gST->ConOut->SetCursorPosition(gST->ConOut, 1, row);
        }
        break;
      default: //instead of case CHAR_CARRIAGE_RETURN because it is NOT ScanCode but UnicodeChar
        switch (row)
        {
        case PciUtility:
          PciMainProgram();
          break;
        case BDSUtility:
          BDAMainProgram();
          break;
        case CMOSUtility:
          CMOSMainProgram();
          break;
        }
        PromptMainMenu();
        break;
      }
    }
    gBS->Stall(15000);
  }
  return EFI_SUCCESS;
}