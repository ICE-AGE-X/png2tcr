//
//  main.cpp
//  png2tcr
//
//  Created by 冰河世纪 on 2019/10/6.
//  Copyright © 2019年 冰河世纪. All rights reserved.
//

#include <iostream>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <string>

#define BLOCK_SIZE 8*8
#define BLOCK_WIDTH 8
using namespace std;

struct TcrHeader{
    int magic=0x30524354;
    int magic2=0x64;
    int unknown=1;
    short width;
    short height;
    short unknown1=0xffff;
    short unknown2=0x3;
    int imgDataOffset=0x80;
    int zero[26]={0};
};

const unsigned short tileOrder[BLOCK_SIZE]={
    0, 1, 4, 5, 16, 17, 20, 21,
    2, 3, 6, 7, 18, 19, 22, 23,
    8, 9, 12, 13, 24, 25, 28, 29,
    10, 11, 14, 15, 26, 27, 30, 31,
    32, 33, 36, 37, 48, 49, 52, 53,
    34, 35, 38, 39, 50, 51, 54, 55,
    40, 41, 44, 45, 56, 57, 60, 61,
    42, 43, 46, 47, 58, 59, 62, 63,
};
const unsigned short linOrder[BLOCK_SIZE]={
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63,
};

unsigned int conv4to8(const unsigned int &value)
{
    return value<<4 | value;
}

unsigned short conv8to4(const unsigned short &value)
{
    return value/0x11;
}

unsigned short conv8to16(const unsigned short &a,const unsigned short &b)
{
    return a | b<<8;
}
//将32位数据转16位
unsigned short convRGBA8888toRGBA4444(const unsigned char *offset)
{
    unsigned short r=offset[0];
    unsigned short g=offset[1];
    unsigned short b=offset[2];
    unsigned short a=offset[3];
    unsigned short ret=(conv8to4(r)<<12)+(conv8to4(g)<<8)+(conv8to4(b)<<4)+(conv8to4(a));
    return ret;
}

unsigned int convRGBA4444toRGBA8888(const unsigned int &value)
{
    
//    return (conv4to8(value>>12)<<24)+(conv4to8((value>>8)&0xf)<<16)+(conv4to8((value>>4)&0xf)<<8)+conv4to8(value&0xf);
    unsigned int r=conv4to8(value>>12);
    unsigned int g=conv4to8((value>>8)&0xf)<<8;
    unsigned int b=conv4to8((value>>4)&0xf)<<16;
    unsigned int a=conv4to8(value&0xf)<<24;
    
    return r|g|b|a;
}

unsigned int convCharToInt(const unsigned char *offset)
{
    return (offset[0]<<24)+(offset[1]<<16)+(offset[2]<<8)+offset[3];
}

void getBlockData(const unsigned short *data,unsigned short * out, int x, int y ,const int &width,const int &height)
{
    for (int startY=y; startY<y+8; startY++) {
        for (int startX=x; startX<x+8; startX++) {
            int offset=startX+startY*width;
            *out= data[offset];
            out++;
        }
    }
}

void blockToTile(const unsigned short *block,unsigned short *tile)
{
    for (int i=0; i<BLOCK_SIZE; i++) {
        int idx=tileOrder[i];
        tile[idx]=block[i];
    }
}

void tileToBlock(const unsigned int *tile,unsigned int *block)
{
    for (int i=0; i<BLOCK_SIZE; i++) {
        int idx=tileOrder[i];
        block[i]=tile[idx];
    }
}



bool tcr2png(const string &inPath,const string &outPath)
{
    ifstream infile(inPath);
    if(!infile.is_open())
        return false;
    infile.seekg(0xc);
    short width,height;
    infile.read((char*)&width, sizeof(width));
    infile.read((char*)&height,sizeof(height));
    infile.seekg(0x80);
    
    unsigned short data[width*height];
    infile.read((char*)data, sizeof(data));
    unsigned int tileData[width*height];
    
    for (int i=0;i<width*height; i++) {
        tileData[i]=convRGBA4444toRGBA8888(data[i]);
//        printf("%d",i>>1);
    }
    
    
    unsigned int linData[width*height];
    unsigned int tileAry[BLOCK_SIZE];
    unsigned int blockAry[BLOCK_SIZE];
    int x=0,y=0;
    for (int i=0; i<width*height; i+=BLOCK_SIZE) {
        memcpy(tileAry, &tileData[i], sizeof(tileAry));
        tileToBlock(tileAry, blockAry);
        for (int sy=0; sy<BLOCK_WIDTH; sy++) {
            for (int sx=0; sx<BLOCK_WIDTH; sx++) {
                linData[sx+x+(sy+y)*width]=blockAry[sx+sy*BLOCK_WIDTH];
            }
        }
        x+=BLOCK_WIDTH;
        if(x>=width/BLOCK_SIZE)
        {
            x=0;
            y+=BLOCK_WIDTH;
        }
        
    }
    infile.close();
    
//    ofstream os(outPath);
//    if(!os.is_open())
//        return false;
    stbi_flip_vertically_on_write(true);
    stbi_write_png(outPath.c_str(), width, height, 4, (const void*)&linData, 0);
//    os.write((const char *)&linData, sizeof(linData));
//    os.close();
    return true;
}


bool png2tcr(string inPath,string outPath)
{
    int width,height,nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data=stbi_load(inPath.c_str(), &width, &height, &nrChannels, 0);
    
    if(!data)
        return false;
    
    unsigned short linData[width*height];
    int pos=0;
    for (int i=0; i<width*height; i++) {
        linData[i]=convRGBA8888toRGBA4444(&data[pos]);
        pos+=4;
    }
    stbi_image_free(data);
    
    unsigned short tileData[width*height];
    //一个小块
    pos=0;
    unsigned short blockAry[BLOCK_SIZE];
    unsigned short tileAry[BLOCK_SIZE];
    for(int blockY=0;blockY<height;blockY+=BLOCK_WIDTH)
    {
        for (int blockX=0; blockX<width; blockX+=BLOCK_WIDTH) {
            getBlockData(linData,blockAry,blockX,blockY,width,height);
            blockToTile(blockAry,tileAry);
            for(int i=0;i<BLOCK_SIZE;i++)
            {
                tileData[pos]= tileAry[i];
                pos++;
            }
        }
    }
    
    ofstream os(outPath);
    if(!os.is_open())
        return false;
    TcrHeader header;
    header.width=width;
    header.height=height;
    os.write((const char*)&header, sizeof(header));
    os.flush();
    os.write((const char *)&tileData, sizeof(tileData));
    os.close();
    return true;
}

int main(int argc, const char * argv[]) {
//    unsigned short a=0xf;
//    unsigned short b=0x1;
//    unsigned short r= conv8to16(&a, &b);
    if(argc !=4)
    {
        printf("useage:png2tcr -op *.tcr *.png\n");
        printf("       png2tcr -ot *.png *.tcr\n");
        return 0;
    }
    string arg1(argv[1]),arg2(argv[2]),arg3(argv[3]);
    if(arg1=="-op")
    {
        tcr2png(arg2, arg3);
    }else if( arg1=="-ot")
    {
        png2tcr(arg2,arg3);
    }
    
    return 0;
}
