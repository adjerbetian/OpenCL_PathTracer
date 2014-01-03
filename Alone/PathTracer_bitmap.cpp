
#include "PathTracer_bitmap.h"


namespace PathTracerNS
{

	bool LoadBMPIntoDC ( HDC hDC, LPCTSTR bmpfile )
	{
		if ( ( hDC == NULL  ) || ( bmpfile == NULL ) )
			return false;

		HANDLE hBmp = LoadImage ( NULL, bmpfile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE );

		if ( hBmp == NULL )
			return false; 

		HDC dcmem = CreateCompatibleDC ( NULL );

		if ( SelectObject ( dcmem, hBmp ) == NULL )
		{	// failed to load bitmap into device context
			DeleteDC ( dcmem ); 
			return false; 
		}

		BITMAP bm;
		GetObject ( hBmp, sizeof(bm), &bm );

		// and blit it to the visible dc
		if ( BitBlt ( hDC, 0, 0, bm.bmWidth, bm.bmHeight, dcmem, 0, 0, SRCCOPY ) == 0 )
		{	// failed the blit
			DeleteDC ( dcmem ); 
			return false; 
		}

		DeleteDC ( dcmem );  // clear up the memory dc	
		return true;
	}

	BYTE* LoadBMP ( int* width, int* height, long* size, char* filePath )
	{
		// Creation of the LPCTSTR bmpfile
		int cubeMapPathLength = (int) strlen( filePath );
		wchar_t *cubeMapPathWChr = new wchar_t[cubeMapPathLength];
		MultiByteToWideChar(0, 0, filePath, cubeMapPathLength + 1, cubeMapPathWChr, cubeMapPathLength + 1);
		LPCTSTR bmpfile = cubeMapPathWChr;

		BITMAPFILEHEADER bmpheader;
		BITMAPINFOHEADER bmpinfo;
		DWORD bytesread;

		HANDLE file = CreateFile ( bmpfile , GENERIC_READ, FILE_SHARE_READ,	NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
		if ( file == NULL )
			return NULL;

		if ( ReadFile ( file, &bmpheader, sizeof ( BITMAPFILEHEADER ), &bytesread, NULL ) == false )
		{
			CloseHandle ( file );
			return NULL;
		}

		if ( ReadFile ( file, &bmpinfo, sizeof ( BITMAPINFOHEADER ), &bytesread, NULL ) == false )
		{
			CloseHandle ( file );
			return NULL;
		}

		//	Check if file is a BitMap
		if ( bmpheader.bfType != 'MB' )
		{
			CloseHandle ( file );
			return NULL;
		}

		//	Check if uncompressed
		if ( bmpinfo.biCompression != BI_RGB )
		{
			CloseHandle ( file );
			return NULL;
		}

		//	Check if 24 bit bmp
		if ( bmpinfo.biBitCount != 24 )
		{
			CloseHandle ( file );
			return NULL;
		}

		*width   = bmpinfo.biWidth;
		*height  = abs ( bmpinfo.biHeight );
		*size = bmpheader.bfSize - bmpheader.bfOffBits;

		BYTE* Buffer = new BYTE[ *size ];

		SetFilePointer ( file, bmpheader.bfOffBits, NULL, FILE_BEGIN );


		if ( ReadFile ( file, Buffer, *size, &bytesread, NULL ) == false )
		{
			delete [] Buffer;
			CloseHandle ( file );
			return NULL;
		}

		CloseHandle ( file );
		return Buffer;

	}


	BYTE* ConvertBMPToRGBBuffer ( BYTE* Buffer, int width, int height )
	{
		if ( ( NULL == Buffer ) || ( width == 0 ) || ( height == 0 ) )
			return NULL;

		int padding = 0;
		int scanlinebytes = width * 3;
		while ( (( scanlinebytes + padding ) & 3) != 0 )
			padding++;

		int psw = scanlinebytes + padding;

		BYTE* newbuf = new BYTE[width*height*3];

		long bufpos = 0;   
		long newpos = 0;
		for ( int y = 0; y < height; y++ )
		{
			for ( int x = 0; x < 3 * width; x+=3 )
			{
				newpos = y * 3 * width + x;     
				bufpos = ( height - y - 1 ) * psw + x;

				newbuf[newpos] = Buffer[bufpos + 2];       
				newbuf[newpos + 1] = Buffer[bufpos+1]; 
				newbuf[newpos + 2] = Buffer[bufpos];     
			}
		}

		return newbuf;
	}




	bool SaveBMP ( BYTE* buffer, int width, int height, long paddedsize, LPCWSTR bmpfile )
	{
		BITMAPFILEHEADER bmfh;
		BITMAPINFOHEADER info;
		memset ( &bmfh, 0, sizeof (BITMAPFILEHEADER ) );
		memset ( &info, 0, sizeof (BITMAPINFOHEADER ) );

		bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
		bmfh.bfReserved1 = 0;
		bmfh.bfReserved2 = 0;
		bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
		bmfh.bfOffBits = 0x36;

		info.biSize = sizeof(BITMAPINFOHEADER);
		info.biWidth = width;
		info.biHeight = height;
		info.biPlanes = 1;	
		info.biBitCount = 24;
		info.biCompression = BI_RGB;	
		info.biSizeImage = 0;
		info.biXPelsPerMeter = 0x0ec4;  
		info.biYPelsPerMeter = 0x0ec4;     
		info.biClrUsed = 0;	
		info.biClrImportant = 0; 


		HANDLE file = CreateFile ( bmpfile , GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( NULL == file )
		{
			CloseHandle ( file );
			return false;
		}


		unsigned long bwritten;
		if ( WriteFile ( file, &bmfh, sizeof ( BITMAPFILEHEADER ), &bwritten, NULL ) == false )
		{	
			CloseHandle ( file );
			return false;
		}

		if ( WriteFile ( file, &info, sizeof ( BITMAPINFOHEADER ), &bwritten, NULL ) == false )
		{	
			CloseHandle ( file );
			return false;
		}

		if ( WriteFile ( file, buffer, paddedsize, &bwritten, NULL ) == false )
		{	
			CloseHandle ( file );
			return false;
		}

		CloseHandle ( file );
		return true;
	}


	BYTE* ConvertRGBToBMPBuffer ( BYTE* Buffer, int width, int height, long* newsize )
	{
		if ( ( NULL == Buffer ) || ( width == 0 ) || ( height == 0 ) )
			return NULL;

		int padding = 0;
		int scanlinebytes = width * 3;
		while ( (( scanlinebytes + padding ) & 3) != 0 ) 
			padding++;
		int psw = scanlinebytes + padding;

		*newsize = height * psw;
		BYTE* newbuf = new BYTE[*newsize];

		memset ( newbuf, 0, *newsize );

		long bufpos = 0;   
		long newpos = 0;
		for ( int y = 0; y < height; y++ )
		{
			for ( int x = 0; x < 3 * width; x+=3 )
			{
				bufpos = y * 3 * width + x;     // position in original buffer
				newpos = ( height - y - 1 ) * psw + x; // position in padded buffer
				newbuf[newpos] = Buffer[bufpos+2];       // swap r and b
				newbuf[newpos + 1] = Buffer[bufpos + 1]; // g stays
				newbuf[newpos + 2] = Buffer[bufpos];     // swap b and r
			}
		}
		return newbuf;
	}


	BYTE* ConvertRGBAToBMPBuffer ( RGBAColor const * imageColor, float const * imageRay, int width, int height, long* newsize )
	{
		if ( ( NULL == imageColor ) || ( width == 0 ) || ( height == 0 ) )
			return NULL;

		int padding = 0;
		int scanlinebytes = width * 3;
		while ( (( scanlinebytes + padding ) & 3) != 0 ) 
			padding++;
		int psw = scanlinebytes + padding;

		*newsize = height * psw;
		BYTE* newbuf = new BYTE[*newsize];

		memset ( newbuf, 0, *newsize );

		long newpos = 0;
		long pos = 0;
		for ( int y = 0; y < height; y++ )
		{
			for ( int x = 0; x < width; x++ )
			{
				float sumFilterCoeff = imageRay == NULL ? 1 : imageRay[pos];

				int r,g,b;

				if( (imageColor[pos].x < 0) || (imageColor[pos].x < 0) || (imageColor[pos].x < 0) )
				{
					r = 255;
					g = 0;
					b = 0;
				}
				else
				{
					r = (int) min(imageColor[pos].x * 255.f / sumFilterCoeff, 255.f);
					g = (int) min(imageColor[pos].y * 255.f / sumFilterCoeff, 255.f);
					b = (int) min(imageColor[pos].z * 255.f / sumFilterCoeff, 255.f);
				}

				newbuf[newpos + 0] = b;		// swap r and b
				newbuf[newpos + 1] = g;		// g stays
				newbuf[newpos + 2] = r;		// swap b and r

				newpos += 3;
				pos++;
			}
			newpos += padding;
		}
		return newbuf;
	}

}
