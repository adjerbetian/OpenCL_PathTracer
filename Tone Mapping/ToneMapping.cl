
#include "C:\Users\Alexandre Djerbetian\Documents\Visual Studio 2012\Projects\OpenCL_PathTracer\src\Tone Mapping\ToneMapping_Header.cl"

__kernel void /*__attribute__((vec_type_hint(float4)))*/  ToneMappingPerPixel(
	__global float4* inputImage, 
	__global float4* outputImage,
    __global CHDRData* pData,
    int iImageWidth
    )
{
	// Load tone mapping parameters
    float4 fPowKLow = (float4)pData->fPowKLow;
    float4 fFStops = (float4)pData->fFStops;
    float4 fFStopsInv = (float4)pData->fFStopsInv;
    float4 fPowExposure = (float4)pData->fPowExposure;
    float4 fDefog = (float4)pData->fDefog;
    float4 fGamma = (float4)pData->fGamma;
    float4 fPowGamma = (float4)pData->fPowGamma;


	// and define method constants.
    float4 fOne = 1.0f;
    float4 fZerro = 0.0f;
    float4 fSaturate = 255.f;
    
    
    int iRow = get_global_id(0);
    int iCol = get_global_id(1);

    ///for (int iCol = 0; iCol< iImageWidth; iCol++)
    {
        float4 fColor = 0.0f;

        fColor = inputImage[iRow*iImageWidth+iCol];


        //Defog 
        fColor = fColor - fDefog;
        fColor = max(fZerro, fColor);


        // Multiply color by pow( 2.0f, exposure +  2.47393f )
        fColor = fColor * fPowExposure;

        int4 iCmpFlag = 0;
        //iCmpFlag = isgreater(fColor, fPowKLow);
        iCmpFlag = fColor > fPowKLow;

        if(any(iCmpFlag))
        {

            //fPowKLow = 2^kLow
            //fFStopsInv = 1/fFStops;
            //fTmpPixel = fPowKLow + log((fTmpPixel-fPowKLow) * fFStops + 1.0f)*fFStopsInv;
            float4 fTmpPixel = fColor - fPowKLow;
            fTmpPixel = fTmpPixel * fFStops;
            fTmpPixel = fTmpPixel + fOne;
            fTmpPixel = native_log( fTmpPixel );
            fTmpPixel = fTmpPixel * fFStopsInv;
            fTmpPixel = fTmpPixel + fPowKLow;

			//color channels update versions
            ///fColor = select(fTmpPixel, fColor, iCmpFlag);
            ///fColor = select(fColor, fTmpPixel, iCmpFlag);
            fColor = fTmpPixel;
        }

        //Gamma correction
        fColor = powr(fColor, fGamma);

        // Scale the values
        fColor = fColor * fSaturate;
        fColor = fColor * fPowGamma;

        //Saturate
        fColor = max(fColor, 0.f);
        fColor = min(fColor, fSaturate);

        //Store result pixel 
        outputImage[iRow*iImageWidth+iCol] = fColor;
    }
}
