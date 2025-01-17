#include <strings.h>
#include "Remapper.hpp"
#include "json.h"
#include "printf.h"

static Result PMDBG_GetCurrentAppInfo(FS_ProgramInfo *outProgramInfo, u32 *outPid, u32 *outLaunchFlags)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x100, 0, 0);
    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;

    memcpy(outProgramInfo, cmdbuf + 2, sizeof(FS_ProgramInfo));
    *outPid = cmdbuf[6];
    *outLaunchFlags = cmdbuf[7];
    return cmdbuf[1];
}

// shamelessly stolen from newlib
char *strtok_r(char *s, const char *delim, char **lasts)
{
    char *spanp;
    int c, sc;
    char *tok;


    if (s == NULL && (s = *lasts) == NULL)
        return NULL;

    /*
    * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
    */
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
        if (c == sc) {
            if (true) {
                goto cont;
            }
            else {
                *lasts = s;
                s[-1] = 0;
                return (s - 1);
            }
        }   
    }

    if (c == 0) {       /* no non-delimiter characters */
        *lasts = NULL;
        return (NULL);
    }
    tok = s - 1;

    /*
     * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too.
     */
    for (;;) {
        c = *s++;
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *lasts = s;
                return (tok);
            }
        } while (sc != 0);
    }
    /* NOTREACHED */
}

static uint32_t keystrtokeyval(char *str)
{
    uint32_t val = 0;
    static const key_s keys[] = {
        { "A",      KEY_A},
        { "B",      KEY_B},
        { "SELECT", KEY_SELECT},
        { "START" , KEY_START},
        { "RIGHT",  KEY_DRIGHT},
        { "LEFT",   KEY_DLEFT},
        { "UP",     KEY_DUP},
        { "DOWN",   KEY_DDOWN},
        { "R",      KEY_R},
        { "L",      KEY_L},
        { "X",      KEY_X},
        { "Y",      KEY_Y},
        { "ZL",     KEY_ZL},
        { "ZR",     KEY_ZR},
        { "CRIGHT", KEY_CPAD_RIGHT},
        { "CLEFT",  KEY_CPAD_LEFT},
        { "CDOWN",  KEY_CPAD_DOWN},
        { "CUP",    KEY_CPAD_UP},
        { "CSRIGHT", KEY_CSTICK_RIGHT},
        { "CSLEFT",  KEY_CSTICK_LEFT},
        { "CSDOWN",  KEY_CSTICK_DOWN},
        { "CSUP",    KEY_CSTICK_UP}
    };
    char *key; char *rest = nullptr;
    key = strtok_r(str, "+", &rest);

    while(key != NULL)
    {
        for (int i = 0; i <  (int)(sizeof(keys) / sizeof(keys[0])); i++)
        {
            if(strcasecmp(keys[i].key, key) == 0)
                val |= keys[i].val;
        }
        key = strtok_r(NULL, "+", &rest);
    }
    return val;
}

static void hexItoa(u64 number, char *out, u32 digits, bool uppercase)
{
    const char hexDigits[] = "0123456789ABCDEF";
    const char hexDigitsLowercase[] = "0123456789abcdef";
    u32 i = 0;

    while(number > 0)
    {
        out[digits - 1 - i++] = uppercase ? hexDigits[number & 0xF] : hexDigitsLowercase[number & 0xF];
        number >>= 4;
    }

    while(i < digits) out[digits - 1 - i++] = '0';
}

static json_object_entry *getremapkey(json_value *value, char *name)
{
    // Only perform key lookup if the object has 2 keys to avoid slowdowns from invalid json
    if (value->type == json_type::json_object && value->u.object.length == 2) {
        for (int i = 0; i < value->u.object.length; i++) {
            if (strcasecmp(value->u.object.values[i].name, name)) {
                return &value->u.object.values[i];
            }
        }
    }
    return nullptr;
}

void Remapper::GenerateFileLocation()
{
    pmDbgInit();
    FS_ProgramInfo programinfo;
    u32 launchflags;
    u32 pid;
    char stid[16+1];
    Result res = PMDBG_GetCurrentAppInfo(&programinfo, &pid, &launchflags);
    if (R_FAILED(res)) programinfo.programId = 0;
    pmDbgExit();
    hexItoa(programinfo.programId, stid, 16, true);
    stid[16] = 0;
    memset(m_fileloc, 0, 40);
    strcpy(m_fileloc, "/rehid/");
    strcat(m_fileloc, stid);
    strcat(m_fileloc, "/rehid.json");
}

extern char data[0x100];

uint32_t Remapper::Remap(uint32_t hidstate)
{
    uint32_t newstate = hidstate;

    for(int i = 0; i < m_keyentries; i++)
    {
        if((hidstate & m_remapkeyobjects[i].oldkey) == m_remapkeyobjects[i].oldkey)
        {
            newstate = newstate ^ m_remapkeyobjects[i].newkey;
            newstate = newstate ^ m_remapkeyobjects[i].oldkey;
        }
    }

    m_touchoveridex = 0;
    m_touchoveridey = 0;

    for(int i = 0; i < m_touchentries; i++)
    {
        if((hidstate & m_remaptouchobjects[i].key) == m_remaptouchobjects[i].key)
        {
            newstate &= ~m_remaptouchobjects[i].key;
            m_touchoveridex = m_remaptouchobjects[i].x;
            m_touchoveridey = m_remaptouchobjects[i].y;
        }
    }

    m_cpadoveridex = -1;
    m_cpadoveridey = -1;

    for(int i = 0; i < m_cpadentries; i++)
    {
        if((hidstate & m_remapcpadobjects[i].key) == m_remapcpadobjects[i].key)
        {
            newstate &= ~m_remapcpadobjects[i].key;
            m_cpadoveridex = m_remapcpadobjects[i].x;
            m_cpadoveridey = m_remapcpadobjects[i].y;
        }
    }

    return newstate;
}

Result Remapper::ReadConfigFile()
{
    Handle fshandle;
   // char globalfileloc[] = "/rehid.json";
    Result ret = FSUSER_OpenFileDirectly(&fshandle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL), fsMakePath(PATH_ASCII, m_fileloc), FS_OPEN_READ, 0);
    if(ret) // Bindings for title not found, check if global profile is present
    {
        ret =  FSUSER_OpenFileDirectly(&fshandle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL), fsMakePath(PATH_ASCII, "/rehid/rehid.json"), FS_OPEN_READ, 0);
        if(ret) return -1; // global profile not found, do not appply any settings.
    }

    ret = FSFILE_GetSize(fshandle, &m_filedatasize);
    m_filedata = (char*)malloc(m_filedatasize + 1);
    if(m_filedata == nullptr) return -2;
    memset(m_filedata, 0, m_filedatasize);
    ret = FSFILE_Read(fshandle, NULL, 0, m_filedata, m_filedatasize);
    if(ret) return ret;
    ret = FSFILE_Close(fshandle);
    if(ret) return ret;
    return 0;
}

void Remapper::ParseConfigFile()
{
    json_value *value = json_parse(m_filedata, m_filedatasize);
    if(value == nullptr) *(u32*)0xF00FFAAB = m_filedata[m_filedatasize - 2];
    m_touchentries = 0;
    m_keyentries = 0;
    for(int index = 0; index < (int)value->u.object.length; index++ )
    {
        if(strcasecmp(value->u.object.values[index].name, "keys") == 0)
        {
            int length = value->u.object.values[index].value->u.array.length;
            json_value *arr = value->u.object.values[index].value;
            m_keyentries = length;
            for(int i = 0; i < length; i++)
            {
                // Process key objects
                json_object_entry * getkey = getremapkey(arr->u.array.values[i], "get");
                json_object_entry * presskey = getremapkey(arr->u.array.values[i], "press");
                if (getkey != nullptr && presskey != nullptr) {
                    // pointers are not null, matches found
                    m_remapkeyobjects[i].newkey = keystrtokeyval(getkey->value->u.string.ptr);
                    m_remapkeyobjects[i].oldkey = keystrtokeyval(presskey->value->u.string.ptr);
                }
            }
        }
        else if(strcasecmp(value->u.object.values[index].name, "touch") == 0)
        {
            int length = value->u.object.values[index].value->u.array.length; // size of touch entries
            json_value *arr = value->u.object.values[index].value; // touch
            m_touchentries = length;
            for(int i = 0; i < length; i++)
            {
                json_object_entry * getkey = getremapkey(arr->u.array.values[i], "get");
                json_object_entry * presskey = getremapkey(arr->u.array.values[i], "press");
                if (getkey != nullptr && presskey != nullptr) {
                    // pointers are not null, matches found
                    m_remaptouchobjects[i].x = getkey->value->u.array.values[0]->u.integer;
                    m_remaptouchobjects[i].y = getkey->value->u.array.values[1]->u.integer;
                    m_remaptouchobjects[i].key = keystrtokeyval(presskey->value->u.string.ptr);
                }
                
            }
        }
        
        else if(strcasecmp(value->u.object.values[index].name, "cpad") == 0)
        {
            int length = value->u.object.values[index].value->u.array.length; // size of cpad entries
            json_value *arr = value->u.object.values[index].value; // cpad
            m_cpadentries = length;
            for(int i = 0; i < length; i++)
            {
                json_object_entry * getkey = getremapkey(arr->u.array.values[i], "get");
                json_object_entry * presskey = getremapkey(arr->u.array.values[i], "press");
                if (getkey != nullptr && presskey != nullptr) {
                    // pointers are not null, matches found
                    m_remapcpadobjects[i].x = getkey->value->u.array.values[0]->u.integer;
                    m_remapcpadobjects[i].y = getkey->value->u.array.values[1]->u.integer;
                    m_remapcpadobjects[i].key = keystrtokeyval(presskey->value->u.string.ptr);
                }
            }
        }
        
        else if(strcasecmp(value->u.object.values[index].name, "cpadtodpad") == 0)
        {
            json_value *cpadtodpad = value->u.object.values[index].value; // CPAD-TO-DPAD
            m_docpadtodpad = cpadtodpad->u.boolean & 0xFF;
        }
        else if(strcasecmp(value->u.object.values[index].name, "dpadtocpad") == 0)
        {
            json_value *dpadtocpad = value->u.object.values[index].value; // DPAD-TO-CPAD
            m_dodpadtocpad = dpadtocpad->u.boolean & 0xFF;
        }
        else if(strcasecmp(value->u.object.values[index].name, "overridecpadpro") == 0)
        {
            json_value *overridecppro = value->u.object.values[index].value; //  OVERRIDE CPAD PRO
            overridecpadpro = overridecppro->u.boolean & 0xFF;
        }
    }
    json_value_free(value);
    free(m_filedata);
}