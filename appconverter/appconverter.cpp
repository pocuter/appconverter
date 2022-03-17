// appconverter.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//
#include "miniz/miniz.h"

#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <inttypes.h>
#include <direct.h>

#define BLOCK_SIZE 1024*40
using namespace std;

struct fheader {
    char magic[5];
    uint8_t fileVersion;
    uint16_t crc16;
    uint64_t appID;
    struct version {
        uint8_t major;
        uint8_t minor;
        uint8_t patch;
    } version;
    struct features {
        char hasSignartur : 1;
        char hasMetaData  : 1;
        char forFutureUse : 6;
    } features;
    uint32_t startSignatur;
    uint32_t sizeSignatur;
    uint32_t startMetaFile;
    uint32_t sizeMetaFile;
    uint32_t startFlashFile;
    uint32_t sizeFlashFile;

};

vector<string> split(const string& s, char delim) {
    vector<string> result;
    stringstream ss(s);
    string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

void printHelp(const char* appName) {
    printf("%s\n\n", appName);
    printf("   -image   [image filename]\n");
    printf("   -version [X.Y.Z]\n");
    printf("   -meta    [metadata filename]\n");
    printf("   -sign    [private key filename]\n");
    printf("   -id      [APP-ID]\n");
    printf("   -help    \n");
}

void crc16(const unsigned char* data_p, size_t length, uint16_t* crc) {
    unsigned char x;
    //unsigned short crc = 0xFFFF;

    while (length--) {
        x = *crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        *crc = (*crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
    }
    
}

int generateAppFile(std::string& image, std::string& version, std::string& sign, std::string& meta, uint64_t appId) {
    
    fheader header;
    uint16_t crc = 0xFFFF;
    char outFn[256];
    snprintf(outFn, 256, "apps");
    _mkdir(outFn);
    snprintf(outFn, 256, "apps/%" PRIu64, appId);
    _mkdir(outFn);
    snprintf(outFn, 256, "apps/%" PRIu64 "/esp32c3.app", appId);
    header.magic[0] = 'P'; header.magic[1] = '1'; header.magic[2] = 'A'; header.magic[3] = 'P'; header.magic[4] = 'P';
    header.fileVersion = 0;
    header.features.forFutureUse = 0;

    vector<char> metaData;
    vector<char> flashData;

    //////////// APP Version ////////////////
    vector<string> vers = split(version, '.');
    if (vers.size() != 3) {
        printf("Wrong Version string. For example: 1.2.3");
        return -1;
    }
   
    int a = atoi(vers.at(0).c_str());
    int b = atoi(vers.at(1).c_str());
    int c = atoi(vers.at(2).c_str());
    if (a > 255 || a < 0 || b > 255 || b < 0 || c > 255 || c < 0) {
        printf("Wrong Version string. For example: 1.2.3");
        return -1;
    }
        
    header.version.major = a;
    header.version.minor = b;
    header.version.patch = c;

    header.appID = appId;

    ////////////////////////////
    if (meta.size()) {
        header.features.hasMetaData = true;
    } else {
        header.features.hasMetaData = false;
        header.startMetaFile = 0;
        header.sizeMetaFile = 0;
    }

    if (sign.size()) {
        header.features.hasSignartur = true;
    } else {
        header.startSignatur = 0;
        header.sizeSignatur = 0;
        header.features.hasSignartur = false;
    }


    //////////// Meta file ////////////////
    if (header.features.hasMetaData) {
        FILE* ptr;
        fopen_s(&ptr, meta.c_str(), "rb");
        char ch;
            
        if (!ptr) {
            printf("Did not find meta file!\n");
            return -1;
        }
        do {
            ch = fgetc(ptr);
            if (ch != EOF) metaData.push_back(ch);
        } while (ch != EOF);
        fclose(ptr);
        metaData.push_back('\n');
        metaData.push_back(0);
        metaData.push_back(EOF);

        header.startMetaFile = sizeof(fheader);
        header.sizeMetaFile = (uint32_t)metaData.size();

    }

    //////////// Flash file ////////////////
    {
        struct stat sb;
        if (stat(image.c_str(), &sb) == -1) {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        FILE* ptr;
        fopen_s(&ptr, image.c_str(), "rb");
        if (!ptr) {
            printf("Did not find flash file!\n");
            return -1;
        }
        
        // Todo: Size check!
        
        unsigned char* bufIn = new unsigned char[sb.st_size];
        
        size_t s = fread(bufIn, 1, sb.st_size, ptr);
           
        fclose(ptr);
        if (s != sb.st_size) {
            printf("Reading Image file failed!\n");
            return -1;
        }
            
        size_t bufOutSize = sb.st_size;
        unsigned char* bufOut = new unsigned char[bufOutSize];
        size_t fileOutSize = 0;
        size_t overAllSize = 0;
        while (overAllSize < sb.st_size) {
            uint32_t* blockLength = (uint32_t*)(bufOut + fileOutSize);
            fileOutSize += 4;

            int currSize = BLOCK_SIZE;
            int currOutSize = bufOutSize - fileOutSize;
            if (overAllSize + currSize > sb.st_size) currSize = sb.st_size - overAllSize;
            
            int err = mz_compress2(bufOut + fileOutSize, (mz_ulong*)&currOutSize, bufIn + overAllSize, (mz_ulong)currSize, MZ_BEST_COMPRESSION);
            if (err != MZ_OK){
                printf("Compress error! %d\n", err);
                delete[] bufOut;
                return -1;
            }
            *blockLength = currOutSize;
            fileOutSize += currOutSize;
            overAllSize += currSize;


        }
       
        flashData.assign((char*)bufOut, (char*)(bufOut + fileOutSize));
        delete[] bufOut;

        header.startFlashFile = (uint32_t)(header.startMetaFile + header.sizeMetaFile);
        header.sizeFlashFile = (uint32_t)flashData.size();
    }
     
     
     
     
    ///////////////// Generate outfile ////////////////////////////////
    {
        FILE* ptr;
        fopen_s(&ptr, outFn, "wb");
        if (!ptr) {
            printf("Could not open output file!\n");
            return -1;
        }
        
        fwrite(&header, sizeof(header), 1, ptr);
        crc16((const unsigned char*)(&header) + 8, sizeof(header) - 8, &crc);
        if (header.features.hasMetaData) {
            fwrite(metaData.data(), metaData.size(), 1, ptr);
            crc16((const unsigned char*)metaData.data(), metaData.size(), &crc);
        }
        
        fwrite(flashData.data(), flashData.size(), 1, ptr);
        crc16((const unsigned char*)flashData.data(), flashData.size(), &crc);

        // write crc into header
        fseek(ptr, 6, SEEK_SET);
        fwrite(&crc, 2, 1, ptr);
        fclose(ptr);
        printf("OK, file saved to %s\n", outFn);
    }
    
    

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 0) return -1; // should not happen

    bool showHelp = true;
    std::string image;
    std::string version;
    std::string sign;
    std::string meta;
    
    uint64_t appId = 0;

    for (int i = 1; i < argc; i++) {
        if (argc > i + 1) {
            // here switches with one argument
            if (! strcmp(argv[i], "-image")) {
                image = argv[i + 1];
                i++;
            }
            if (! strcmp(argv[i], "-version")) {
                version = argv[i + 1];
                i++;
            }
            if (! strcmp(argv[i], "-sign")) {
                printf("Signing feature not supported, yet\n");
                return -1;

                sign = argv[i + 1];
                i++;
            }
            if (! strcmp(argv[i], "-meta")) {
                meta = argv[i + 1];
                i++;
            }
            
            if (!strcmp(argv[i], "-id")) {
                appId = std::stoull(argv[i + 1]);
                i++;
            }
            
            
        }
        // here switches with no arguments
        if (! strcmp(argv[i], "-help")) {
            printHelp(argv[0]);
            showHelp = false;
        }
        if (image.size() && version.size() && appId > 0) {
            showHelp = false;
            return generateAppFile(image, version, sign, meta, appId);
        }

    }

    
    if (showHelp)
        printHelp(argv[0]);




}
