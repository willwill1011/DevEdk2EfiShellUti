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

extern EFI_SYSTEM_TABLE *gST;

#define PCI_INDEX_IO_PORT 0xCF8
#define PCI_DATA_IO_PORT 0xCFC

#define SCAN_NULL 0x0000
#define SCAN_UP 0x0001
#define SCAN_DOWN 0x0002
#define SCAN_F2 0x000C
#define SCAN_F3 0x000D
#define SCAN_ESC 0x0017

#define CHAR_CARRIAGE_RETURN 0x000D

//top menu display
EFI_STATUS
PrintTopMenu(
    OUT UINT32 *PciAddrArray,
    OUT UINT8 *ptrNumberOfPci)
{
  UINT8 NumberOfPci = 0;
  UINT16 Bus;
  UINT8 Dev;
  UINT8 Fun;
  UINT16 VenID, DevID;
  UINT32 Addr = 0x80000000;

  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L" No# \t\t Bus \t\t Dev \t\t Fun \t\t VenID \t\t DevID \t\t Addr \n");
  for (Bus = 0; Bus < 256; Bus++)
  {
    for (Dev = 0; Dev < 32; Dev++)
    {
      for (Fun = 0; Fun < 8; Fun++)
      {

        Addr &= 0xFF000000;                               //clean the address
        Addr |= ((Bus << 16) | (Dev << 11) | (Fun << 8)); //set the pci bus dev and fun
        IoWrite32(PCI_INDEX_IO_PORT, Addr);
        DevID = (IoRead32(PCI_DATA_IO_PORT) & 0xffff0000) >> 16;
        VenID = (IoRead32(PCI_DATA_IO_PORT) & 0x0000ffff);
        if ((VenID == 0xffff) && (DevID == 0xffff))
        {
          continue;
        }
        PciAddrArray[NumberOfPci] = Addr;
        Print(L" %02d \t \t \t %02d \t \t %02d \t \t %02d \t \t %04x \t \t %04x \t %08x \t\n", NumberOfPci + 1, Bus, Dev, Fun, VenID, DevID, PciAddrArray[NumberOfPci]);
        NumberOfPci++;
      }
    }
  }
  Print(L"\nUse <UP>/<DOWN> to Select PCI Device\n");
  Print(L"\nUse <Enter> to See Device Table\n");
  Print(L"\nUse <Esc> to Quit\n");
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
    for (UINT16 j = 0; j < 64; j++)
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
      Print(L"%02x\t", i); //print column index: 00 01 .... 0F
    }
    Print(L"\n");

    for (j = 0; j < 256; j++)
    {
      if (j % 16 == 0)
      {
        Print(L"%02x\t", k++); //print row index: 00 01 ... 0F
        Print(L"%02x\t", ByteTable[j]);
      }
      else if (j % 16 == 15)
      {
        Print(L"%02x\t\n", ByteTable[j]);
      }
      else
      {
        Print(L"%02x\t", ByteTable[j]);
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
        Print(L"%02x\t", k++);          //print row index: transpose(00 01 ... 0F)
        Print(L"%04x\t", WordTable[j]); // 00 8086 1237 ...
      }
      else if (j % 8 == 7)
      {
        Print(L"%04x\t\n", WordTable[j]); // new line
      }
      else
      {
        Print(L"%04x\t", WordTable[j]);
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
        Print(L"%02x\t", k++);         //print row index: transpose(00 01 ... 0F)
        Print(L"%08x\t", PciTable[j]); // 00 80861237 00000000 ...
      }
      else if (j % 4 == 3)
      {
        Print(L"%08x\t\n", PciTable[j]); // new line
      }
      else
      {
        Print(L"%08x\t", PciTable[j]);
      }
    }
    break;
  }
  return EFI_SUCCESS;
}
//top bar display Bus Decvice Function Number
EFI_STATUS
PciAddrToBDF(
    IN UINT32 PciAddr)
{
  UINT16 B;
  UINT8 D, F;
  B = (0x00FF0000 & PciAddr) >> 16;
  D = (0x0000F800 & PciAddr) >> 11;
  F = (0x00000700 & PciAddr) >> 8;
  Print(L"\t ---PciAddr: %8x \t Bus=%2x \t Dev=%2x \t Fun=%2x ---\n\n", PciAddr, B, D, F);
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

  gST->ConOut->ClearScreen(gST->ConOut);
  PortIoRead32(PciAddr, PciTable);
  PciAddrToBDF(PciAddr);
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

  UINT32 PciAddrArray[1000]; //array for pci device addresses
  UINT8 GetNumberOfPci = 0;  //number of all pci devices

  UINT32 PciAddr = 0x80000000; //pci configuration space address
  UINT8 col = 1, row = 1;      //cursor position

  UINT8 Byte = 0, FTwo = 0;   //default display mode: Byte and circle variable: FTwo
  BOOLEAN FlagTopMenu = TRUE; //flag to top or sub menu

  BOOLEAN *OffsetErr, *ValueErr;              //check input status
  CHAR16 *OffsetStr = NULL, *ValueStr = NULL; //input strings
  UINT64 ModifyOffset = 0, ModifyValue = 0;   //string to uint

  PrintTopMenu(PciAddrArray, &GetNumberOfPci);                           //print all pci devices on top menu
  SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, col, row); //set cursor default position

  while (1)
  {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); //read key from keyboard
    if (Status == EFI_SUCCESS)                            //fail will back to while loop
    {
      //Print(L"\n\r Scancode [0x%4x], \t UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
      if ((FlagTopMenu) && (SCAN_ESC == Key.ScanCode)) //escape from top menu to terminal
      {
        SystemTable->ConOut->ClearScreen(gST->ConOut); //clear screen
        break;
      }
      if ((FlagTopMenu) && (SCAN_UP == Key.ScanCode) && (row - 1 > 0)) //move cursor on top menu - <Up>
      {
        row--;
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, 1, row);
      }
      if ((FlagTopMenu) && (SCAN_DOWN == Key.ScanCode) && (row + 1 <= GetNumberOfPci)) //move cursor on top menu - <Down>
      {
        row++;
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, 1, row);
      }
      if ((FlagTopMenu) && (CHAR_CARRIAGE_RETURN == Key.UnicodeChar)) //select pci device from top menu - <Enter>
      {
        PciAddr = PciAddrArray[row - 1]; // row starts from 1; row - 1 for array index
        PrintNextMenu(PciAddr, Byte);
        //Go to Next Menu
        FlagTopMenu = FALSE;
      }
      if ((!FlagTopMenu) && (SCAN_ESC == Key.ScanCode)) //quit top menu - <Esc>
      {
        //Back to Top Menu
        PrintTopMenu(PciAddrArray, &GetNumberOfPci);
        FlagTopMenu = TRUE;
        row = 1;
        col = 1;
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, col, row);
        FTwo = 0;
      }
      if ((!FlagTopMenu) && (SCAN_UP == Key.ScanCode) && (row - 1 > 0)) //switch pci table from sub menu - <Up>
      {
        //Switch to <UP> Pci Table
        row--;
        PciAddr = PciAddrArray[row - 1]; //index of array = row - 1
        PrintNextMenu(PciAddr, Byte);
        FTwo = 0; // reset SCAN_F2 key to default Mode
      }
      if ((!FlagTopMenu) && (SCAN_DOWN == Key.ScanCode) && (row + 1 <= GetNumberOfPci)) //switch pci table from sub menu - <Down>
      {
        //Switch to <DOWN> Pci Table
        row++;
        PciAddr = PciAddrArray[row - 1]; //index of array = row - 1
        PrintNextMenu(PciAddr, Byte);
      }
      if ((!FlagTopMenu) && (SCAN_F2 == Key.ScanCode)) //switch display mode from sub menu - <F2>
      {
        //Print(L"\n\r Scancode [0x%4x], \t UnicodeChar [%4x] \n\r", Key.ScanCode, Key.UnicodeChar);
        FTwo++;
        PrintNextMenu(PciAddr, FTwo % 3);
      }
      if ((!FlagTopMenu) && (SCAN_F3 == Key.ScanCode)) //modify pci table from sub menu - <F3>
      {

        Print(L"\n");
        do
        {
          ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"Modify Offset: 0x", (VOID **)&OffsetStr); //offset input
          if (OffsetStr != NULL)
          {
            CheckHexValue(OffsetStr, FTwo, OffsetErr);                        //check hex format and length matched up with FTwo(Byte/word/DWord)
            ShellConvertStringToUint64(OffsetStr, &ModifyOffset, TRUE, TRUE); //convert string to uint
          }
        } while ((OffsetStr == NULL) || (!*OffsetErr));
        do
        {
          ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"\tModify Value: 0x", (VOID **)&ValueStr); //value input
          if (ValueStr != NULL)
          {
            CheckHexValue(ValueStr, FTwo, ValueErr);                        //check hex format and length matched up with FTwo(Byte/word/DWord)
            ShellConvertStringToUint64(ValueStr, &ModifyValue, TRUE, TRUE); //convert string to uint
          }
        } while ((ValueStr == NULL) || (!*ValueErr));
        //TODO: ModifyOffset
        //  if FTwo%3=0 ignore
        //  if FTwo%3=1 word: offset  & 0x00ff
        //  if FTwo%3=1 dword: offset & 0x000000ff

        switch (FTwo % 3)
        {
        case 1:
          ModifyOffset = ModifyOffset & 0x00ff;
          break;
        case 2:
          ModifyOffset = ModifyOffset & 0x000000ff;
          break;
        }

        IoWrite32(PCI_INDEX_IO_PORT, PciAddr + ModifyOffset);                 //write offset addr
        IoWrite32(PCI_DATA_IO_PORT, ModifyValue);                             //write value
        PrintNextMenu(PciAddr, FTwo % 3);                                     //print table
        Print(L" \nWrite Value %x to Offset %x ", ModifyValue, ModifyOffset); //prompt input offset and value
      }                                                                       //end if ((!FlagTopMenu) && (SCAN_F3 == Key.ScanCode))
    }                                                                         //end if (Status == EFI_SUCCESS)
    gBS->Stall(150000);                                                       //delay mini seconds
  }                                                                           //end while(1)

  /*
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *SimpleEx;
  //EFI_KEY_DATA KeyData;
  EFI_STATUS Status;

  Status = gBS->OpenProtocol(
      gST->ConsoleInHandle,
      &gEfiSimpleTextInputExProtocolGuid,
      (VOID **)&SimpleEx,
      ImageHandle,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

  if (EFI_ERROR(Status))
  {
    printf("OpenProtocol ERROR!!\n");
  }

  Status = SimpleEx->ReadKeyStrokeEx(
      SimpleEx,
      &KeyData);

*/
  return 0;
}
