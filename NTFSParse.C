#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>

// Function to print error messages
void PrintError(const char* msg) {
    std::cerr << msg << " Error code: " << GetLastError() << std::endl;
}

// Function to read sectors from the volume
bool ReadSectors(HANDLE hVolume, ULONGLONG startSector, DWORD sectorCount, DWORD sectorSize, std::vector<BYTE>& buffer) {
    LARGE_INTEGER sectorOffset;
    sectorOffset.QuadPart = startSector * sectorSize;

    DWORD bytesRead;
    SetFilePointerEx(hVolume, sectorOffset, NULL, FILE_BEGIN);
    if (!ReadFile(hVolume, buffer.data(), sectorCount * sectorSize, &bytesRead, NULL) || bytesRead != sectorCount * sectorSize) {
        PrintError("Failed to read sectors");
        return false;
    }
    return true;
}

// Function to convert NTFS timestamp to human-readable format
std::string FileTimeToString(ULONGLONG fileTime) {
    // NTFS time starts from January 1, 1601 (UTC)
    // Convert fileTime from 100-nanosecond intervals to seconds
    ULONGLONG seconds = fileTime / 10000000ULL;
    // Unix time starts from January 1, 1970 (UTC)
    // Difference between 1601 and 1970 in seconds
    const ULONGLONG EPOCH_DIFFERENCE = 11644473600ULL;
    time_t unixTime = static_cast<time_t>(seconds - EPOCH_DIFFERENCE);

    struct tm timeinfo;
    char buffer[80];
    gmtime_s(&timeinfo, &unixTime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return std::string(buffer);
}

// Function to parse and print an MFT record
void ParseAndPrintMFTRecord(const std::vector<BYTE>& buffer) {
    std::string signature(buffer.begin(), buffer.begin() + 4);
    if (signature == "FILE") {
        std::cout << "-------------------------" << std::endl;
        // Print in color: MFT Record found
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
        std::cout << "MFT Record found" << std::endl;
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color

        // Parse MFT Record Header
        DWORD fixupOffset = *reinterpret_cast<const WORD*>(&buffer[4]);
        DWORD fixupEntryCount = *reinterpret_cast<const WORD*>(&buffer[6]);
        ULONGLONG lsn = *reinterpret_cast<const ULONGLONG*>(&buffer[8]);
        DWORD sequenceNumber = *reinterpret_cast<const WORD*>(&buffer[16]);
        DWORD hardLinkCount = *reinterpret_cast<const WORD*>(&buffer[18]);
        DWORD firstAttributeOffset = *reinterpret_cast<const WORD*>(&buffer[20]);
        DWORD flags = *reinterpret_cast<const WORD*>(&buffer[22]);
        DWORD usedSize = *reinterpret_cast<const DWORD*>(&buffer[24]);
        DWORD allocatedSize = *reinterpret_cast<const DWORD*>(&buffer[28]);
        ULONGLONG baseRecordReference = *reinterpret_cast<const ULONGLONG*>(&buffer[32]);
        DWORD nextAttributeID = *reinterpret_cast<const WORD*>(&buffer[40]);

        std::cout << "Fixup Offset: " << fixupOffset << std::endl;
        std::cout << "Fixup Entry Count: " << fixupEntryCount << std::endl;
        std::cout << "Log File Sequence Number: " << lsn << std::endl;
        std::cout << "Sequence Number: " << sequenceNumber << std::endl;
        std::cout << "Hard Link Count: " << hardLinkCount << std::endl;
        std::cout << "First Attribute Offset: " << firstAttributeOffset << std::endl;
        std::cout << "Flags: " << flags << std::endl;
        std::cout << "Used Size of MFT Entry: " << usedSize << std::endl;
        std::cout << "Allocated Size of MFT Entry: " << allocatedSize << std::endl;
        std::cout << "Base Record Reference: " << baseRecordReference << std::endl;
        std::cout << "Next Attribute ID: " << nextAttributeID << std::endl;

        // Parse Attributes
        DWORD attributeOffset = firstAttributeOffset;
        while (attributeOffset < usedSize) {
            DWORD attributeType = *reinterpret_cast<const DWORD*>(&buffer[attributeOffset]);
            if (attributeType == 0xFFFFFFFF) {
                break;
            }
            DWORD length = *reinterpret_cast<const DWORD*>(&buffer[attributeOffset + 4]);
            if (attributeType == 0x10) {  // Standard Information attribute
                // Print in color: Standard Information Attribute found
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
                std::cout << "Standard Information Attribute found" << std::endl;
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color

                ULONGLONG creationTime = *reinterpret_cast<const ULONGLONG*>(&buffer[attributeOffset + 24]);
                ULONGLONG modificationTime = *reinterpret_cast<const ULONGLONG*>(&buffer[attributeOffset + 32]);
                ULONGLONG mftChangeTime = *reinterpret_cast<const ULONGLONG*>(&buffer[attributeOffset + 40]);
                ULONGLONG lastAccessTime = *reinterpret_cast<const ULONGLONG*>(&buffer[attributeOffset + 48]);

                std::cout << "Creation Time: " << FileTimeToString(creationTime) << std::endl;
                std::cout << "Modification Time: " << FileTimeToString(modificationTime) << std::endl;
                std::cout << "MFT Change Time: " << FileTimeToString(mftChangeTime) << std::endl;
                std::cout << "Last Access Time: " << FileTimeToString(lastAccessTime) << std::endl;
            }
            if (attributeType == 0x30) {  // File Name attribute
                BYTE nameLength = buffer[attributeOffset + 88];
                std::wstring fileName(reinterpret_cast<const wchar_t*>(&buffer[attributeOffset + 90]), nameLength);
                // Print in color: File Name
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_BLUE);
                std::wcout << L"File Name: " << fileName << std::endl;
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color
            }
            attributeOffset += length;
        }
    }
    else {
        std::cout << "Invalid MFT record signature: " << signature << std::endl;
    }
}

// Function to read and parse the first two MFT records
void ReadAndParseMFT(const std::wstring& volumePath) {
    HANDLE hVolume = CreateFileW(volumePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolume == INVALID_HANDLE_VALUE) {
        PrintError("Failed to open volume");
        return;
    }

    // Read the NTFS boot sector (first sector)
    const DWORD bootSectorSize = 512;
    std::vector<BYTE> bootSector(bootSectorSize);
    if (!ReadSectors(hVolume, 0, 1, bootSectorSize, bootSector)) {
        CloseHandle(hVolume);
        return;
    }

    // Extract sector size and MFT start LCN from the boot sector
    DWORD sectorSize = *reinterpret_cast<const WORD*>(&bootSector[0x0B]);
    ULONGLONG mftStartCluster = *reinterpret_cast<const ULONGLONG*>(&bootSector[0x30]);
    DWORD clusterSize = sectorSize * 8; // NTFS uses 8 sectors per cluster
    ULONGLONG mftStartLcn = mftStartCluster * 8; // Calculate the LCN for the MFT start

    // Print in color: Sector size and MFT start LCN
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_BLUE);
    std::cout << "Sector size: " << sectorSize << std::endl;
    std::cout << "MFT starts at LCN: " << mftStartLcn << std::endl;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color

    // Read the first few sectors of the MFT
    const DWORD recordSize = 1024;
    const DWORD sectorsPerRecord = recordSize / sectorSize;
    const DWORD numRecordsToRead = 50; // Read first two records
    std::vector<BYTE> buffer(sectorSize * sectorsPerRecord * numRecordsToRead);

    if (ReadSectors(hVolume, mftStartLcn, sectorsPerRecord * numRecordsToRead, sectorSize, buffer)) {
        for (int i = 0; i < numRecordsToRead; ++i) {
            std::vector<BYTE> recordBuffer(buffer.begin() + i * recordSize, buffer.begin() + (i + 1) * recordSize);
            std::cout << "Dumping record " << i << ": ";
            for (int j = 0; j < 4; ++j) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(recordBuffer[j]) << " ";
            }
            std::cout << std::dec << std::endl;
            ParseAndPrintMFTRecord(recordBuffer);
        }
    }
    else {
        std::cerr << "Failed to read MFT starting sectors" << std::endl;
    }

    CloseHandle(hVolume);
}

int main() {
    std::wstring volumePath = L"\\\\.\\C:";
    ReadAndParseMFT(volumePath);
    return 0;
}
