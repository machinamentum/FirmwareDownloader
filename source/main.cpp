#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <3ds.h>

#include "utils.h"
#include "cia.h"


static const std::string NUS_URL = "http://nus.cdn.c.shop.nintendowifi.net/ccs/download/";

Result DownloadFile(std::string url, std::ofstream &os)
{
    httpcContext context;
    u32 fileSize = 0;
    Result ret = 0;
    u32 status;

    httpcOpenContext(&context, HTTPC_METHOD_GET, (char *)url.c_str(), 1);

    ret = httpcBeginRequest(&context);
    if (ret != 0) return ret;

    ret = httpcGetResponseStatusCode(&context, &status, 0);
    if (ret != 0) return ret;

    if (status != 200) return status;

    ret = httpcGetDownloadSizeState(&context, NULL, &fileSize);
    if (ret != 0) return ret;

    unsigned char *buffer = (unsigned char *)linearAlloc(fileSize);
    memset(buffer, 0, fileSize);

    printf("Downloading file of size %d bytes\n", fileSize);
    ret = httpcDownloadData(&context, buffer, fileSize, NULL);
    if (ret != 0)
    {
        free(buffer);
        return ret;
    }

    os.write((char *)buffer, fileSize);
    linearFree(buffer);

    httpcCloseContext(&context);

    return 0;
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
    printf("processing TIK...\n");
    TIK_CONTEXT tik_context = process_tik(tik);

    printf("processing TMD...\n");
    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) return -1;
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        free(tmd_context.content);
        fclose(tik);
        fclose(tmd);
        return -1;
    }
    //TID comparison check
    if(check_tid(tik_context.title_id,tmd_context.title_id) != 1){
        printf("[!] Caution, Ticket and TMD Title IDs do not match\n");
        printf("[!] CETK Title ID:  "); u8_hex_print_be(tik_context.title_id,0x8); printf("\n");
        printf("[!] TMD Title ID:   "); u8_hex_print_be(tmd_context.title_id,0x8); printf("\n");
    }
    //Title Version comparison
    if(tik_context.title_version != tmd_context.title_version){
        printf("[!] Caution, Ticket and TMD Title Versions do not match\n");
        printf("[!] CETK Title Ver: %d\n",tik_context.title_version);
        printf("[!] TMD Title Ver:  %d\n",tmd_context.title_version);
    }

    chdir(cwd);

    FILE *output = fopen((dir + "/" + titleId + ".cia").c_str(),"wb");
    if (!output) return -2;

    printf("building CIA...\n");
    int result = generate_cia(tmd_context,tik_context,output);
    if(result != 0){
        remove((dir + "/" + titleId + ".cia").c_str());
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
    int mdret;

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

Result DownloadTitle(std::string titleId, std::string version, std::string outputDir)
{
    mkpath((outputDir + "/tmp/").c_str(), 0777);
    std::ofstream ofs;
    ofs.open(outputDir + "/tmp/tmd", std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    Result res = DownloadFile(NUS_URL + titleId + "/tmd." + version, ofs);
    if (res != 0)
    {
        ofs.close();
        return res;
    }
    ofs.close();

    u16 numContents = 0;
    std::ifstream tmdfs;
    tmdfs.open(outputDir + "/tmp/tmd", std::ofstream::in | std::ofstream::binary);
    tmdfs.seekg(518, std::ios::beg);
    tmdfs.read((char *)&numContents, 2);
    numContents = __builtin_bswap16(numContents);
    printf("\nnumContents:%d\n", numContents);
    for (u16 i = 0; i <= numContents; ++i)
    {
        int offset = 2820 + (48 * (i - 1));
        tmdfs.seekg(offset, std::ios::beg);
        u32 cid = 0;
        tmdfs.read((char *)&cid, 4);
        std::string contentId = u32_to_hex_string(__builtin_bswap32(cid));
        ofs = std::ofstream();
        ofs.open(outputDir + "/tmp/" + contentId, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
        DownloadFile(NUS_URL + titleId + "/" + contentId, ofs);
        ofs.close();
        printf("Downloaded %s\n", contentId.c_str());
    }
    tmdfs.close();

    ofs.open(outputDir + "/tmp/cetk", std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    res = DownloadFile(NUS_URL + titleId + "/cetk", ofs);
    ofs.close();

    ConvertToCIA(outputDir + "/tmp", titleId);

    rename((outputDir + "/tmp/" + titleId + ".cia").c_str(), (outputDir + "/" + titleId + ".cia").c_str());

    // TODO remove tmp dir

    return res;
}

int main()
{
    gfxInitDefault();
    httpcInit(0);
    hidInit();

    consoleInit(GFX_TOP, NULL);
    printf("Downloading Title: 0004001000021700 v2055...");
    Result res = DownloadTitle("0004001000021700", "2055", "sdmc:/3ds/FirmwareDownloader");
    if (res)
        printf("Error downloading title\n");
    else
        printf("Download successful.\n");


    while (aptMainLoop())
    {
        hidScanInput();

        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    hidExit();
    httpcExit();
}