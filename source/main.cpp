#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <hbkb.h>

#include <3ds.h>

#include "utils.h"
#include "cia.h"
#include "data.h"

#include "svchax/svchax.h"

static const u16 top = 0x140;
static bool bSvcHaxAvailable = false;
static bool bInstallMode = false;

bool FileExists (std::string name){
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

Result ConvertToCIA(std::string dir, std::string titleId)
{
    char cwd[1024];
    if (getcwdir(cwd, sizeof(cwd)) == NULL){
        printf("[!] Could not store Current Working Directory\n");
        return -1;
    }
    chdir(dir.c_str());
    FILE *tik = fopen("cetk", "rb");
    if (!tik) return -1;
    TIK_CONTEXT tik_context = process_tik(tik);

    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) return -1;
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        fclose(tik);
        fclose(tmd);
        return -1;
    }

    chdir(cwd);

    FILE *output = fopen((dir + "/" + titleId + ".cia").c_str(),"wb");
    if (!output) return -2;

    int result;
    if (bInstallMode)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        result = generate_cia(tmd_context, tik_context, output);
        if(result != 0){
            remove((dir + "/" + titleId + ".cia").c_str());
        }
    }

    return result;
}

std::string u32_to_hex_string(u32 i)
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::setw(sizeof(u32)*2) << std::hex << i;
    return stream.str();
}

int mkpath(std::string s,mode_t mode)
{
    size_t pre=0,pos;
    std::string dir;
    int mdret = 0;

    if(s[s.size()-1]!='/'){
        // force trailing / so we can handle everything in loop
        s+='/';
    }

    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.size()==0) continue; // if leading / first time is 0 length
        if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            return mdret;
        }
    }
    return mdret;
}

char parse_hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    std::abort();
}

char* parse_string(const std::string & s)
{
    char* buffer = new char[s.size() / 2];
    for (std::size_t i = 0; i != s.size() / 2; ++i)
        buffer[i] = 16 * parse_hex(s[2 * i]) + parse_hex(s[2 * i + 1]);
    return buffer;
}

void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath)
{
    std::ofstream ofs;

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    ofs.write(tikTemp, 0xA50);
    ofs.close();

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::in | std::ofstream::binary);

    //write version
    ofs.seekp(top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);

    //write title id
    ofs.seekp(top+0x9C, std::ios::beg);
    ofs.write(parse_string(titleId), 0x8);

    //write key
    ofs.seekp(top+0x7F, std::ios::beg);
    ofs.write(parse_string(encTitleKey), 0x10);

    ofs.close();
}

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string outputDir)
{
    printf("Starting %s - %s\n", (bInstallMode ? "install" : "download"), titleId.c_str());

    mkpath((outputDir + "/tmp/").c_str(), 0777);
    //if (FileExists(outputDir + "/" + titleId + ".cia")) return 0;
    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) return -1;
    Result res = DownloadFile((NUS_URL + titleId + "/tmd").c_str(), oh, false);
    fclose(oh);
    if (res != 0)
    {
        printf("Could not download TMD. Internet/Title ID is OK?\n");
        return res;
    }

    //read version
    std::ifstream tmdfs;
    tmdfs.open(outputDir + "/tmp/tmd", std::ofstream::out | std::ofstream::in | std::ofstream::binary);
    char titleVersion[2];
    tmdfs.seekg(top+0x9C, std::ios::beg);
    tmdfs.read(titleVersion, 0x2);
    tmdfs.close();

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/cetk");

    printf("Now %s the CIA...\n", (bInstallMode ? "installing" : "creating"));

    res = ConvertToCIA(outputDir + "/tmp", titleId);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", (bInstallMode ? "install" : "create"));
        return res;
    }

    if (!bInstallMode)
    {
        rename((outputDir + "/tmp/" + titleId + ".cia").c_str(), (outputDir + "/" + titleId + ".cia").c_str());
    }

    printf(" DONE!\n");
    printf("Enjoy the game :)\n");

    // TODO remove tmp dir

    return res;
}

std::string getInput(HB_Keyboard* sHBKB)
{
    sHBKB->HBKB_Clean();
    touchPosition touch;
    u8 KBState = 4;
    std::string input;
    while (KBState != 1 || input.length() == 0)
    {
        hidScanInput();
        hidTouchRead(&touch);
        KBState = sHBKB->HBKB_CallKeyboard(touch);
        input = sHBKB->HBKB_CheckKeyboardInput();
        if (KBState != 4)
        {
            printf("%c[2K\r", 27);
            printf("%s", input.c_str());
        }
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
    }
    printf("\n");
    return input;
}

std::istream& GetLine(std::istream& is, std::string& t)
{
    t.clear();
    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
              return is;
            case '\r':
              if (sb->sgetc() == '\n')
                sb->sbumpc();
              return is;
            case  EOF:
              if (t.empty())
                is.setstate(std::ios::eofbit);
              return is;
            default:
              t += (char)c;
        }
    }
}

int main(int argc, const char* argv[])
{
    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    gfxInitDefault();
    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();

    // Trigger svchax so we can install CIAs
    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            printf("Failed to acquire kernel access. Install mode disabled.\n");
            return -1;
        } else {
            bSvcHaxAvailable = true;
        }
    }

    amInit();
    AM_InitializeExternalTitleDatabase(false);

    consoleInit(GFX_TOP, NULL);
    printf("CIAngel by cearp and Drakia\n\n");
    printf("Press Start to exit\n");
    printf("Press A to read data from SD and download CIA.\n");
    printf("Press X to input a Key/ID pair and download CIA.\n");
    printf("Press Y = dl encTitleKeys.bin from 3ds.nfshost.com\n");
    printf("Press R to switch to Install mode (EXPERIMENTAL!)\n");
    printf("\n");

    HB_Keyboard sHBKB;
    touchPosition touch;

    while (aptMainLoop())
    {
        hidScanInput();

        u32 keys = hidKeysDown();

        if (keys & KEY_A)
        {
            std::ifstream input;
            std::string titleId;
            std::string key;
            input.open("/CIAngel/input.txt", std::ofstream::in);
            GetLine(input, titleId);
            GetLine(input, key);
            DownloadTitle(titleId, key, "/CIAngel");
            
            printf("Press START to exit.\n\n");
        }

        if (keys & KEY_X)
        {
            printf("Please enter a titleID:\n");
            std::string titleId = getInput(&sHBKB);
            printf("Please enter the corresponding encTitleKey:\n");
            std::string key = getInput(&sHBKB);
            if (titleId.length() == 16 && key.length() == 32)
            {
                DownloadTitle(titleId, key, "/CIAngel");
            } else
            {
                printf("encTitleKeys are 32 characters long,\nand titleIDs are 16 characters long.\nPress X to try again, or Start to exit.\n");
            }
        }

        if (keys & KEY_Y)
        {
            printf("Downloading encTitleKeys.bin...\n");
            FILE *oh = fopen("/CIAngel/encTitleKeys.bin", "wb");
            Result res = DownloadFile("http://3ds.nfshost.com/downloadenc", oh, true);
            if (res != 0)
            {
                printf("Could not download file.\n");
                fclose(oh);
                return res;
            }
            fclose(oh);
            printf("Downloaded OK!\n");
        }

        if (keys & KEY_R)
        {
            bInstallMode = !bInstallMode;
            if (bInstallMode)
            {
                if (!bSvcHaxAvailable)
                {
                    printf("Kernel access not available. Can't enable Install Mode.\n");
                    bInstallMode = false;
                }
                else
                {
                    printf("Switched to Install Mode. This is EXPERIMENTAL!\n");
                }
            }
            else
            {
                printf("Switched to Create Mode.\n");
            }
        }

        if (keys & KEY_START) break;

        //Prevent keyboard flicker after failed input attempt
        hidTouchRead(&touch);
        sHBKB.HBKB_CallKeyboard(touch);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    amExit();

    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
