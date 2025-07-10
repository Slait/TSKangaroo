// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC

#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h> 
#include <inttypes.h>
#include <stdint.h>

#include "cuda_runtime.h"
#include "cuda.h"

#include "../defs.h"
#include "../utils.h"
#include "../GpuKang.h"
#include "ServerClient.h"

#ifndef _WIN32
#include <unistd.h>
#endif

time_t program_start_time = time(NULL);

// Global variables
EcJMP EcJumps1[JMP_CNT];
EcJMP EcJumps2[JMP_CNT];
EcJMP EcJumps3[JMP_CNT];

RCGpuKang* GpuKangs[MAX_GPU_CNT];
int GpuCnt;
volatile long ThrCnt;
volatile bool gSolved;

EcInt Int_HalfRange;
EcPoint Pnt_HalfRange;
EcPoint Pnt_NegHalfRange;
EcInt Int_TameOffset;
Ec ec;

CriticalSection csAddPoints;
u8* pPntList;
u8* pPntList2;
volatile int PntIndex;
TFastBase db;  // Local DB for non-server mode
EcPoint gPntToSolve;
EcInt gPrivKey;

volatile u64 TotalOps;
u32 TotalSolved;
u32 gTotalErrors;
u64 PntTotalOps;
bool IsBench;

u32 gDP;
u32 gRange;
EcInt gStart;
bool gStartSet;
EcPoint gPubKey;
u8 gGPUs_Mask[MAX_GPU_CNT];
char gTamesFileName[1024];
double gMax;
bool gGenMode;
bool gIsOpsLimit;

// Server-specific variables
std::string gServerUrl;
std::string gClientId;
std::string gRangeSize;
std::unique_ptr<ServerWork> gCurrentWork;
std::vector<ServerDP> gPendingPoints;
u64 gLastServerSubmit = 0;
const u64 SERVER_SUBMIT_INTERVAL = 30000; // Submit every 30 seconds

#pragma pack(push, 1)
struct DBRec
{
    u8 x[12];
    u8 d[22];
    u8 type;
};
#pragma pack(pop)

void InitGpus()
{
    GpuCnt = 0;
    int gcnt = 0;
    cudaGetDeviceCount(&gcnt);
    if (gcnt > MAX_GPU_CNT)
        gcnt = MAX_GPU_CNT;

    if (!gcnt)
        return;

    int drv, rt;
    cudaRuntimeGetVersion(&rt);
    cudaDriverGetVersion(&drv);
    char drvver[100];
    sprintf(drvver, "%d.%d/%d.%d", drv / 1000, (drv % 100) / 10, rt / 1000, (rt % 100) / 10);

    printf("CUDA devices: %d, CUDA driver/runtime: %s\r\n", gcnt, drvver);
    cudaError_t cudaStatus;
    for (int i = 0; i < gcnt; i++)
    {
        cudaStatus = cudaSetDevice(i);
        if (cudaStatus != cudaSuccess)
        {
            printf("cudaSetDevice for gpu %d failed!\r\n", i);
            continue;
        }

        if (!gGPUs_Mask[i])
            continue;

        cudaDeviceProp deviceProp;
        cudaGetDeviceProperties(&deviceProp, i);
        printf("GPU %d: %s, %.2f GB, %d CUs, cap %d.%d, PCI %d, L2 size: %d KB\r\n", 
               i, deviceProp.name, 
               ((float)(deviceProp.totalGlobalMem / (1024 * 1024))) / 1024.0f, 
               deviceProp.multiProcessorCount, deviceProp.major, deviceProp.minor,
               deviceProp.pciBusID, deviceProp.l2CacheSize / 1024);

        if (deviceProp.major < 6)
        {
            printf("GPU %d - not supported, skip\r\n", i);
            continue;
        }

        cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);

        GpuKangs[GpuCnt] = new RCGpuKang();
        GpuKangs[GpuCnt]->CudaIndex = i;
        GpuKangs[GpuCnt]->persistingL2CacheMaxSize = deviceProp.persistingL2CacheMaxSize;
        GpuKangs[GpuCnt]->mpCnt = deviceProp.multiProcessorCount;
        GpuKangs[GpuCnt]->IsOldGpu = deviceProp.l2CacheSize < 16 * 1024 * 1024;
        GpuCnt++;
    }
    printf("Total GPUs for work: %d\r\n", GpuCnt);
}

#ifdef _WIN32
u32 __stdcall kang_thr_proc(void* data)
{
    RCGpuKang* Kang = (RCGpuKang*)data;
    Kang->Execute();
    InterlockedDecrement(&ThrCnt);
    return 0;
}
#else
void* kang_thr_proc(void* data)
{
    RCGpuKang* Kang = (RCGpuKang*)data;
    Kang->Execute();
    __sync_fetch_and_sub(&ThrCnt, 1);
    return 0;
}
#endif

void AddPointsToList(u32* data, int pnt_cnt, u64 ops_cnt)
{
    csAddPoints.Enter();
    
    if (g_useServer) {
        // Convert points to server format and add to pending list
        for (int i = 0; i < pnt_cnt; i++) {
            u8* p = (u8*)data + i * GPU_DP_SIZE;
            ServerDP dp = g_serverClient->convertDP(p);
            gPendingPoints.push_back(dp);
        }
    } else {
        // Original local database logic
        if (PntIndex + pnt_cnt >= MAX_CNT_LIST) {
            csAddPoints.Leave();
            printf("DPs buffer overflow, some points lost, increase DP value!\r\n");
            return;
        }
        memcpy(pPntList + GPU_DP_SIZE * PntIndex, data, pnt_cnt * GPU_DP_SIZE);
        PntIndex += pnt_cnt;
    }
    
    PntTotalOps += ops_cnt;
    csAddPoints.Leave();
}

bool Collision_SOTA(EcPoint& pnt, EcInt t, int TameType, EcInt w, int WildType, bool IsNeg)
{
    if (IsNeg)
        t.Neg();
    if (TameType == TAME)
    {
        gPrivKey = t;
        gPrivKey.Sub(w);
        EcInt sv = gPrivKey;
        gPrivKey.Add(Int_HalfRange);
        EcPoint P = ec.MultiplyG(gPrivKey);
        if (P.IsEqual(pnt))
            return true;
        gPrivKey = sv;
        gPrivKey.Neg();
        gPrivKey.Add(Int_HalfRange);
        P = ec.MultiplyG(gPrivKey);
        return P.IsEqual(pnt);
    }
    else
    {
        gPrivKey = t;
        gPrivKey.Sub(w);
        if (gPrivKey.data[4] >> 63)
            gPrivKey.Neg();
        gPrivKey.ShiftRight(1);
        EcInt sv = gPrivKey;
        gPrivKey.Add(Int_HalfRange);
        EcPoint P = ec.MultiplyG(gPrivKey);
        if (P.IsEqual(pnt))
            return true;
        gPrivKey = sv;
        gPrivKey.Neg();
        gPrivKey.Add(Int_HalfRange);
        P = ec.MultiplyG(gPrivKey);
        return P.IsEqual(pnt);
    }
}

void CheckNewPoints()
{
    if (g_useServer) {
        // Submit points to server periodically
        u64 current_time = GetTickCount64();
        if (current_time - gLastServerSubmit > SERVER_SUBMIT_INTERVAL && !gPendingPoints.empty()) {
            csAddPoints.Enter();
            std::vector<ServerDP> points_to_submit = gPendingPoints;
            gPendingPoints.clear();
            csAddPoints.Leave();
            
            std::string response_status, solution;
            if (g_serverClient->submitPoints(points_to_submit, response_status, solution)) {
                if (response_status == "solved") {
                    gSolved = true;
                    printf("\r\n*** SOLUTION FOUND BY SERVER! ***\r\n");
                    printf("Solution: %s\r\n", solution.c_str());
                }
            }
            gLastServerSubmit = current_time;
        }
        
        // Also check if server found solution
        if (!gSolved) {
            std::string solution;
            if (g_serverClient->checkSolved(solution)) {
                gSolved = true;
                printf("\r\n*** SOLUTION FOUND BY ANOTHER CLIENT! ***\r\n");
                printf("Solution: %s\r\n", solution.c_str());
            }
        }
    } else {
        // Original local collision detection logic
        csAddPoints.Enter();
        if (!PntIndex) {
            csAddPoints.Leave();
            return;
        }

        int cnt = PntIndex;
        memcpy(pPntList2, pPntList, GPU_DP_SIZE * cnt);
        PntIndex = 0;
        csAddPoints.Leave();

        for (int i = 0; i < cnt; i++) {
            DBRec nrec;
            u8* p = pPntList2 + i * GPU_DP_SIZE;
            memcpy(nrec.x, p, 12);
            memcpy(nrec.d, p + 16, 22);
            nrec.type = gGenMode ? TAME : p[40];

            DBRec* pref = (DBRec*)db.FindOrAddDataBlock((u8*)&nrec);
            if (gGenMode)
                continue;
            if (pref) {
                DBRec tmp_pref;
                memcpy(&tmp_pref, &nrec, 3);
                memcpy(((u8*)&tmp_pref) + 3, pref, sizeof(DBRec) - 3);
                pref = &tmp_pref;

                if (pref->type == nrec.type) {
                    if (pref->type == TAME)
                        continue;
                    if (*(u64*)pref->d == *(u64*)nrec.d)
                        continue;
                }

                EcInt w, t;
                int TameType, WildType;
                if (pref->type != TAME) {
                    memcpy(w.data, pref->d, sizeof(pref->d));
                    if (pref->d[21] == 0xFF) memset(((u8*)w.data) + 22, 0xFF, 18);
                    memcpy(t.data, nrec.d, sizeof(nrec.d));
                    if (nrec.d[21] == 0xFF) memset(((u8*)t.data) + 22, 0xFF, 18);
                    TameType = nrec.type;
                    WildType = pref->type;
                } else {
                    memcpy(w.data, nrec.d, sizeof(nrec.d));
                    if (nrec.d[21] == 0xFF) memset(((u8*)w.data) + 22, 0xFF, 18);
                    memcpy(t.data, pref->d, sizeof(pref->d));
                    if (pref->d[21] == 0xFF) memset(((u8*)t.data) + 22, 0xFF, 18);
                    TameType = TAME;
                    WildType = nrec.type;
                }

                bool res = Collision_SOTA(gPntToSolve, t, TameType, w, WildType, false) || 
                          Collision_SOTA(gPntToSolve, t, TameType, w, WildType, true);
                if (!res) {
                    bool w12 = ((pref->type == WILD1) && (nrec.type == WILD2)) || 
                              ((pref->type == WILD2) && (nrec.type == WILD1));
                    if (!w12) {
                        printf("Collision Error\r\n");
                        gTotalErrors++;
                    }
                    continue;
                }
                gSolved = true;
                break;
            }
        }
    }
}

void ShowStats(u64 tm_start, double exp_ops, double dp_val)
{
    int speed = GpuKangs[0]->GetStatsSpeed();
    for (int i = 1; i < GpuCnt; i++)
        speed += GpuKangs[i]->GetStatsSpeed();

    u64 est_dps_cnt = (u64)(exp_ops / dp_val);
    u64 exp_sec = 0xFFFFFFFFFFFFFFFFull;

    if (speed)
        exp_sec = (u64)((exp_ops / 1000000) / speed);

    u64 exp_days = exp_sec / (3600 * 24);
    int exp_hours = (int)(exp_sec % (3600 * 24)) / 3600;
    int exp_min = (int)(exp_sec % 3600) / 60;
    int exp_remaining_sec = (int)(exp_sec % 60);

    u64 sec = (GetTickCount64() - tm_start) / 1000;
    u64 days = sec / (3600 * 24);
    int hours = (int)(sec % (3600 * 24)) / 3600;
    int min = (int)(sec % 3600) / 60;
    int remaining_sec = (int)(sec % 60);

    const char* mode_prefix;
    if (g_useServer) {
        mode_prefix = "CLIENT: ";
    } else if (gGenMode) {
        mode_prefix = "GEN: ";
    } else if (IsBench) {
        mode_prefix = "BENCH: ";
    } else {
        mode_prefix = "MAIN: ";
    }

    u64 dp_count;
    if (g_useServer) {
        dp_count = gPendingPoints.size();
    } else {
        dp_count = db.GetBlockCnt();
    }

    printf("%sSpeed: %d MKeys/s, Err: %d, DPs: %lluK/%lluK, Time: %llud:%02dh:%02dm:%02ds/%llud:%02dh:%02dm:%02ds",
        mode_prefix, speed, gTotalErrors,
        dp_count / 1000, est_dps_cnt / 1000,
        days, hours, min, remaining_sec,
        exp_days, exp_hours, exp_min, exp_remaining_sec
    );
    
    if (g_useServer && !g_currentRangeId.empty()) {
        printf(" [%s]", g_currentRangeId.c_str());
    }
    
    printf("\r");
    fflush(stdout);
}

bool SolvePoint(EcPoint PntToSolve, int Range, int DP, EcInt* pk_res)
{
    if ((Range < 32) || (Range > 180)) {
        printf("Unsupported Range value (%d)!\r\n", Range);
        return false;
    }
    if ((DP < 14) || (DP > 60)) {
        printf("Unsupported DP value (%d)!\r\n", DP);
        return false;
    }

    printf("\r\n");
    if (g_useServer) {
        printf("CLIENT MODE\r\n");
        printf("Server: %s\r\n", gServerUrl.c_str());
        printf("Client ID: %s\r\n", gClientId.c_str());
    } else {
        printf("MAIN MODE\r\n");
    }
    printf("\r\n");

    printf("Solving point: Range %d bits, DP %d, start...\r\n", Range, DP);
    double ops = 1.15 * pow(2.0, Range / 2.0);
    double dp_val = (double)(1ull << DP);
    double ram = (32 + 4 + 4) * ops / dp_val;
    ram += sizeof(TListRec) * 256 * 256 * 256;
    ram /= (1024 * 1024 * 1024);
    printf("SOTA method, estimated ops: 2^%.3f, RAM for DPs: %.3f GB. DP and GPU overheads not included!\r\n", log2(ops), ram);

    u64 total_kangs = GpuKangs[0]->CalcKangCnt();
    for (int i = 1; i < GpuCnt; i++)
        total_kangs += GpuKangs[i]->CalcKangCnt();
    double path_single_kang = ops / total_kangs;
    double DPs_per_kang = path_single_kang / dp_val;
    printf("Estimated DPs per kangaroo: %.3f.%s\r\n", DPs_per_kang, 
           (DPs_per_kang < 5) ? " DP overhead is big, use less DP value if possible!" : "");

    SetRndSeed(0);
    PntTotalOps = 0;
    PntIndex = 0;

    // Prepare jumps
    EcInt minjump, t;
    minjump.Set(1);
    minjump.ShiftLeft(Range / 2 + 3);
    for (int i = 0; i < JMP_CNT; i++) {
        EcJumps1[i].dist = minjump;
        t.RndMax(minjump);
        EcJumps1[i].dist.Add(t);
        EcJumps1[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE;
        EcJumps1[i].p = ec.MultiplyG(EcJumps1[i].dist);
    }

    minjump.Set(1);
    minjump.ShiftLeft(Range - 10);
    for (int i = 0; i < JMP_CNT; i++) {
        EcJumps2[i].dist = minjump;
        t.RndMax(minjump);
        EcJumps2[i].dist.Add(t);
        EcJumps2[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE;
        EcJumps2[i].p = ec.MultiplyG(EcJumps2[i].dist);
    }

    minjump.Set(1);
    minjump.ShiftLeft(Range - 10 - 2);
    for (int i = 0; i < JMP_CNT; i++) {
        EcJumps3[i].dist = minjump;
        t.RndMax(minjump);
        EcJumps3[i].dist.Add(t);
        EcJumps3[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE;
        EcJumps3[i].p = ec.MultiplyG(EcJumps3[i].dist);
    }
    SetRndSeed(GetTickCount64());

    Int_HalfRange.Set(1);
    Int_HalfRange.ShiftLeft(Range - 1);
    Pnt_HalfRange = ec.MultiplyG(Int_HalfRange);
    Pnt_NegHalfRange = Pnt_HalfRange;
    Pnt_NegHalfRange.y.NegModP();
    Int_TameOffset.Set(1);
    Int_TameOffset.ShiftLeft(Range - 1);
    EcInt tt;
    tt.Set(1);
    tt.ShiftLeft(Range - 5);
    Int_TameOffset.Sub(tt);
    gPntToSolve = PntToSolve;

    // Prepare GPUs
    for (int i = 0; i < GpuCnt; i++) {
        if (!GpuKangs[i]->Prepare(PntToSolve, Range, DP, EcJumps1, EcJumps2, EcJumps3)) {
            GpuKangs[i]->Failed = true;
            printf("GPU %d Prepare failed\r\n", GpuKangs[i]->CudaIndex);
        }
    }

    u64 tm0 = GetTickCount64();
    printf("GPUs started...\r\n");

#ifdef _WIN32
    HANDLE thr_handles[MAX_GPU_CNT];
#else
    pthread_t thr_handles[MAX_GPU_CNT];
#endif

    u32 ThreadID;
    gSolved = false;
    ThrCnt = GpuCnt;
    gLastServerSubmit = GetTickCount64();
    
    for (int i = 0; i < GpuCnt; i++) {
#ifdef _WIN32
        thr_handles[i] = (HANDLE)_beginthreadex(NULL, 0, kang_thr_proc, (void*)GpuKangs[i], 0, &ThreadID);
#else
        pthread_create(&thr_handles[i], NULL, kang_thr_proc, (void*)GpuKangs[i]);
#endif
    }

    u64 tm_stats = GetTickCount64();
    while (!gSolved) {
        CheckNewPoints();

#ifdef _WIN32
        Sleep(5);
#else
        usleep(5000);
#endif

        if (GetTickCount64() - tm_stats > 5000) {
            ShowStats(tm0, ops, dp_val);
            tm_stats = GetTickCount64();
        }
    }

    printf("\r\n");
    printf("\r\nStopping work ...\r\n");
    
    for (int i = 0; i < GpuCnt; i++)
        GpuKangs[i]->Stop();

    while (ThrCnt > 0) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }

    // Submit remaining points to server
    if (g_useServer && !gPendingPoints.empty()) {
        std::string response_status, solution;
        g_serverClient->submitPoints(gPendingPoints, response_status, solution);
        gPendingPoints.clear();
    }

    u64 tm1 = GetTickCount64();
    int sec = (int)((tm1 - tm0) / 1000);
    int min = sec / 60;
    sec %= 60;
    int hours = min / 60;
    min %= 60;
    
    printf("Total Time: ");
    if (hours > 0) printf("%d hours, ", hours);
    if (min > 0) printf("%d minutes, ", min);
    printf("%d seconds\r\n", sec);

    double exp_k = ops / (PntTotalOps * 1.15);
    printf("Point solved, K: %.3f (with DP and GPU overheads)\r\n", exp_k);

    char str[1000];
    gPrivKey.GetHex(str);
    printf("\r\n\r\nPRIVATE KEY: %s\r\n", str);

    FILE* f = fopen("RESULTS.TXT", "a");
    if (f) {
        fprintf(f, "PRIVATE KEY: %s\r\n", str);
        fclose(f);
    }

    for (int i = 0; i < GpuCnt; i++)
        GpuKangs[i]->Release();

    return true;
}

bool ParseCommandLine(int argc, char* argv[])
{
    memset(gGPUs_Mask, 1, sizeof(gGPUs_Mask));
    gGenMode = false;
    gStartSet = false;
    gMax = 0;
    gTamesFileName[0] = 0;
    g_useServer = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-gpu")) {
            if (i + 1 >= argc) {
                printf("Missing GPU list\r\n");
                return false;
            }
            memset(gGPUs_Mask, 0, sizeof(gGPUs_Mask));
            char* gpu_list = argv[i + 1];
            for (int j = 0; gpu_list[j]; j++) {
                if ((gpu_list[j] >= '0') && (gpu_list[j] <= '9')) {
                    int gpu_ind = gpu_list[j] - '0';
                    if (gpu_ind < MAX_GPU_CNT)
                        gGPUs_Mask[gpu_ind] = 1;
                }
            }
            i++;
        } else if (!strcmp(argv[i], "-server")) {
            if (i + 1 >= argc) {
                printf("Missing server URL\r\n");
                return false;
            }
            gServerUrl = argv[i + 1];
            g_useServer = true;
            i++;
        } else if (!strcmp(argv[i], "-clientid")) {
            if (i + 1 >= argc) {
                printf("Missing client ID\r\n");
                return false;
            }
            gClientId = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-configure")) {
            // Configure server mode
            if (i + 5 >= argc) {
                printf("Configure requires: start_range end_range pubkey dp_bits range_size\r\n");
                return false;
            }
            if (!g_useServer) {
                printf("Configure mode requires -server option\r\n");
                return false;
            }
            // Configuration will be handled in main()
            i += 5;
        } else if (!strcmp(argv[i], "-pubkey")) {
            if (i + 1 >= argc) {
                printf("Missing public key\r\n");
                return false;
            }
            if (!gPubKey.SetHex(argv[i + 1])) {
                printf("Invalid public key\r\n");
                return false;
            }
            i++;
        } else if (!strcmp(argv[i], "-start")) {
            if (i + 1 >= argc) {
                printf("Missing start offset\r\n");
                return false;
            }
            if (!gStart.SetHex(argv[i + 1])) {
                printf("Invalid start offset\r\n");
                return false;
            }
            gStartSet = true;
            i++;
        } else if (!strcmp(argv[i], "-range")) {
            if (i + 1 >= argc) {
                printf("Missing range\r\n");
                return false;
            }
            
            // Check if it's a range format like "start:end"
            char* range_str = argv[i + 1];
            char* colon = strchr(range_str, ':');
            if (colon) {
                // Range format: start:end
                *colon = '\0';
                EcInt start_range, end_range;
                if (!start_range.SetHex(range_str) || !end_range.SetHex(colon + 1)) {
                    printf("Invalid range format\r\n");
                    return false;
                }
                gStart = start_range;
                gStartSet = true;
                
                // Calculate bit range
                EcInt diff = end_range;
                diff.Sub(start_range);
                gRange = diff.GetBitLength();
            } else {
                // Bit range format
                gRange = atoi(argv[i + 1]);
            }
            i++;
        } else if (!strcmp(argv[i], "-dp")) {
            if (i + 1 >= argc) {
                printf("Missing DP value\r\n");
                return false;
            }
            gDP = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-max")) {
            if (i + 1 >= argc) {
                printf("Missing max value\r\n");
                return false;
            }
            gMax = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-tames")) {
            if (i + 1 >= argc) {
                printf("Missing tames filename\r\n");
                return false;
            }
            strcpy(gTamesFileName, argv[i + 1]);
            i++;
        } else {
            printf("Unknown option: %s\r\n", argv[i]);
            return false;
        }
    }

    if (g_useServer) {
        if (gServerUrl.empty()) {
            printf("Server mode requires -server option\r\n");
            return false;
        }
        if (gClientId.empty()) {
            // Generate default client ID
            char hostname[256];
#ifdef _WIN32
            DWORD size = sizeof(hostname);
            GetComputerNameA(hostname, &size);
#else
            gethostname(hostname, sizeof(hostname));
#endif
            gClientId = std::string(hostname) + "_" + std::to_string(time(NULL));
        }
    } else {
        // Non-server mode validation
        if (!gPubKey.IsSet()) {
            IsBench = true;
            return true;
        }
        if (!gStartSet) {
            printf("Start offset is required for non-benchmark mode\r\n");
            return false;
        }
        if (!gRange) {
            printf("Range is required\r\n");
            return false;
        }
        if (!gDP) {
            printf("DP value is required\r\n");
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
    printf("This software is free and open-source: https://github.com/RetiredC\r\n");
    printf("It demonstrates fast GPU implementation of SOTA Kangaroo method for solving ECDLP\r\n");
#ifdef _WIN32
    printf("Windows version\r\n");
#else
    printf("Linux version\r\n");
#endif

    if (!ParseCommandLine(argc, argv)) {
        printf("\r\nUsage:\r\n");
        printf("Local mode:\r\n");
        printf("  %s -dp <dp_bits> -range <bits> -start <hex> -pubkey <hex> [-gpu <list>]\r\n", argv[0]);
        printf("  %s -dp <dp_bits> -range <start_hex>:<end_hex> -pubkey <hex> [-gpu <list>]\r\n", argv[0]);
        printf("\r\nServer mode:\r\n");
        printf("  %s -server <url> [-clientid <id>] [-gpu <list>]\r\n", argv[0]);
        printf("  %s -server <url> -configure <start> <end> <pubkey> <dp_bits> <range_size>\r\n", argv[0]);
        printf("\r\nBenchmark:\r\n");
        printf("  %s [-gpu <list>]\r\n", argv[0]);
        return 1;
    }

    // Initialize server client if needed
    if (g_useServer) {
        g_serverClient = new ServerClient(gServerUrl, gClientId);
        if (!g_serverClient->initialize()) {
            printf("Failed to initialize server client\r\n");
            return 1;
        }

        // Check for configuration mode
        bool configure_mode = false;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-configure")) {
                configure_mode = true;
                std::string start_range = argv[i + 1];
                std::string end_range = argv[i + 2];
                std::string pubkey = argv[i + 3];
                int dp_bits = atoi(argv[i + 4]);
                std::string range_size = argv[i + 5];
                
                if (g_serverClient->configureSearch(start_range, end_range, pubkey, dp_bits, range_size)) {
                    printf("Server configured successfully\r\n");
                } else {
                    printf("Failed to configure server\r\n");
                    return 1;
                }
                break;
            }
        }

        if (configure_mode) {
            delete g_serverClient;
            return 0;
        }
    }

    InitGpus();

    if (!GpuCnt) {
        printf("No GPUs found\r\n");
        return 1;
    }

    pPntList = (u8*)malloc(MAX_CNT_LIST * GPU_DP_SIZE);
    pPntList2 = (u8*)malloc(MAX_CNT_LIST * GPU_DP_SIZE);

    if (g_useServer) {
        // Server mode: get work assignments and solve
        while (true) {
            gCurrentWork = g_serverClient->getWork();
            if (!gCurrentWork) {
                printf("No work available from server, waiting...\r\n");
#ifdef _WIN32
                Sleep(30000);  // Wait 30 seconds
#else
                sleep(30);
#endif
                continue;
            }

            // Parse work assignment
            EcPoint pubkey;
            if (!pubkey.SetHex(gCurrentWork->pubkey)) {
                printf("Invalid public key from server\r\n");
                continue;
            }

            EcInt start_offset;
            if (!start_offset.SetHex(gCurrentWork->start_range)) {
                printf("Invalid start range from server\r\n");
                continue;
            }

            gPubKey = pubkey;
            gStart = start_offset;
            gRange = gCurrentWork->bit_range;
            gDP = gCurrentWork->dp_bits;
            gStartSet = true;

            printf("Working on range: %s to %s\r\n", 
                   gCurrentWork->start_range.c_str(), 
                   gCurrentWork->end_range.c_str());

            // Solve the assigned range
            SolvePoint(gPubKey, gRange, gDP, &gPrivKey);

            if (gSolved) {
                printf("Range completed with solution found!\r\n");
                break;
            } else {
                printf("Range completed, requesting new work...\r\n");
            }
        }
    } else {
        // Local mode
        if (IsBench) {
            // Benchmark mode
            gRange = 78;
            gDP = 16;
            gPubKey.RndPoint();
            gStart.Set(1);
            gStart.ShiftLeft(gRange - 1);
        }

        printf("Start Range: ");
        char str[1000];
        gStart.GetHex(str);
        printf("%s\r\n", str);

        EcInt end_range = gStart;
        EcInt range_size;
        range_size.Set(1);
        range_size.ShiftLeft(gRange);
        end_range.Add(range_size);
        end_range.GetHex(str);
        printf("End   Range: %s\r\n", str);
        printf("Bits: %d\r\n", gRange);

        SolvePoint(gPubKey, gRange, gDP, &gPrivKey);
    }

    free(pPntList);
    free(pPntList2);

    if (g_useServer) {
        delete g_serverClient;
    }

    return 0;
}