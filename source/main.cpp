#include <string>
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

#include <3ds.h>

#include "utils.h"
#include "cia.h"

#include "builtin_rootca_der.h"


static const std::string NUS_URL = "http://nus.cdn.c.shop.nintendowifi.net/ccs/download/";
static const std::string YLS8_URL = "https://yls8.mtheall.com/ninupdates/titlelist.php?";

typedef enum
{
    SYS_CTR = 0,
    SYS_KTR
} SystemType;

typedef enum
{
    REG_USA = 0,
    REG_JPN,
    REG_EUR,
    REG_KOR,
    REG_TWN,
} SystemRegion;

std::string GetSystemString(int sys)
{
    switch (sys)
    {
        case SYS_CTR: return "ctr";
        case SYS_KTR: return "ktr";
    }

    return "";
}

std::string GetRegionString(int reg)
{
    switch (reg)
    {
        case REG_USA: return "e";
        case REG_JPN: return "j";
        case REG_EUR: return "p";
        case REG_KOR: return "k";
        case REG_TWN: return "t";
    }

    return "";
}

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

Result network_request(char *hostname, char *http_netreq, std::ofstream &ofs)
{
    Result ret=0;

    struct addrinfo hints;
    struct addrinfo *resaddr = NULL, *resaddr_cur;
    int sockfd;
    u8 *readbuf = (u8 *)linearAlloc(0x400);

    sslcContext sslc_context;
    u32 RootCertChain_contexthandle=0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd==-1)
    {
        printf("Failed to create the socket.\n");
        return -1;
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(hostname, "443", &hints, &resaddr)!=0)
    {
        printf("getaddrinfo() failed.\n");
        closesocket(sockfd);
        return -1;
    }

    for(resaddr_cur = resaddr; resaddr_cur!=NULL; resaddr_cur = resaddr_cur->ai_next)
    {
        if(connect(sockfd, resaddr_cur->ai_addr, resaddr_cur->ai_addrlen)==0)break;
    }

    freeaddrinfo(resaddr);

    if(resaddr_cur==NULL)
    {
        printf("Failed to connect.\n");
        closesocket(sockfd);
        return -1;
    }

    ret = sslcCreateRootCertChain(&RootCertChain_contexthandle);
    if(R_FAILED(ret))
    {
        printf("sslcCreateRootCertChain() failed: 0x%08x.\n", (unsigned int)ret);
        closesocket(sockfd);
        return ret;
    }

    ret = sslcAddTrustedRootCA(RootCertChain_contexthandle, (u8*)builtin_rootca_der, builtin_rootca_der_size, NULL);
    if(R_FAILED(ret))
    {
        printf("sslcAddTrustedRootCA() failed: 0x%08x.\n", (unsigned int)ret);
        closesocket(sockfd);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        return ret;
    }

    ret = sslcCreateContext(&sslc_context, sockfd, SSLCOPT_Default, hostname);
    if(R_FAILED(ret))
    {
        printf("sslcCreateContext() failed: 0x%08x.\n", (unsigned int)ret);
        closesocket(sockfd);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        return ret;
    }

    ret = sslcContextSetRootCertChain(&sslc_context, RootCertChain_contexthandle);
    if(R_FAILED(ret))
    {
        printf("sslcContextSetRootCertChain() failed: 0x%08x.\n", (unsigned int)ret);
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        closesocket(sockfd);
        return ret;
    }

    ret = sslcStartConnection(&sslc_context, NULL, NULL);
    if(R_FAILED(ret))
    {
        printf("sslcStartConnection() failed: 0x%08x.\n", (unsigned int)ret);
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        closesocket(sockfd);
        return ret;
    }

    ret = sslcWrite(&sslc_context, (u8*)http_netreq, strlen(http_netreq));
    if(R_FAILED(ret))
    {
        printf("sslcWrite() failed: 0x%08x.\n", (unsigned int)ret);
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        closesocket(sockfd);
        return ret;
    }

    memset(readbuf, 0, 0x400);

    while ((ret = sslcRead(&sslc_context, readbuf, 0x400-1, false)) > 0)
    {
        if(R_FAILED(ret))
        {
            printf("sslcWrite() failed: 0x%08x.\n", (unsigned int)ret);
            sslcDestroyContext(&sslc_context);
            sslcDestroyRootCertChain(RootCertChain_contexthandle);
            closesocket(sockfd);
            return ret;
        }

        ofs.write((const char *)readbuf, ret);
    }
    
    sslcDestroyContext(&sslc_context);
    sslcDestroyRootCertChain(RootCertChain_contexthandle);
    
    closesocket(sockfd);

    return 0;
}

Result DownloadFileSecure(std::string url, std::ofstream &ofs)
{
    std::string page = url.substr(url.find(".com/") + 4);
    size_t len = url.find(".com") + 4 - (url.find("://") + 3);
    std::string hostname = url.substr(url.find("://") + 3, len);
    std::string req = "GET " + page + " HTTP/1.1\r\nUser-Agent: FirmwareDownloader\r\nConnection: close\r\nHost: " + hostname + "\r\n\r\n";
    return network_request((char *)hostname.c_str(), (char *)req.c_str(), ofs);
}

Result DownloadCSV(std::string sys, std::string reg, std::string outputDir)
{
    std::ofstream ofs;
    ofs.open((outputDir + "/" + sys + "_" + reg + ".csv").c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    Result res = DownloadFileSecure(YLS8_URL + "sys=" + sys + "&csv=1" + "&reg=" + reg, ofs);
    if (res != 0)
    {
        remove((outputDir + "/" + sys + "_" + reg + ".csv").c_str());
    }
    ofs.close();
    return res;
}

bool FileExists (std::string name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

Result CheckCSVFiles(std::string dir)
{
    for (int sys = SYS_CTR; sys <= SYS_KTR; ++sys)
    {
        for (int reg = REG_USA; reg <= REG_TWN; ++reg)
        {
            if (!FileExists(dir + "/" + GetSystemString(sys) + "_" + GetRegionString(reg) + ".csv"))
            {
                printf("Downloading CSV for system/region:%s/%s...", GetSystemString(sys).c_str(), GetRegionString(reg).c_str());
                Result res = DownloadCSV(GetSystemString(sys), GetRegionString(reg), dir);
                if (res != 0)
                {
                    printf("error downloading CSV: %ld\n", res);
                    return res;
                }
                else
                {
                    printf("done\n");
                }
            }
        }
    }

    return 0;
}

int main()
{
    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    gfxInitDefault();
    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();

    consoleInit(GFX_TOP, NULL);
    printf("FirmwareDownloader by machinamentum\n");
    CheckCSVFiles("sdmc:/3ds/FirmwareDownloader");


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
    socExit();
    sslcExit();
}