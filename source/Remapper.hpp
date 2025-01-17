#pragma once
#include <3ds.h>
#include "irrst.hpp"

struct KeyObject
{
    uint32_t oldkey; // Old Key Combo
    uint32_t newkey; // New Key Combo
};

struct KeyAndCoordObject
{
    uint32_t key; 
    uint16_t x;
    uint16_t y; 
};

struct key_s
{
    char key[10];
    uint32_t val;
};

class Remapper
{
    public:
        void GenerateFileLocation();
        uint32_t Remap(uint32_t hidtstate);
        Result ReadConfigFile();
        void ParseConfigFile();
        void Reset()
        {
            m_keyentries = 0;
            m_touchentries = 0;
            m_cpadentries = 0;
            m_dodpadtocpad = 0;
            m_docpadtodpad = 0;
            overridecpadpro = 0;
        }
        KeyObject m_remapkeyobjects[10]; // Support upto 10 remapable key combos
        KeyAndCoordObject m_remaptouchobjects[10]; // Support upto 10 key > touch binds
        KeyAndCoordObject m_remapcpadobjects[10]; // support upto 10 key > cpad binds
        uint8_t m_keyentries;
        uint8_t m_touchentries;
        uint8_t m_cpadentries;
        uint8_t m_docpadtodpad = 0;
        uint8_t m_dodpadtocpad = 0;
        int16_t m_touchoveridex = 0;
        int16_t m_touchoveridey = 0;
        int16_t m_cpadoveridex = -1;
        int16_t m_cpadoveridey = -1;

    private:
        char *m_filedata;
        uint64_t m_filedatasize;
        char m_fileloc[40];
};