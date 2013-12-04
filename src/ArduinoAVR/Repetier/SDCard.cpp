/*
    This file is part of Repetier-Firmware.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

    This firmware is a nearly complete rewrite of the sprinter firmware
    by kliment (https://github.com/kliment/Sprinter)
    which based on Tonokip RepRap firmware rewrite based off of Hydra-mmm firmware.
*/

#include "Repetier.h"
#include "ui.h"

#if SDSUPPORT

#ifndef SD_ALLOW_LONG_NAMES
#define SD_ALLOW_LONG_NAMES false
#endif

char tempLongFilename[LONG_FILENAME_LENGTH+1];

SDCard sd;

SDCard::SDCard()
{
    sdmode = false;
    sdactive = false;
    savetosd = false;
    Printer::setAutomount(false);
    //power to SD reader
#if SDPOWER > -1
    SET_OUTPUT(SDPOWER);
    WRITE(SDPOWER,HIGH);
#endif
#if defined(SDCARDDETECT) && SDCARDDETECT>-1
    SET_INPUT(SDCARDDETECT);
    WRITE(SDCARDDETECT,HIGH);
#endif
}

void SDCard::automount()
{
#if defined(SDCARDDETECT) && SDCARDDETECT>-1
    if(READ(SDCARDDETECT) != SDCARDDETECTINVERTED)
    {
        if(sdactive)   // Card removed
        {
            Com::printFLN(Com::tSDRemoved);
#if UI_DISPLAY_TYPE!=0
            uid.executeAction(UI_ACTION_TOP_MENU);
#endif
            unmount();
            UI_STATUS(UI_TEXT_SD_REMOVED);
        }
    }
    else
    {
        if(!sdactive)
        {
            UI_STATUS(UI_TEXT_SD_INSERTED);
            Com::printFLN(Com::tSDInserted);
            Printer::setMenuMode(MENU_MODE_SD_MOUNTED,true);
            initsd();
#if UI_DISPLAY_TYPE!=0
            if(sdactive) {
                Printer::setAutomount(true);
                uid.executeAction(UI_ACTION_SD_PRINT+UI_ACTION_TOPMENU);
            }
#endif
        }
    }
#endif
}

void SDCard::initsd()
{
    sdactive = false;
#if SDSS >- 1
    /*if(dir[0].isOpen())
        dir[0].close();*/
    if(!fat.begin(SDSS,SPI_FULL_SPEED))
    {
        Com::printFLN(Com::tSDInitFail);
        return;
    }
    sdactive = true;
    Printer::setMenuMode(MENU_MODE_SD_MOUNTED,true);
    if(!selectFile("init.g",true))
    {
        startPrint();
    }
#endif
}

void SDCard::mount()
{
    sdmode = false;
    initsd();
}

void SDCard::unmount()
{
    sdmode = false;
    sdactive = false;
    savetosd = false;
    Printer::setAutomount(false);
    Printer::setMenuMode(MENU_MODE_SD_MOUNTED+MENU_MODE_SD_PAUSED+MENU_MODE_SD_PRINTING,false);
#if UI_DISPLAY_TYPE!=0
    uid.cwd[0]='/';
    uid.cwd[1]=0;
    uid.folderLevel=0;
#endif
}

void SDCard::startPrint()
{
    if(!sdactive) return;
    sdmode = true;
    Printer::setMenuMode(MENU_MODE_SD_PRINTING,true);
    Printer::setMenuMode(MENU_MODE_SD_PAUSED,false);
}
void SDCard::pausePrint(bool intern)
{
    if(!sd.sdactive) return;
    sdmode = false;
    Printer::setMenuMode(MENU_MODE_SD_PAUSED,true);
    if(intern) {
        Printer::MemoryPosition();
        Printer::moveToReal(Printer::xMin,Printer::yMin+Printer::yLength,Printer::currentPosition[Z_AXIS],Printer::currentPosition[E_AXIS],Printer::maxFeedrate[X_AXIS]);
    }
}
void SDCard::continuePrint(bool intern)
{
    if(!sd.sdactive) return;
    Printer::setMenuMode(MENU_MODE_SD_PAUSED,false);
    if(intern) {
        Printer::GoToMemoryPosition(true,true,false,true,Printer::maxFeedrate[X_AXIS]);
    }
}
void SDCard::stopPrint()
{
    if(!sd.sdactive) return;
    Printer::setMenuMode(MENU_MODE_SD_PRINTING,false);
    Printer::setMenuMode(MENU_MODE_SD_PAUSED,false);
    Com::printFLN(PSTR("SD print stopped by user."));
}

void SDCard::writeCommand(GCode *code)
{
    unsigned int sum1=0,sum2=0; // for fletcher-16 checksum
    uint8_t buf[100];
    uint8_t p=2;
    file.writeError = false;
    int params = 128 | (code->params & ~1);
    *(int*)buf = params;
    if(code->isV2())   // Read G,M as 16 bit value
    {
        *(int*)&buf[p] = code->params2;
        p+=2;
        if(code->hasString())
            buf[p++] = strlen(code->text);
        if(code->hasM())
        {
            *(int*)&buf[p] = code->M;
            p+=2;
        }
        if(code->hasG())
        {
            *(int*)&buf[p]= code->G;
            p+=2;
        }
    }
    else
    {
        if(code->hasM())
        {
            buf[p++] = (uint8_t)code->M;
        }
        if(code->hasG())
        {
            buf[p++] = (uint8_t)code->G;
        }
    }
    if(code->hasX())
    {
        *(float*)&buf[p] = code->X;
        p+=4;
    }
    if(code->hasY())
    {
        *(float*)&buf[p] = code->Y;
        p+=4;
    }
    if(code->hasZ())
    {
        *(float*)&buf[p] = code->Z;
        p+=4;
    }
    if(code->hasE())
    {
        *(float*)&buf[p] = code->E;
        p+=4;
    }
    if(code->hasF())
    {
        *(float*)&buf[p] = code->F;
        p+=4;
    }
    if(code->hasT())
    {
        buf[p++] = code->T;
    }
    if(code->hasS())
    {
        *(long int*)&buf[p] = code->S;
        p+=4;
    }
    if(code->hasP())
    {
        *(long int*)&buf[p] = code->P;
        p+=4;
    }
    if(code->hasI())
    {
        *(float*)&buf[p] = code->I;
        p+=4;
    }
    if(code->hasJ())
    {
        *(float*)&buf[p] = code->J;
        p+=4;
    }
    if(code->hasString())   // read 16 uint8_t into string
    {
        char *sp = code->text;
        if(code->isV2())
        {
            uint8_t i = strlen(code->text);
            for(; i; i--) buf[p++] = *sp++;
        }
        else
        {
            for(uint8_t i=0; i<16; ++i) buf[p++] = *sp++;
        }
    }
    uint8_t *ptr = buf;
    uint8_t len = p;
    while (len)
    {
        uint8_t tlen = len > 21 ? 21 : len;
        len -= tlen;
        do
        {
            sum1 += *ptr++;
            if(sum1>=255) sum1-=255;
            sum2 += sum1;
            if(sum2>=255) sum2-=255;
        }
        while (--tlen);
    }
    buf[p++] = sum1;
    buf[p++] = sum2;
    if(params == 128)
    {
        Com::printErrorFLN(Com::tAPIDFinished);
    }
    else
        file.write(buf,p);
    if (file.writeError)
    {
        Com::printFLN(Com::tErrorWritingToFile);
    }
}

char *SDCard::createFilename(char *buffer,const dir_t &p)
{
    char *pos = buffer,*src = (char*)p.name;
    for (uint8_t i = 0; i < 11; i++,src++)
    {
        if (*src == ' ') continue;
        if (i == 8)
            *pos++ = '.';
        *pos++ = *src;
    }
    *pos = 0;
    return pos;
}

bool SDCard::showFilename(const uint8_t *name)
{
    if (*name == DIR_NAME_DELETED || *name == '.') return false;
#if !SD_ALLOW_LONG_NAMES
    uint8_t i=11;
    while(i--)
    {
        if(*name=='~') return false;
        name++;
    }
#endif
    return true;
}

int8_t stricmp(const char* s1, const char* s2)
{
    while(*s1 && (tolower(*s1) == tolower(*s2)))
        s1++,s2++;
    return (const uint8_t)tolower(*s1)-(const uint8_t)tolower(*s2);
}
int8_t strnicmp(const char* s1, const char* s2, size_t n)
{
    while(n--)
    {
        if(tolower(*s1)!=tolower(*s2))
            return (uint8_t)tolower(*s1) - (uint8_t)tolower(*s2);
        s1++;
        s2++;
    }
    return 0;
}
uint8_t SDCard::lsRecursive(SdBaseFile *parent, uint8_t level, char *findFilename)
{
    dir_t *p = NULL;
    uint8_t cnt=0;
    char *oldpathend = pathend;
    char filename[13];
    parent->rewind();

    while ((p = parent->getLongFilename(p, tempLongFilename)))
    {
        HAL::pingWatchdog();
        if (! (DIR_IS_FILE(p) || DIR_IS_SUBDIR(p))) continue;
        if( DIR_IS_SUBDIR(p))
        {
            if(level>=SD_MAX_FOLDER_DEPTH) continue; // can't go deeper
            if(level<SD_MAX_FOLDER_DEPTH)
            {
                createFilename(filename, *p);
                if (findFilename == NULL)
                {
                    if(level)
                    {
                        Com::print(fullName);
                        Com::print('/');
                    }
                    Com::print(filename);
                    Com::printFLN(Com::tSlash); //End with / to mark it as directory entry, so we can see empty directories.
                }
            }
            char *tmp = oldpathend;
            if(level) *tmp++ = '/';
            char *dirname = tmp;
            pathend = createFilename(tmp,*p);
            SdBaseFile next;
            uint16_t index = parent->curPosition()/32 - 1;
            if(next.open(parent, dirname, O_READ))
            {
                if (lsRecursive(&next,level+1, findFilename))
                    return true;
            }
            parent->seekSet(32 * (index + 1));
            *oldpathend = 0;
        }
        else
        {
            createFilename(filename,*p);

            if (findFilename != NULL)
            {
                int8_t cFullname;

                cFullname = strlen(fullName);
                if (strnicmp(fullName, findFilename, cFullname) == 0)
                {
                    if (cFullname > 0)
                        cFullname++;
                    if (stricmp(tempLongFilename, findFilename+cFullname) == 0)
                    {
                        strcpy(findFilename, filename);
                        parentFound = *parent;
                        return true;
                    }
                }
            }
            else
            {
                if(level)
                {
                    Com::print(fullName);
                    Com::print('/');
                }
#if SD_ALLOW_LONG_NAMES==true
                Com::print(tempLongFilename);
#else
                Com::print(filename);
#endif
#ifdef SD_EXTENDED_DIR
                Com::printF(Com::tSpace,(long)p->fileSize);
#endif
                Com::println();
            }
        }
    }
    return false;
}
void SDCard::ls()
{
    Com::printFLN(Com::tBeginFileList);
    *fullName = 0;
    pathend = fullName;
    fat.chdir();
    lsRecursive(fat.vwd(),0,NULL);
    Com::printFLN(Com::tEndFileList);
}

bool SDCard::selectFile(char *filename,bool silent)
{
    SdBaseFile *parent;
    char searchFilename[LONG_FILENAME_LENGTH+1], *oldP = filename, *p;
    SdBaseFile next;

    if(!sdactive) return false;
    sdmode = false;
    file.close();
    fat.chdir();

    parent = fat.vwd();
    if ((p = strrchr(oldP, '/')) != NULL)
    {
        *p = 0;
        if(next.open(parent, filename, O_READ))
            parent = &next;
        oldP = p+1;
    }
    strcpy(searchFilename, oldP);

    if (lsRecursive(parent,0, searchFilename) != NULL && file.open(&parentFound, searchFilename, O_READ))
    {
        if(!silent)
        {
            Com::printF(Com::tFileOpened,filename);
            Com::printFLN(Com::tSpaceSizeColon,file.fileSize());
        }
        sdpos = 0;
        filesize = file.fileSize();
        Com::printFLN(Com::tFileSelected);
        return true;
    }
    else
    {
        if(!silent)
            Com::printFLN(Com::tFileOpenFailed);
        return false;
    }
}

void SDCard::printStatus()
{
    if(sdactive)
    {
        Com::printF(Com::tSDPrintingByte,sdpos);
        Com::printFLN(Com::tSlash,filesize);
    }
    else
    {
        Com::printFLN(Com::tNotSDPrinting);
    }
}
void SDCard::startWrite(char *filename)
{
    if(!sdactive) return;
    file.close();
    sdmode = false;
    fat.chdir();
    if(!file.open(filename, O_CREAT | O_APPEND | O_WRITE | O_TRUNC))
    {
        Com::printFLN(Com::tOpenFailedFile,filename);
    }
    else
    {
        UI_STATUS(UI_TEXT_UPLOADING);
        savetosd = true;
        Com::printFLN(Com::tWritingToFile,filename);
    }
}
void SDCard::finishWrite()
{
    if(!savetosd) return; // already closed or never opened
    file.sync();
    file.close();
    savetosd = false;
    Com::printFLN(Com::tDoneSavingFile);
    UI_CLEAR_STATUS;
}
void SDCard::deleteFile(char *filename)
{
    if(!sdactive) return;
    sdmode = false;
    file.close();
    if(fat.remove(filename))
    {
        Com::printFLN(Com::tFileDeleted);
    }
    else
    {
        if(fat.rmdir(filename))
            Com::printFLN(Com::tFileDeleted);
        else
            Com::printFLN(Com::tDeletionFailed);
    }
}
void SDCard::makeDirectory(char *filename)
{
    if(!sdactive) return;
    sdmode = false;
    file.close();
    if(fat.mkdir(filename))
    {
        Com::printFLN(Com::tDirectoryCreated);
    }
    else
    {
        Com::printFLN(Com::tCreationFailed);
    }
}
#endif

