/*
 * ZpaqFxaCtrl.h
 *
 *  Created on: Apr 22, 2010
 *      Author: indigo
 */

#ifndef ZPAQ_FXA_FPGA_CTRL_H_
#define ZPAQ_FXA_FPGA_CTRL_H_

#include <thread>
#include "libzpaq.h"

// 1, 2, 4, 8 byte unsigned integers
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;

#define SUCCESS 1
#define FAILURE 0
#define BYTE_NUM_PER_BLOCK 4096
#define COMPRESSCORE_NUM 2

//reg0:start
//reg1:rst
//reg2:LastType(Unused)
//reg3:FileFinalBlockCountFlag
//reg4:FinalBlockCounter(only read)(Unused)
//reg5:FinalBlockNumber
//reg6:EncLow(only read)
//reg7:EncMid(only read)
//reg8:EncHigh(only read)
//reg9:FileFinish(only read)
struct CompressControlRegs
{
    U32 CompressStart;              //0
    U32 CompressReset;
    U32 LastType;
    U32 FileFinalBlockCountFlag;
    U32 FinalBlockCounter;       //Only Read
    U32 FinalBlockNumber;
    U32 EncLow;
    U32 EncMid;
    U32 EncHigh;
    U32 FileFinish;
    U32 Reserve4;
    U32 Reserve5;
    U32 Reserve6;
    U32 Reserve7;
    U32 Reserve8;
    U32 Reserve9;                   //15
    U64 padding[504];
};   

struct CompressConrrol
{
    CompressControlRegs Control[COMPRESSCORE_NUM];
};

class ZpaqFxaFpgaCtrl{
private:
    const U32 mapSize = 4UL * 1024 * COMPRESSCORE_NUM;
    const U32 mapMask = mapSize - 1UL;
    int regFd, inputFd0, outputFd0, inputFd1, outputFd1;

    volatile CompressConrrol *Controls;
public:
    ZpaqFxaFpgaCtrl(
	    const char *fRegs,
	    const char *fInH2C0,
	    const char *fOutC2H0,
	    const char *fInH2C1,
	    const char *fOutC2H1
    );
    virtual ~ZpaqFxaFpgaCtrl();
    U8 inputBuf0[BYTE_NUM_PER_BLOCK];
    U8 inputBuf1[BYTE_NUM_PER_BLOCK];
    U8 *outputBuf0, *outputBuf1;
    U8 FBMark;
    int startCompress(int channel);
    int resetCompressCore(int channel);
    int restartCompress(int channel);
    int GetOutput(int channel);
    int SetInput(int channel, const void * buf, size_t n);
    int isFileFinish(int channel);
    void readEncData(libzpaq::Compressor *co,int channel);
    int fpgaCompress(libzpaq::Compressor *co, U64 FileSize,int channel);             //substitution for co.compress     //this is repalced by thread function, unused
    int markLastBlock(U32 ByteNumber,int channel);
    int clearLastBlockMark(int channel);
};

//thread argument
struct JobRecv{
  ZpaqFxaFpgaCtrl *Ctrl;
  libzpaq::Compressor *co;
  int channel;
};

//thread argument
struct JobTrans{
  ZpaqFxaFpgaCtrl *Ctrl;
  libzpaq::Compressor *co;
  int channel;
  U64 FileSize;
};

void compressOutRecvThread(ZpaqFxaFpgaCtrl *ctrl, libzpaq::Compressor *co);
void *compressOutRecvThread(void *arg);
void *compressInTransThread(void *arg);
#endif