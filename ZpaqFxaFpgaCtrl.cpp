/*
 * ZpaqFxaCtrl.h
 *
 *  Created on: Apr 22, 2010
 *      Author: indigo
 */
#include "ZpaqFxaFpgaCtrl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <stdlib.h>

using namespace std;
char PostprocessBuf = 0;
void *compressInTransThread(void *arg)
{
    JobTrans &job = *(JobTrans *)arg;
    ZpaqFxaFpgaCtrl *ctrl = job.Ctrl; 
    libzpaq::Compressor *co = job.co;
    int channel = job.channel;
    int ch=0;
    int i = 0;
    U64 InputCounter = job.FileSize;
    std::cout << "Channel" << channel << " Input size:" << InputCounter << "B,Start FPGA Compress.\n";
    int rc = ctrl->SetInput(channel, &PostprocessBuf, 1);         //PostProgress
    if(rc != 1)
    {
        throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
    }
    while((ch=co->in->get())>=0 && InputCounter > 0){
        rc = ctrl->SetInput(channel, &ch, 1);
        if(rc != 1)
        {
            throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
        }
        InputCounter--;
        if(InputCounter == BYTE_NUM_PER_BLOCK){
            // while((ch=co->in->get())>=0){            //this can not be used when file split to two part
            for(i = 0; i < BYTE_NUM_PER_BLOCK; i++){
                ch=co->in->get();
                if(ch >= 0){
                    if(channel)ctrl->inputBuf1[i] = (U8)ch;
                    else ctrl->inputBuf0[i] = (U8)ch;
                }
                else 
                {
                    throw "Read file failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
                }
                // channel? ctrl->inputBuf1[i]:ctrl->inputBuf0[i]=(U8)ch;
                // i++;
            }
            ctrl->markLastBlock(BYTE_NUM_PER_BLOCK, channel);
            rc = ctrl->SetInput(channel, channel? ctrl->inputBuf1:ctrl->inputBuf0, i);
            if(rc != i)
            {
                throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
            }
            InputCounter-=rc;
        }
    }
    // return ch>=0;
}
void *compressOutRecvThread(void *arg){
    JobRecv &job = *(JobRecv *)arg;
    ZpaqFxaFpgaCtrl *ctrl = job.Ctrl; 
    libzpaq::Compressor *co = job.co;
    int channel = job.channel;

    S64 total = 0;
    int fl = 0;
    int to = 0;
    while(true){
        fl = ctrl->GetOutput(channel);
        if(fl == 0)
        {
            usleep(10);
            if(60000 == ++to)
            {
                break;
            }
        }
        else
        {
            to = 0;
            if(ctrl->isFileFinish(channel))
            {
                total += fl-1;
                for(int i = 0; i < fl-1; i++)
                {
                    co->enc.out->put(channel? ctrl->outputBuf1[i]:ctrl->outputBuf0[i]);
                }
                break;
            }
            else
            {
                total += fl; 
                for(int i = 0; i < fl; i++)
                {
                    co->enc.out->put(channel? ctrl->outputBuf1[i]:ctrl->outputBuf0[i]);
                }
            }
        }
        
    }
    std::cout << "FPGA channel"<< channel <<" Compressed received:" << total << "B, finished." << endl;

}
ZpaqFxaFpgaCtrl::ZpaqFxaFpgaCtrl(
	const char *fRegs,
	const char *fInH2C0,
	const char *fOutC2H0,
	const char *fInH2C1,
	const char *fOutC2H1)
{
    regFd = open(fRegs, O_RDWR | O_SYNC);
    inputFd0 = open(fInH2C0, O_RDWR);
    outputFd0 = open(fOutC2H0, O_RDWR | O_NONBLOCK);
    inputFd1 = open(fInH2C1, O_RDWR);
    outputFd1 = open(fOutC2H1, O_RDWR | O_NONBLOCK);

    FBMark = 0;
    int test = sizeof(CompressConrrol);
    int test0 = sizeof(CompressControlRegs);
    U8 *regBase = (U8 *)mmap(0, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, regFd, 0);

    Controls = (CompressConrrol *)(regBase);

    if(regFd < 0 || regBase == NULL || inputFd0 < 0 || outputFd0 < 0 || inputFd1 < 0 || outputFd1 < 0)
    {
        if(regFd >= 0) close(regFd);
        if(inputFd0 >= 0) close(inputFd0);
        if(outputFd0 >= 0) close(outputFd0);
        if(inputFd1 >= 0) close(inputFd1);
        if(outputFd1 >= 0) close(outputFd1);
        throw "files open failed in constructor of ZpaqFxaFpgaCtrl.\n";
    }

    posix_memalign((void **)&outputBuf0, 4096, BYTE_NUM_PER_BLOCK * 2);
    if(outputBuf0 == NULL)
    {
        throw "output buf allocation failed in constructor of ZpaqFxaFpgaCtrl.\n";
    }

    posix_memalign((void **)&outputBuf1, 4096, BYTE_NUM_PER_BLOCK * 2);
    if(outputBuf1 == NULL)
    {
        throw "output buf allocation failed in constructor of ZpaqFxaFpgaCtrl.\n";
    }
}

ZpaqFxaFpgaCtrl::~ZpaqFxaFpgaCtrl()
{
    if(regFd >= 0) close(regFd);
    if(inputFd0 >= 0) close(inputFd0);
    if(outputFd0 >= 0) close(outputFd0);
    if(inputFd1 >= 0) close(inputFd1);
    if(outputFd1 >= 0) close(outputFd1);
    if(outputBuf0 != NULL) free(outputBuf0);
    if(outputBuf1 != NULL) free(outputBuf1);
}

int ZpaqFxaFpgaCtrl::markLastBlock(U32 ByteNumber, int channel)
{
    this->Controls->Control[channel].FinalBlockNumber = ByteNumber;
    this->Controls->Control[channel].FileFinalBlockCountFlag = 1;
    this->FBMark = 1;
    return SUCCESS;
}

int ZpaqFxaFpgaCtrl::clearLastBlockMark(int channel)
{
    this->Controls->Control[channel].FinalBlockNumber = 0;
    this->Controls->Control[channel].FileFinalBlockCountFlag = 0;
    this->FBMark = 0;
    return SUCCESS;
}

int ZpaqFxaFpgaCtrl::startCompress(int channel)
{
    this->Controls->Control[channel].CompressStart = 1;
    this->Controls->Control[channel].CompressStart = 0;
    return SUCCESS;
}

int ZpaqFxaFpgaCtrl::resetCompressCore(int channel)
{
    clearLastBlockMark(channel);
    this->Controls->Control[channel].CompressReset = 1;
    this->Controls->Control[channel].CompressReset = 0;
    this->Controls->Control[channel].CompressStart = 0;
    return SUCCESS;
}

int ZpaqFxaFpgaCtrl::restartCompress(int channel)
{
    clearLastBlockMark(channel);
    this->Controls->Control[channel].CompressReset = 1;
    this->Controls->Control[channel].CompressReset = 0;
    this->Controls->Control[channel].CompressStart = 0;
    usleep(10);
    startCompress(channel);
    return SUCCESS;
}

int ZpaqFxaFpgaCtrl::GetOutput(int channel)
{
    S32 rc = read(channel? this->outputFd1:this->outputFd0, channel? outputBuf1:outputBuf0, 2 * BYTE_NUM_PER_BLOCK/* * num*/);
    return rc;
}

int ZpaqFxaFpgaCtrl::SetInput(int channel, const void * buf, size_t n)
{
    int rc = write(channel? this->inputFd1:this->inputFd0, buf, n);
    return rc;
}

void ZpaqFxaFpgaCtrl::readEncData(libzpaq::Compressor *co ,int channel)
{
#ifdef _FPGA_COMPRESS_
    co->enc.high = this->Controls->Control[channel].EncHigh;
    co->enc.low = this->Controls->Control[channel].EncLow;
#endif
}

int ZpaqFxaFpgaCtrl::isFileFinish(int channel)
{
    return this->Controls->Control[channel].FileFinish;
}

int ZpaqFxaFpgaCtrl::fpgaCompress(libzpaq::Compressor *co, U64 FileSize, int channel)
{
    int ch=0;
    int i = 0;
    U64 InputCounter = FileSize;
    std::cout << "Input size:" << InputCounter << "B,Start FPGA Compress.\n";
    int rc = write(channel? this->outputFd1:this->outputFd0, &PostprocessBuf, 1);         //PostProgress
    if(rc != 1)
    {
        throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
    }
    while((ch=co->in->get())>=0 && InputCounter > 0){
        rc = write(channel? this->outputFd1:this->outputFd0, &ch, 1);
        if(rc != 1)
        {
            throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
        }
        InputCounter--;
        if(InputCounter == BYTE_NUM_PER_BLOCK){
            while((ch=co->in->get())>=0){
                inputBuf0[i]=(U8)ch;
                i++;
            }
            markLastBlock(BYTE_NUM_PER_BLOCK, channel);
            rc = write(channel? this->outputFd1:this->outputFd0, &inputBuf0, i);
            if(rc != i)
            {
                throw "write H2C stream failed in ZpaqFxaFpgaCtrl::fpgaCompress.\n";
            }
            InputCounter-=rc;
        }
    }
    return ch>=0;
}