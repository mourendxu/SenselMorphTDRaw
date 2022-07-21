/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "SenselMorphCHOP.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
void
FillCHOPPluginInfo(CHOP_PluginInfo *info)
{
	// Always set this to CHOPCPlusPlusAPIVersion.
	info->apiVersion = CHOPCPlusPlusAPIVersion;

	// The opType is the unique name for this CHOP. It must start with a 
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Senselmorphraw");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("Sensel Morph Raw");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Da Xu");
	info->customOPInfo.authorEmail->setString("da.xu@wearesohappy.net");

	// This CHOP can work with 0 inputs
	info->customOPInfo.minInputs = 0;

	// It can accept up to 1 input though, which changes it's behavior
	info->customOPInfo.maxInputs = 0;
}

DLLEXPORT
CHOP_CPlusPlusBase*
CreateCHOPInstance(const OP_NodeInfo* info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per CHOP that is using the .dll
	return new SenselMorphCHOP(info);
}

DLLEXPORT
void
DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the CHOP using that instance is deleted, or
	// if the CHOP loads a different DLL
	delete (SenselMorphCHOP*)instance;
}

};


SenselMorphCHOP::SenselMorphCHOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	mySenselHandle = nullptr;
	frame = nullptr;
	currentSenselId = -1;

	getSensels();
	reconnectSensel = true;

	/*
	getSensels();
	getSensels();
	*/
}

SenselMorphCHOP::~SenselMorphCHOP()
{
	if(mySenselHandle != nullptr)
		senselClose(mySenselHandle);
}

void SenselMorphCHOP::getSensels(int senselIdx)
{


	if ((frame != NULL) && (mySenselHandle != NULL))
	{
		if (senselFreeFrameData(mySenselHandle, frame) == SENSEL_OK)
			frame = NULL;
		else
			return;
	}


	
	if (mySenselHandle != NULL)
	{
		if (senselClose(mySenselHandle) == SENSEL_OK)
		{
			mySenselHandle = NULL;
		}
		else
		{
			return;
		}
	}

	senselGetDeviceList(&mySenselList);


	

	if (mySenselList.num_devices > 0)
	{
		if (senselOpenDeviceByID(&mySenselHandle, mySenselList.devices[senselIdx].idx) != SENSEL_OK)
		{
			mySenselHandle = NULL;
			return;
		}

		senselGetFirmwareInfo(mySenselHandle, &myFirmwareInfo);
		senselGetSensorInfo(mySenselHandle, &mySenselInfo);

		senselSetMaxFrameRate(mySenselHandle, 125);

		//senselSetScanMode(mySenselHandle, SenselScanMode::SCAN_MODE_SYNC);
		senselSetScanDetail(mySenselHandle, SenselScanDetail::SCAN_DETAIL_LOW);

		senselSetFrameContent(mySenselHandle, FRAME_CONTENT_CONTACTS_MASK);
		senselSetContactsMask(mySenselHandle, CONTACT_MASK_BOUNDING_BOX | CONTACT_MASK_PEAK );


		senselSetDynamicBaselineEnabled(mySenselHandle, 1);
		senselSetContactsEnableBlobMerge(mySenselHandle, 1);

		senselAllocateFrameData(mySenselHandle, &frame);
		senselStartScanning(mySenselHandle);

		currentSenselId = senselIdx;

	}



}



void
SenselMorphCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	// Note: To disable timeslicing you'll need to turn this off, as well as ensure that
	// getOutputInfo() returns true, and likely also set the info->numSamples to how many
	// samples you want to generate for this CHOP. Otherwise it'll take on length of the
	// input CHOP, which may be timesliced.
	ginfo->timeslice = false;

	ginfo->inputMatchIndex = 0;
}

bool
SenselMorphCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{

	if (reconnectSensel)
	{
		getSensels();
		reconnectSensel = false;
	}
	//frame = NULL;
	skippedFrames = -1;

	if (mySenselHandle == nullptr)
	{
		return false;
	}






	unsigned int num_frames = 0;

	senselReadSensor(mySenselHandle);

	/*
		I will only output 1 frame at a time. Due to the way TD's CHOPs are designed.
		Outputting multiple frames can be convoluted or not easily digestable by other ops.
	*/
	senselGetNumAvailableFrames(mySenselHandle, &num_frames);

	for (unsigned int i = 0; i < num_frames; i++)
	{
		//I am skipping over all the backed up frames in the buffer.
		senselGetFrame(mySenselHandle, frame);
	}

	// If you need speed, comment out the for loop above, and uncomment the call below.
	//senselGetFrame(mySenselHandle, frame);
	skippedFrames = num_frames - 1;



	info->numChannels = 13;

	info->numSamples = 0;

	for (int i = 0; i < frame->n_contacts; i++)
	{

		if (frame->contacts[i].state != CONTACT_END)
		{
			info->numSamples++;
		}

	}


	//info->numSamples = frame->n_contacts;


	info->sampleRate = 60;

	return true;
}

void
SenselMorphCHOP::getChannelName(int32_t index, OP_String *name, const OP_Inputs* inputs, void* reserved1)
{
	switch (index)
	{
	case 0:
		name->setString("id");
		break;
	case 1:
		name->setString("x_pos");
		break;
	case 2:
		name->setString("y_pos");
		break;
	case 3:
		name->setString("state");
		break;
	case 4:
		name->setString("total_force");
		break;
	case 5:
		name->setString("area");
		break;
	case 6:
		name->setString("box_min_x");
		break;
	case 7:
		name->setString("box_min_y");
		break;
	case 8:
		name->setString("box_max_x");
		break;
	case 9:
		name->setString("box_max_y");
		break;
	case 10:
		name->setString("peak_force");
		break;
	case 11:
		name->setString("peak_x");
		break;
	case 12:
		name->setString("peak_y");
		break;

	/*
	case 13:
		name->setString("delta_x");
		break;
	case 14:
		name->setString("delta_y");
		break;
	case 15:
		name->setString("delta_force");
		break;
	case 16:
		name->setString("delta_area");
		break;
	*/
	}
}

void
SenselMorphCHOP::execute(CHOP_Output* output,
							  const OP_Inputs* inputs,
							  void* reserved)
{
	

	if ((mySenselHandle == nullptr) || (frame == nullptr))
		return;
	
	if (frame->n_contacts == 0)
	{

		for (int i = 0; i < 13; i++)
		{
			output->channels[i][0] = 0;
		}
		return;

	}


	for (int c = 0; c < frame->n_contacts; c++)
	{


		if (frame->contacts[c].state == CONTACT_START)
		{
			senselSetLEDBrightness(mySenselHandle, frame->contacts[c].id, 100);
		}
		else if (frame->contacts[c].state == CONTACT_END)
		{
			senselSetLEDBrightness(mySenselHandle, frame->contacts[c].id, 0);
			continue;
		}


		output->channels[0][c] = frame->contacts[c].id;
		output->channels[1][c] = frame->contacts[c].x_pos;
		output->channels[2][c] = frame->contacts[c].y_pos;
		output->channels[3][c] = static_cast<float>(frame->contacts[c].state);



		output->channels[4][c] = frame->contacts[c].total_force;

		output->channels[5][c] = frame->contacts[c].area;
		output->channels[6][c] = frame->contacts[c].min_x;
		output->channels[7][c] = frame->contacts[c].min_y;
		output->channels[8][c] = frame->contacts[c].max_x;
		output->channels[9][c] = frame->contacts[c].max_y;
		output->channels[10][c] = frame->contacts[c].peak_force;
		output->channels[11][c] = frame->contacts[c].peak_x;
		output->channels[12][c] = frame->contacts[c].peak_y;

		/*
		output->channels[13][c] = frame->contacts[c].delta_x;
		output->channels[14][c] = frame->contacts[c].delta_y;
		output->channels[15][c] = frame->contacts[c].delta_force;
		output->channels[16][c] = frame->contacts[c].delta_area;
		*/
	}
}

int32_t
SenselMorphCHOP::getNumInfoCHOPChans(void * reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 1;

}

void
SenselMorphCHOP::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan,
										void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	chan->name->setString("skippedFrames");
	chan->value = static_cast<float>(skippedFrames);
}

bool		
SenselMorphCHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	if (mySenselHandle != nullptr)
	{
		infoSize->rows = 9;
		infoSize->cols = 2;
		infoSize->byColumn = false;
	}
	else
	{
		infoSize->rows = 0;
		infoSize->cols = 0;

	}

	return true;
}

void
SenselMorphCHOP::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries, 
										void* reserved1)
{
	char tempBuffer[4096];

	if (mySenselList.num_devices < 1)
	{
		return;
	}


	switch (index)
	{
	case 0:
		entries->values[0]->setString("device_id");
		sprintf_s(tempBuffer, "%d", mySenselList.devices[0].idx);
		entries->values[1]->setString(tempBuffer);
		break;

	case 1:
		entries->values[0]->setString("serial_num");
		sprintf_s(tempBuffer, "%s", mySenselList.devices[0].serial_num);
		entries->values[1]->setString(tempBuffer);
		break;

	case 2:
		entries->values[0]->setString("firmware_major");
		sprintf_s(tempBuffer, "%d", myFirmwareInfo.fw_version_major);
		entries->values[1]->setString(tempBuffer);
		break;

	case 3:
		entries->values[0]->setString("firmware_minor");
		sprintf_s(tempBuffer, "%d", myFirmwareInfo.fw_version_minor);
		entries->values[1]->setString(tempBuffer);
		break;

	case 4:
		entries->values[0]->setString("firmware_build");
		sprintf_s(tempBuffer, "%d", myFirmwareInfo.fw_version_build);
		entries->values[1]->setString(tempBuffer);
		break;

	case 5:
		entries->values[0]->setString("width");
		sprintf_s(tempBuffer, "%f", mySenselInfo.width);
		entries->values[1]->setString(tempBuffer);
		break;

	case 6:
		entries->values[0]->setString("height");
		sprintf_s(tempBuffer, "%f", mySenselInfo.height);
		entries->values[1]->setString(tempBuffer);
		break;

	case 7:
		entries->values[0]->setString("cols");
		sprintf_s(tempBuffer, "%d", mySenselInfo.num_cols);
		entries->values[1]->setString(tempBuffer);
		break;

	case 8:
		entries->values[0]->setString("rows");
		sprintf_s(tempBuffer, "%d", mySenselInfo.num_rows);
		entries->values[1]->setString(tempBuffer);
		break;

	}
}

void
SenselMorphCHOP::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
	{
		OP_NumericParameter np;
		np.name = "Detect";
		np.label = "Detect Sensels";
		np.page = "Sensel";
	
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void 
SenselMorphCHOP::pulsePressed(const char* name, void* reserved1)
{
	if (!strcmp(name, "Detect"))
	{
		getSensels();
	}
}

