/**********************************************************************************
Filename	: AlazarAcquireThread.cpp
Authors		: Kevin Wong, Yifan Jian, Jing Xu, Marinko Sarunic
Published	: January 30th, 2014

Copyright (C) 2014 Biomedical Optics Research Group - Simon Fraser University

This file is part of a free software. Details of this software has been described 
in the paper: 

"Jian Y, Wong K, Sarunic MV; Graphics processing unit accelerated optical coherence 
tomography processing at megahertz axial scan rate and high resolution video rate 
volumetric rendering. J. Biomed. Opt. 0001;18(2):026002-026002.  
doi:10.1117/1.JBO.18.2.026002."

Please refer to this paper for further information about this software. Redistribution 
and modification of this code is restricted to academic purposes ONLY, provided that 
the following conditions are met:
-	Redistribution of this code must retain the above copyright notice, this list of 
	conditions and the following disclaimer
-	Any use, disclosure, reproduction, or redistribution of this software outside of 
	academic purposes is strictly prohibited

*DISCLAIMER*
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
TORT (INCLUDING NEGLIGENCE OR OTHERWISE)ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied.
**********************************************************************************/

#include "AlazarAcquireThread.h"
#define __SOFTWARERESAMPLE


int AcquireThreadAlazar::AlazarAcquireDMA (void)
{

	// local parameters
	unsigned short *DataFramePos;
	int channelCount;

	//set up the system
    RETURN_CODE status = ApiSuccess;
    unsigned int NumOfBoards;
	unsigned int systemId = 1;
	unsigned int boardId = 1;
	unsigned int bufferIndex; 
	unsigned char bitsPerSample; //Using this char as an 8-bit integer, not a character
	bool success = true;
	unsigned short* pBuffer;
	unsigned int maxSamplesPerChannel;
	fid = fopen("OCTLog.txt","w");

	AlazarSetParameter(
		alazarBoardHandle, 
		CHANNEL_A,
		SET_DATA_FORMAT,
		0 //Set the Alazar board to acquire as Unsigned.
	);

    alazarBoardHandle = AlazarGetBoardBySystemID(systemId, boardId);
	
	status = AlazarSetRecordSize( alazarBoardHandle, bd.PreDepth, bd.RecLength-bd.PreDepth);
	checkAlazarError(status, "AlazarSetRecordSize");

	status = AlazarSetRecordCount(alazarBoardHandle, bd.RecordCount);
	checkAlazarError(status, "AlazarSetRecordCount");

	status = AlazarSetCaptureClock( 
		alazarBoardHandle,
		bd.ClockSource,
		bd.SampleRate,
		bd.ClockEdge,
		0);
	checkAlazarError(status, "AlazarSetCaptureClock");
 
	// write DAC value to board -- CONTROL CLOCK LEVEL VALUE
    
	status = AlazarInputControl( alazarBoardHandle,
		CHANNEL_A,
		bd.CouplingChanA,
		bd.InputRangeChanA,
		bd.InputImpedChanA);
	checkAlazarError(status, "AlazarInputControl");
	
	
	status = AlazarInputControl( alazarBoardHandle,
		CHANNEL_B,
		bd.CouplingChanB,
		bd.InputRangeChanB,
		bd.InputImpedChanB);
	checkAlazarError(status, "AlazarInputControl");
		
	status = AlazarSetTriggerOperation( alazarBoardHandle,
		bd.TriEngOperation,
		bd.TriggerEngine1,
		bd.TrigEngSource1,
		bd.TrigEngSlope1,
		bd.TrigEngLevel1,
		bd.TriggerEngine2,
		bd.TrigEngSource2,
		bd.TrigEngSlope2,
		bd.TrigEngLevel2);
	checkAlazarError(status, "AlazarSetTriggerOperation");

	status = AlazarConfigureAuxIO(
		alazarBoardHandle, // HANDLE -- board handle
		AUX_IN_TRIGGER_ENABLE, // unsigned int -- mode
		 TRIGGER_SLOPE_POSITIVE// unsigned int -- parameter
		);
	checkAlazarError(status, "AlazarConfigureAuxIO");
	
	status = AlazarSetBWLimit (
		alazarBoardHandle, // HANDLE -- board handle
		CHANNEL_A, // unsigned int -- channel identifier
		0// unsigned int -- 0 = disable, 1 = enable
	);
	checkAlazarError(status, "AlazarSetBWLimit");

	if ((bd.TrigEngSource1 == TRIG_EXTERNAL)||(bd.TrigEngSource2 == TRIG_EXTERNAL))
	{
		status = AlazarSetExternalTrigger( alazarBoardHandle, ExtTriggerCoupling, ExtTriggerRange); // Set External Trigger to be 5V range, DC coupling
		checkAlazarError(status, "AlazarSetExternalTrigger");

		if (ExtTriggerRange == ETR_DIV5)
			fprintf(fid, "External TRIGGER Range DIV5");
		else
			fprintf(fid, "External TRIGGER Range X1");
		if (ExtTriggerCoupling == AC_COUPLING)
			fprintf(fid, "External TRIGGER Coupling AC");
		else
			fprintf(fid, "External TRIGGER Coupling DC");
	}
	else
		fprintf(fid, "INTERNAL TRIGGER");

	//Create a 5 Second timeout delay.  
	status = AlazarSetTriggerTimeOut( alazarBoardHandle, 0); // force to have no delay since FBG is consistent
	checkAlazarError(status, "AlazarSetTriggerTimeOut");

	channelCount = 2;
	mode = CHANNEL_A| CHANNEL_B; 

	status = AlazarGetChannelInfo(alazarBoardHandle, (unsigned long *)&maxSamplesPerChannel, &bitsPerSample);
	checkAlazarError(status, "AlazarGetChannelInfo");

	// Calculate the size of each DMA buffer in bytes
	unsigned int bytesPerSample = (bitsPerSample + 7) / 8;
	unsigned int bytesPerRecord = bytesPerSample * globalOptions->IMAGEWIDTH;
	unsigned int bytesPerBuffer = bytesPerRecord * globalOptions->IMAGEHEIGHT * channelCount;

	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && success; bufferIndex++)
		BufferArray[bufferIndex] = (unsigned short*)malloc(bytesPerBuffer);

	status = AlazarConfigureAuxIO (
		alazarBoardHandle,		// HANDLE -- board handle
		AUX_IN_TRIGGER_ENABLE,	// unsigned int -- mode 
		TRIGGER_SLOPE_POSITIVE	// unsigned int -- parameter (edge of slope)
		);
	checkAlazarError(status, "AlazarConfigureAuxIO");

	unsigned int recordsPerAcquisition = 0x7fffffff;//0x7fffffff means infinite acquisition
	unsigned int admaFlags = ADMA_EXTERNAL_STARTCAPTURE | ADMA_NPT;//Or we can use ADMA_TRADITIONAL_MODE;

    status = 
          AlazarBeforeAsyncRead(
			alazarBoardHandle, //board handle      
			mode, //2 channels
			-(long)bd.PreDepth, //pre trigger samples
			globalOptions->IMAGEWIDTH, //imagewidth 
			globalOptions->IMAGEHEIGHT,  //imageheight
			recordsPerAcquisition, //records per acquisition, which is infinite in our case
			admaFlags); 
	checkAlazarError(status, "AlazarBeforeAsyncRead");

	
	// Add the buffers to a list of buffers available to be filled by the board
	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && success; bufferIndex++)
	{
		pBuffer = BufferArray[bufferIndex];
		status = AlazarPostAsyncBuffer(alazarBoardHandle, pBuffer, bytesPerBuffer);
		checkAlazarError(status, "AlazarPostAsyncBuffer");
	}

	// START CAPTURE!!
	if (success==true)
	{
		status = AlazarStartCapture(alazarBoardHandle);
		checkAlazarError(status, "AlazarStartCapture");
		unsigned int buffersCompleted = 0;

		while (true)
		{
			if (globalOptions->volumeReady < 1)
			{
				DataFramePos = RawData;
				for (int framenum=0; framenum < globalOptions->NumFramesPerVol; framenum++)
				{
					//Wait for the acquiring buffer to complete
					bufferIndex = buffersCompleted % BUFFER_COUNT;
					pBuffer = BufferArray[bufferIndex];
					status = AlazarWaitAsyncBufferComplete(alazarBoardHandle, pBuffer, 10000); //10000 = 10s timeout per wait async buffer complete
					checkAlazarError(status, "AlazarWaitAsyncBufferComplete");
					
					//Memcpy the completed buffer to the RawData location
					if (!paused) {
						memcpy(DataFramePos, pBuffer, globalOptions->IMAGEHEIGHT*globalOptions->IMAGEWIDTH*sizeof(unsigned short));
					}
					DataFramePos = &DataFramePos[globalOptions->IMAGEHEIGHT*globalOptions->IMAGEWIDTH];

					//Release the completed buffer back into the acquisition queue
					status = AlazarPostAsyncBuffer(alazarBoardHandle, pBuffer, bytesPerBuffer);
					checkAlazarError(status, "AlazarPostAsyncBuffer");
					buffersCompleted++;

					//When Volume scan is NOT checked, always acquire in the first location. 
					if(!globalOptions->bVolumeScan)
						DataFramePos = RawData;

					if (breakLoop) {
						break;
					}
				}
			
				if (breakLoop) {
					breakLoop = false;
				} else {
					globalOptions->volumeReady++;
				}
			}
			else
			{
				if(globalOptions->saveVolumeflag == true)
				{
					fwrite(RawData,2,globalOptions->IMAGEWIDTH*globalOptions->IMAGEHEIGHT*globalOptions->NumFramesPerVol,globalOptions->fid);
					fclose(globalOptions->fid);
					globalOptions->saveVolumeflag = false;
					ScanThrPtr->InitializeSyncAndScan();	
				}
				globalOptions->volumeReady = 0;
			}
		}//while (Running Threads)
	  
		for (int bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
		{
			if (BufferArray[bufferIndex] != NULL)
				free(BufferArray[bufferIndex]);
		}
		status = AlazarAbortAsyncRead(alazarBoardHandle);
		checkAlazarError(status, "AlazarAbortAsyncRead");
	}
	return (0);
}



void AcquireThreadAlazar::SetupAlazarBoard(char* setupFileName)
{
	int InitVal;
    RETURN_CODE status = ApiSuccess;

	uInt32 u32ChannelIndexIncrement;
	FILE * fid;
	fid = fopen("OCTAlazar.txt","w");

	unsigned int NumOfBoards;
	unsigned char SDKMajor, SDKMinor, SDKRevision;
	unsigned char DriverMajor, DriverMinor, DriverRevision;

	NumOfBoards = AlazarBoardsFound();
	
	status = AlazarGetSDKVersion(&SDKMajor, &SDKMinor, &SDKRevision);
	checkAlazarError(status, "AlazarGetSDKVersion");

	status = AlazarGetDriverVersion(&DriverMajor, &DriverMinor, &DriverRevision);
	checkAlazarError(status, "AlazarGetDriverVersion");

	fprintf(fid,"\nSDK \t %d.%d.%d \nDriver \t %d.%d.%d \n",SDKMajor, SDKMinor, SDKRevision,DriverMajor, DriverMinor, DriverRevision);
	fprintf(fid,"Board Type \t %u\n", NumOfBoards);
	
	// these are for reading the .ini file
	TCHAR	szSection[100];
	TCHAR	szDefault[100];
	TCHAR	szString[100];

	bd.RecordCount	= globalOptions->IMAGEHEIGHT;
	bd.RecLength	= globalOptions->IMAGEWIDTH;

	if ( (AcqMode == FWDSCAN) | (AcqMode == BKWDSCAN) )
		bd.RecordCount		 = 	bd.RecordCount*2;		

	_tcscpy(szSection, _T("Acquisition"));		
	_tcscpy(szDefault, _T("Dual"));

	GetPrivateProfileString(szSection, _T("ATSPreTrig"), szDefault, szString, 100, setupFileName);
	bd.PreDepth = (long) atoi(szString);			

	GetPrivateProfileString(szSection, _T("ATSClock"), szDefault, szString, 100, setupFileName);
		
	if (bExtClk)
	{ // we know it is external, but which mode - default should be SLOW
		if (0 == _tcsicmp(szString, _T("EXTERNAL")))
		    bd.ClockSource		= EXTERNAL_CLOCK;
		// EXTERNAL_CLOCK_DC for ATS660 only
		else if (0 == _tcsicmp(szString, _T("EXTERNALDC")))
		    bd.ClockSource		= EXTERNAL_CLOCK_DC;
		// EXTERNAL_CLOCK_AC for ATS660 and 9350
		else if (0 == _tcsicmp(szString, _T("EXTERNALAC")))
		    bd.ClockSource		= EXTERNAL_CLOCK_AC;
		// FAST setting does not apply to ATS660 - only ATS460
		else if (0 == _tcsicmp(szString, _T("FAST")))
		    bd.ClockSource		= FAST_EXTERNAL_CLOCK;
		else if (0 == _tcsicmp(szString, _T("MEDIUM")))
		    bd.ClockSource		= MEDIMUM_EXTERNAL_CLOCK; // intential TYPO propagated from ATS header files MEDIMUM changed to MEDIUM
		else if (0 == _tcsicmp(szString, _T("SLOW")))
		    bd.ClockSource		= SLOW_EXTERNAL_CLOCK;// can also try MEDIMUM_EXTERNAL_CLOCK, FAST_EXTERNAL_CLOCK, and just EXTERNAL_CLOCK
		else //default to internal
		    bd.ClockSource		= INTERNAL_CLOCK;// want external later
	} 
	else
	    bd.ClockSource		= INTERNAL_CLOCK;// want external later

	bd.ClockEdge		= CLOCK_EDGE_FALLING;

	GetPrivateProfileString(szSection, _T("ATSSampleRate"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);			

	switch (InitVal)
	{
		case 500000000:
			bd.SampleRate = SAMPLE_RATE_500MSPS;
			break;
		case 250000000:
			bd.SampleRate = SAMPLE_RATE_250MSPS;
			break;
		case 200000000:                                                        
			bd.SampleRate = SAMPLE_RATE_200MSPS;
			break;
		case 180000000:
			bd.SampleRate = SAMPLE_RATE_180MSPS;
			break;
		case 160000000:
			bd.SampleRate = SAMPLE_RATE_160MSPS;
			break;
		case 125000000:
			bd.SampleRate = SAMPLE_RATE_125MSPS;
			break;
		case 100000000:
			bd.SampleRate = SAMPLE_RATE_100MSPS;
			break;
		case 50000000:
			bd.SampleRate = SAMPLE_RATE_50MSPS;
			break;
		case 10000000:
			bd.SampleRate = SAMPLE_RATE_10MSPS;
			break;
		case 0:
			bd.SampleRate = SAMPLE_RATE_USER_DEF;
			break;
		default:
			bd.SampleRate = SAMPLE_RATE_125MSPS;
			break;
	}

	// Do the Channel 1 parsing
	_tcscpy(szSection, _T("Channel1"));		

	GetPrivateProfileString(szSection, _T("Coupling"), szDefault, szString, 100, setupFileName);
	if (0 == _tcsicmp(szString, _T("AC")))
		bd.CouplingChanA	= AC_COUPLING;
	else // assume DC coupling
		bd.CouplingChanA	= DC_COUPLING;

	GetPrivateProfileString(szSection, _T("Range"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);		

/*ATS660
1 MΩ input impedance: ±200mV, ±400mV, ±800mV,
±2V, ±4V, ±8V, and ±16V,
software selectable
50 Ω input impedance: ±200mV, ±400mV, ±800mV,
±2V, and ±4V, software selectable

ATS9350
50 Ω input impedance: ±40mV, ±100mV, ±200mV, ±400mV, ±800mV,
±1V, ±2V,±4V,
*/
	switch (InitVal)
	{
		case 40:
			bd.InputImpedChanA = INPUT_RANGE_PM_40_MV;
			break;
		case 100:
			bd.InputRangeChanA	= INPUT_RANGE_PM_100_MV;
			break;
		case 200:
			bd.InputRangeChanA	= INPUT_RANGE_PM_200_MV;
			break;
		case 400:
			bd.InputRangeChanA	= INPUT_RANGE_PM_400_MV;
			break;
		case 800:
			bd.InputRangeChanA	= INPUT_RANGE_PM_800_MV;
			break;
		case 1000:
			bd.InputRangeChanA	= INPUT_RANGE_PM_1_V;
			break;
		case 2000:
			bd.InputRangeChanA	= INPUT_RANGE_PM_2_V;
			break;
		case 4000:
			bd.InputRangeChanA	= INPUT_RANGE_PM_4_V;
			break;
		case 8000:
			bd.InputRangeChanA	= INPUT_RANGE_PM_8_V;
			break;
		case 16000:
			bd.InputRangeChanA	= INPUT_RANGE_PM_16_V;
			break;
		default:
			bd.InputRangeChanA	= INPUT_RANGE_PM_800_MV;
			break;
	}

	//IMPEDANCE
	GetPrivateProfileString(szSection, _T("Impedance"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);
	if (InitVal == 50)
		bd.InputImpedChanA	= IMPEDANCE_50_OHM; 
	else //assume 1M OHM
		bd.InputImpedChanA	= IMPEDANCE_1M_OHM; 
	
	// Do the Channel 2 parsing
	_tcscpy(szSection, _T("Channel2"));		

	GetPrivateProfileString(szSection, _T("Coupling"), szDefault, szString, 100, setupFileName);
	if (0 == _tcsicmp(szString, _T("AC")))
		bd.CouplingChanB	= AC_COUPLING;
	else // assume DC coupling
		bd.CouplingChanB	= DC_COUPLING;

	GetPrivateProfileString(szSection, _T("Range"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);			
	switch (InitVal)
	{
		case 40:
			bd.InputRangeChanB = INPUT_RANGE_PM_40_MV;
			break;
		case 100:
			bd.InputRangeChanB	= INPUT_RANGE_PM_100_MV;
			break;
		case 200:
			bd.InputRangeChanB	= INPUT_RANGE_PM_200_MV;
			break;
		case 400:
			bd.InputRangeChanB	= INPUT_RANGE_PM_400_MV;
			break;
		case 800:
			bd.InputRangeChanB	= INPUT_RANGE_PM_800_MV;
			break;
		case 1000:
			bd.InputRangeChanB	= INPUT_RANGE_PM_1_V;
			break;
		case 2000:
			bd.InputRangeChanB	= INPUT_RANGE_PM_2_V;
			break;
		case 4000:
			bd.InputRangeChanB	= INPUT_RANGE_PM_4_V;
			break;
		case 8000:
			bd.InputRangeChanB	= INPUT_RANGE_PM_8_V;
			break;
		case 16000:
			bd.InputRangeChanB	= INPUT_RANGE_PM_16_V;
			break;
		default:
			bd.InputRangeChanB	= INPUT_RANGE_PM_800_MV;
			break;
	}

	GetPrivateProfileString(szSection, _T("Impedance"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);
	if (InitVal == 50)
		bd.InputImpedChanB	= IMPEDANCE_50_OHM; 
	else
		bd.InputImpedChanB	= IMPEDANCE_1M_OHM; 

	// Do the Trigger parsing
	_tcscpy(szSection, _T("Trigger1"));		

    bd.TriEngOperation	= TRIG_ENGINE_OP_J;//_AND_K;//TRIG_ENGINE_OP_J;
    bd.TriggerEngine1	= TRIG_ENGINE_J;

	GetPrivateProfileString(szSection, _T("Source"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString); 
	if (InitVal == 2)
		bd.TrigEngSource1	= TRIG_CHAN_B;
	if (InitVal == -1)
		bd.TrigEngSource1	= TRIG_EXTERNAL;
	else
		bd.TrigEngSource1	= TRIG_DISABLE; 
	//bd.TrigEngSource1 = TRIG_CHAN_A;
	GetPrivateProfileString(szSection, _T("Condition"), szDefault, szString, 100, setupFileName);
	if (0 == _tcsicmp(szString, _T("Rising")))
		bd.TrigEngSlope1	= TRIGGER_SLOPE_POSITIVE;
	else // assume DC coupling
		bd.TrigEngSlope1 = TRIGGER_SLOPE_NEGATIVE;
		

	GetPrivateProfileString(szSection, _T("Coupling"), szDefault, szString, 100, setupFileName);
	if (0 == _tcsicmp(szString, _T("AC")))
		ExtTriggerCoupling	= AC_COUPLING;
	else // assume DC coupling
		ExtTriggerCoupling	= DC_COUPLING;

	ExtTriggerRange = ETR_5V;
	
	GetPrivateProfileString(szSection, _T("Level"), szDefault, szString, 100, setupFileName);
	InitVal = atoi(szString);
	if (InitVal == NULL)
		bd.TrigEngLevel1	= 50;
	else
		bd.TrigEngLevel1	= InitVal;

	bd.TriggerEngine2	= TRIG_ENGINE_K;
    bd.TrigEngSource2	= TRIG_CHAN_B;
    bd.TrigEngSlope2	= TRIGGER_SLOPE_POSITIVE;
    bd.TrigEngLevel2	= 133;


fprintf(fid, "\nbd.ATSClockSource\t%u\nbd.RecordCount\t%u\nbd.RecLength\t%u\nbd.PreDepth\t%u\nbd.ClockEdge\t%u\nbd.SampleRate\t%u\nbd.CouplingChanA\t%d\nbd.InputRangeChanA\t%d\nbd.InputImpedChanA\t%d\nbd.CouplingChanB\t%d\nbd.InputRangeChanB\t%d\nbd.InputImpedChanB\t%d\nbd.TriEngOperation\t%d\nbd.TriggerEngine1\t%d\nbd.TrigEngSource1\t%d\nbd.TrigEngSlope1\t%d\nbd.TrigEngLevel1\t%d\nbd.TriggerEngine2\t%d\nbd.TrigEngSource2\t%d\nbd.TrigEngSlope2\t%d\nbd.TrigEngLevel2\t%d \nExtTriggerRange %d \nExtTriggerCoupling%d\n " 
   , bd.ClockSource
   , bd.RecordCount		
   , bd.RecLength		
   , bd.PreDepth		
   , bd.ClockEdge		
   , bd.SampleRate		
   , bd.CouplingChanA	
   , bd.InputRangeChanA
   , bd.InputImpedChanA	
   , bd.CouplingChanB	
   , bd.InputRangeChanB	
   , bd.InputImpedChanB	
   , bd.TriEngOperation	
   , bd.TriggerEngine1	
   , bd.TrigEngSource1	
   , bd.TrigEngSlope1	
   , bd.TrigEngLevel1	
   , bd.TriggerEngine2	
   , bd.TrigEngSource2	
   , bd.TrigEngSlope2	
   , bd.TrigEngLevel2
   , ExtTriggerRange
   , ExtTriggerCoupling);
}


void AcquireThreadAlazar::checkAlazarError(RETURN_CODE status, char *alazarCmd)
{
	if (status != ApiSuccess) {
		printf("ERROR: Alazar proc '%s' returned error code: %d - %s\n", alazarCmd, status, AlazarErrorToText(status));
	}
}

void AcquireThreadAlazar::UpdateTriggerVal (int TVal)
{

	bd.TrigEngLevel1 = TVal;

	RETURN_CODE status = AlazarSetTriggerOperation( alazarBoardHandle
	,bd.TriEngOperation
	,bd.TriggerEngine1
	,bd.TrigEngSource1
	,bd.TrigEngSlope1
	,bd.TrigEngLevel1
	,bd.TriggerEngine2
	,bd.TrigEngSource2
	,bd.TrigEngSlope2
	,bd.TrigEngLevel2);

	checkAlazarError(status, "AlazarSetTriggerOperation");
}

void AcquireThreadAlazar::UpdateTriggerDelay (int TrigDelayVal)
{
	//trigger delay, number of samples
	RETURN_CODE status = AlazarSetTriggerDelay(alazarBoardHandle, (unsigned int) TrigDelayVal);
	checkAlazarError(status, "AlazarSetTriggerDelay");
}


void AcquireThreadAlazar::UpdatePreTriggerPts (int PreDepthVal)
{
	bd.PreDepth = PreDepthVal;
	RETURN_CODE status = AlazarSetRecordSize( alazarBoardHandle, bd.PreDepth, bd.RecLength-bd.PreDepth);
	checkAlazarError(status, "AlazarSetRecordSize");
}


AcquireThreadAlazar::~AcquireThreadAlazar()
{
	for (int bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
	{
		if (BufferArray[bufferIndex] != NULL)
		{
			free(BufferArray[bufferIndex]);
		}
	}

	RETURN_CODE status = AlazarAbortAsyncRead(alazarBoardHandle);
	checkAlazarError(status, "AlazarAbortAsyncRead");
}



 
