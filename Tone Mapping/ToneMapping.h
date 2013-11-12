#include "stdafx.h"
#include "CL\cl.h"
#include "utils.h"


void Cleanup_OpenCL();
int Setup_OpenCL( const char *program_source );
cl_float* readInput(cl_uint* arrayWidth, cl_uint* arrayHeight);
void EvaluateRaw(float* inputArray, float* outputArray, CHDRData *pData, int arrayWidth, int iRow);
void ExecuteToneMappingReference(cl_float* inputArray, cl_float* outputArray, CHDRData *pData, cl_uint arrayWidth, cl_uint arrayHeight);
bool ExecuteToneMappingKernel(cl_float* inputArray, cl_float* outputArray, CHDRData *pData, cl_uint arrayWidth, cl_uint arrayHeight);
float resetFStopsParameter( float powKLow, float kHigh );
void Usage();

int ToneMappingTest(int argc, _TCHAR* argv[]);
