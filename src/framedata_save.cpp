#include "framedata.h"
#include <fstream>
#include <cstdint>
#include <cstring>

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

// The order of these things is a bit different from the order the original game files use.
// I don't know if it can cause trouble but it's something to keep in mind.

void WriteAF(std::ofstream &file, const Frame_AF *af)
{
	file.write("AFST", 4);

	for(int i = 0; i < af->layers.size(); i++)
	{
		const auto &l = af->layers[i];
		file.write("AFGX", 4);
		file.write(VAL(i), 4);
		uint32_t pat = l.usePat;
		file.write(VAL(pat), 4);
		file.write(VAL(l.spriteId), 4);

		if(l.offset_x || l.offset_y){
			file.write("AFOF", 4);
			file.write(VAL(l.offset_x), 4);
			file.write(VAL(l.offset_y), 4);
		}

		if(l.priority){
			file.write("AFPL", 4);
			file.write(VAL(l.priority), 4);
		}

		if(l.blend_mode){
			file.write("AFAL", 4);
			int anormalized = l.rgba[3]*255.;
			file.write(VAL(l.blend_mode), 4);
			file.write(VAL(anormalized), 4);
		}
		else if (l.rgba[3] != 1.f){
			int type = 1;
			int anormalized = l.rgba[3]*255.f;
			file.write("AFAL", 4);
			file.write(VAL(type), 4);
			file.write(VAL(anormalized), 4);
		}

		if(	l.rgba[0] != 1.f ||
			l.rgba[1] != 1.f ||
			l.rgba[2] != 1.f)
		{
			int anormalized[3];
			for(int i = 0; i < 3; i++)
				anormalized[i] = l.rgba[i]*255.f;
			file.write("AFRG", 4);
			file.write(PTR(anormalized), 3*sizeof(float));
		}
		if(l.rotation[2]){
			file.write("AFAZ", 4);
			file.write(VAL(l.rotation[2]), sizeof(float));
		}
		if(l.rotation[1]){
			file.write("AFAY", 4);
			file.write(VAL(l.rotation[1]), sizeof(float));
		}
		if(l.rotation[0]){
			file.write("AFAX", 4);
			file.write(VAL(l.rotation[0]), sizeof(float));
		}
		
		if(	l.scale[0] != 1.f ||
			l.scale[1] != 1.f){
			file.write("AFZM", 4);
			file.write(PTR(l.scale), 2*sizeof(float));
		}
	}

	if(af->duration > 0 && af->duration < 10){
		char t = af->duration + '0';
		file.write("AFD", 3);
		file.write(VAL(t), 1);
	}
	else{
		file.write("AFDL", 4);
		file.write(VAL(af->duration), 4);
	}

	if(af->aniType){
		if(af->aniType <= 2)
		{
			char t = af->aniType + '0';
			file.write("AFF", 3);
			file.write(VAL(t), 1);
		}
		else
		{
			file.write("AFFL", 4);
			file.write(VAL(af->aniType), 4);
		}
	}
	if(af->aniFlag){
		file.write("AFFE", 4);
		file.write(VAL(af->aniFlag), 4);
	}
	if(af->interpolationType){
		file.write("AFHK", 4);
		file.write(VAL(af->interpolationType), 4);
	}
	if(af->jump){
		file.write("AFJP", 4);
		file.write(VAL(af->jump), 4);
	}
	if(af->frameId){
		file.write("AFID", 4);
		file.write(VAL(af->frameId), 4);
	}
	if(af->afjh){
		file.write("AFJH", 4);
		int AFJH = af->afjh;
		file.write(VAL(AFJH), 4);
	}
	if(memcmp(af->param, "\0\0\0\0", 4))
	{
		file.write("AFPA", 4);
		file.write(PTR(af->param), 4);
	}
	if(af->priority){
		file.write("AFPR", 4);
		file.write(VAL(af->priority), 4);
	}
	if(af->loopCount){
		file.write("AFCT", 4);
		file.write(VAL(af->loopCount), 4);
	}
	if(af->loopEnd){
		file.write("AFLP", 4);
		file.write(VAL(af->loopEnd), 4);
	}
	if(af->landJump){
		file.write("AFJC", 4);
		file.write(VAL(af->landJump), 4);
	}
	if(af->AFRT){
		int val = af->AFRT;
		file.write("AFRT", 4);
		file.write(VAL(val), 4);
	}
	
	
	file.write("AFED", 4);
}

void WriteAS(std::ofstream &file, const Frame_AS *as)
{
	file.write("ASST", 4);

	if((as->movementFlags & 0x11) == 0x11 && 
		as->speed[0] == 0 &&
		as->speed[1] == 0 &&
		as->accel[0] == 0 &&
		as->accel[1] == 0
	){
		file.write("ASVX", 4);
	}
	else if(as->movementFlags != 0 || 
		as->speed[0] != 0 ||
		as->speed[1] != 0 ||
		as->accel[0] != 0 ||
		as->accel[1] != 0
	){
		file.write("ASV0", 4);
		file.write(VAL(as->movementFlags), 4);
		file.write(PTR(as->speed), 2*4);
		file.write(PTR(as->accel), 2*4);
	}

	if(as->canMove){
		int val = as->canMove;
		file.write("ASMV", 4);
		file.write(VAL(val), 4);
	}
	if(as->stanceState){
		char t = as->stanceState + '0';
		file.write("ASS", 3); //lmaop
		file.write(VAL(t), 1);
	}
	if(as->hitsNumber){
		file.write("ASAA", 4);
		file.write(VAL(as->hitsNumber), 4);
	}
	if(as->cancelNormal){
		file.write("ASCN", 4);
		file.write(VAL(as->cancelNormal), 4);
	}
	if(as->cancelSpecial){
		file.write("ASCS", 4);
		file.write(VAL(as->cancelSpecial), 4);
	}
	if(as->counterType){
		file.write("ASCT", 4);
		file.write(VAL(as->counterType), 4);
	}
	if(as->ascf)
	{
		file.write("ASCF", 4);
		file.write(VAL(as->ascf), 4);
	}
	if(as->statusFlags[0])
	{
		file.write("ASF0", 4);
		file.write(VAL(as->statusFlags[0]), 4);
	}
	if(as->statusFlags[1])
	{
		file.write("ASF1", 4);
		file.write(VAL(as->statusFlags[1]), 4);
	}
	if(as->maxSpeedX){
		file.write("ASMX", 4);
		file.write(VAL(as->maxSpeedX), 4);
	}
	if(as->sineFlags)
	{
		file.write("AST0", 4);
		file.write(VAL(as->sineFlags), 4);
		file.write(PTR(as->sineParameters), 4*4);
		file.write(PTR(as->sinePhases), 2*sizeof(float));
	}
	
	if(as->invincibility){
		file.write("ASYS", 4);
		file.write(VAL(as->invincibility), 4);
	}


	file.write("ASED", 4);
}


void WriteAT(std::ofstream &file, const Frame_AT *at)
{
	file.write("ATST", 4);

	if(at->guard_flags){
		file.write("ATGD", 4);
		file.write(VAL(at->guard_flags), 4);
	}
	{ //Always
		constexpr int sizes[2] = {3,2};
		file.write("ATV2", 4);
		file.write(PTR(sizes), 2*4);
		for(int i = 0; i < 3; i++)
		{
			file.write(VAL(at->hVFlags[i]), 4);
			file.write(VAL(at->hitVector[i]), 4);
			file.write(VAL(at->gVFlags[i]), 4);
			file.write(VAL(at->guardVector[i]), 4);
		}
	}
	if(at->hitEffect || at->soundEffect){
		file.write("ATHE", 4);
		file.write(VAL(at->hitEffect), 4);
		file.write(VAL(at->soundEffect), 4);
	}
	if(at->hitgrab){
		file.write("ATNG", 4);
		file.write(VAL(at->hitgrab), 4);
	}
	if(at->hitStop){
		file.write("ATSP", 4);
		file.write(VAL(at->hitStop), 4);
	}
	if(at->untechTime){
		file.write("ATSU", 4);
		file.write(VAL(at->untechTime), 4);
	}
	if(at->correction != 100){
		file.write("ATHS", 4);
		file.write(VAL(at->correction), 4);
	}
	if(at->correction_type){
		file.write("ATHT", 4);
		file.write(VAL(at->correction_type), 4);
	}
	if(at->correction2 != 100){ //This might be wrong.
		file.write("ATHH", 4);
		file.write(VAL(at->correction2), 4);
	}
	if(at->otherFlags){
		file.write("ATF1", 4);
		file.write(VAL(at->otherFlags), 4);
	}
	if (at->damage) {
		file.write("ATAT", 4);
		file.write(VAL(at->damage), 4);
	}
	if (at->minDamage) {
		file.write("ATAM", 4);
		file.write(VAL(at->minDamage), 4);
	}
	if (at->meter_gain) {
		file.write("ATCA", 4);
		file.write(VAL(at->meter_gain), 4);
	}
	if (at->hitStunDecay[0] != 0 ||
		at->hitStunDecay[1] != 0 ||
		at->hitStunDecay[2] != 0) {
		file.write("ATC0", 4);
		file.write(PTR(at->hitStunDecay), 3*4);
	}
	if(at->addedEffect){
		file.write("ATKK", 4);
		file.write(VAL(at->addedEffect), 4);
	}
	
	if(at->extraGravity){
		file.write("ATUH", 4);
		file.write(VAL(at->extraGravity), 4);
	}
	if(at->breakTime){
		file.write("ATBT", 4);
		file.write(VAL(at->breakTime), 4);
	}
	if(at->hitStopTime){
		file.write("ATSN", 4);
		file.write(VAL(at->hitStopTime), 4);
	}
	
	if(at->addHitStun){
		file.write("ATSA", 4);
		file.write(VAL(at->addHitStun), 4);
	}
	if(at->hitStun){
		file.write("ATSH", 4);
		file.write(VAL(at->hitStun), 4);
	}
	if(at->blockStopTime){
		file.write("ATGN", 4);
		file.write(VAL(at->blockStopTime), 4);
	}
	file.write("ATED", 4);
}

void WriteEF(std::ofstream &file, const std::vector<Frame_EF> &ef)
{
	constexpr size_t maxParam = 12;
	for(int i = 0; i < ef.size(); i++)
	{
		int paramN = 0;
		for(int j = 0; j < maxParam; j++)
		{
			if(ef[i].parameters[j])
				paramN = j+1;
		}
		file.write("EFST", 4);
		file.write(VAL(i), 4);
		file.write("EFTP", 4);
		file.write(VAL(ef[i].type), 4);
		file.write("EFNO", 4);
		file.write(VAL(ef[i].number), 4);
		if(paramN)
		{
			file.write("EFPR", 4);
			file.write(VAL(paramN), 4);
			file.write(PTR(ef[i].parameters), paramN*4);
		}
		file.write("EFED", 4);
	}
}

void WriteIF(std::ofstream &file, const std::vector<Frame_IF> &ifs)
{
	constexpr size_t maxParam = 9;
	for(int i = 0; i < ifs.size(); i++)
	{
		int paramN = 0;
		for(int j = 0; j < maxParam; j++)
		{
			if(ifs[i].parameters[j])
				paramN = j+1;
		}
		file.write("IFST", 4);
		file.write(VAL(i), 4);
		file.write("IFTP", 4);
		file.write(VAL(ifs[i].type), 4);
		if(paramN)
		{
			file.write("IFPR", 4);
			file.write(VAL(paramN), 4);
			file.write(PTR(ifs[i].parameters), paramN*4);
		}
		file.write("IFED", 4);
	}
}

struct PatInfo
{
	std::vector<const int*> boxList;
	std::vector<const Frame_AS*> asList;

	//out info
	int totalBoxes = 0;
	int totalAses = 0;
	int totalAts = 0;
};

void WriteFrame(std::ofstream &file, const Frame *frame, PatInfo &patInfo)
{
	file.write("FSTR", 4);
	WriteAF(file, &frame->AF);

	int dupeAsIndex = -1;
	for(int i = 0; i < patInfo.asList.size(); ++i)
	{
		if(!memcmp(&frame->AS, patInfo.asList[i], sizeof(Frame_AS)))
		{
			dupeAsIndex = i;
			break;
		}
	}
	if(dupeAsIndex >= 0)
	{
		file.write("ASSM", 4);
		file.write(VAL(dupeAsIndex), 4);
	}
	else
	{
		WriteAS(file, &frame->AS);
		patInfo.asList.push_back(&frame->AS);
		patInfo.totalAses += 1;
	}


	bool hasAt = false;
	if(!frame->hitboxes.empty())
	{
		auto maxhurt = frame->hitboxes.lower_bound(25);
		if(maxhurt != frame->hitboxes.begin())
			--maxhurt;

		if(maxhurt->first < 25)
		{
			int val = maxhurt->first+1;
			file.write("FSNH", 4);
			file.write(VAL(val), 4);
		}

		auto maxhit = --(frame->hitboxes.end());
		if(maxhit->first >= 25)
		{
			int val = maxhit->first-25+1;
			file.write("FSNA", 4);
			file.write(VAL(val), 4);
			hasAt = true;
		}
	}

	if(!frame->EF.empty())
	{
		int val = frame->EF.size();
		file.write("FSNE", 4);
		file.write(VAL(val), 4);
	}
	if(!frame->IF.empty())
	{
		int val = frame->IF.size();
		file.write("FSNI", 4);
		file.write(VAL(val), 4);
	}

	if(hasAt)
	{
		WriteAT(file, &frame->AT);
		patInfo.totalAts += 1;
	}

	for(const auto& box : frame->hitboxes)
	{
		int index = box.first;
		
		int dupeIndex = -1;
		for(int i = patInfo.boxList.size()-1; i >= 0; --i)
		{
			if(!memcmp(box.second.xy, patInfo.boxList[i], sizeof(int)*4))
			{
				dupeIndex = i;
				break;
			}
		}
		
		if(box.first >= 25)
		{
			index -= 25;
			if(dupeIndex >= 0)
				file.write("HRAS", 4);
			else
			{
				file.write("HRAT", 4);
				//patInfo.attackList.push_back(box.second.xy);
			}
		}
		else
		{
			if(dupeIndex >= 0)
				file.write("HRNS", 4);
			else
			{
				file.write("HRNM", 4);
				//patInfo.hurtList.push_back(box.second.xy);
			}
		}

		if(dupeIndex >= 0)
		{
			file.write(VAL(index), 4);
			file.write(VAL(dupeIndex), 4);
		}
		else
		{
			file.write(VAL(index), 4);
			file.write(PTR(box.second.xy), 4*4);
			patInfo.boxList.push_back(box.second.xy);
			patInfo.totalBoxes += 1;
		}
	}
	
	WriteEF(file, frame->EF);
	WriteIF(file, frame->IF);

	file.write("FEND", 4);
}

void WriteSequence(std::ofstream &file, const Sequence *seq)
{
	if(seq->psts){
		file.write("PSTS", 4);
		file.write(VAL(seq->psts), 4);
	}
	if(seq->level){
		file.write("PLVL", 4);
		file.write(VAL(seq->level), 4);
	}
	if(seq->flag){
		file.write("PFLG", 4);
		file.write(VAL(seq->flag), 4);
	}
	if(seq->pups){
		file.write("PUPS", 4);
		file.write(VAL(seq->pups), 4);
	}
	if(!seq->name.empty()){
		char buf[32]{};
		uint32_t size = 32;
		strncpy(buf, seq->name.c_str(), 32);
		buf[31] = 0;
		file.write("PTT2", 4);
		file.write(VAL(size), 4);
		file.write(PTR(buf), 32);
	}
	if(!seq->codeName.empty()){
		uint32_t size = seq->codeName.size()+1;
		file.write("PTCN", 4);
		file.write(VAL(size), 4);
		file.write(PTR(seq->codeName.data()), size);
	}

	if(!seq->frames.empty())
	{
		uint32_t pds2[8]{};
		pds2[0] = pds2[7] = seq->frames.size();
		for(const auto& frame : seq->frames)
		{
			pds2[2] += frame.EF.size();
			pds2[3] += frame.IF.size();
		}

		uint32_t pds2Size = sizeof(pds2);

		file.write("PDS2", 4);
		file.write(VAL(pds2Size), 4);
		auto dataBlockPos = file.tellp();
		file.write(PTR(pds2), pds2Size);

		PatInfo info{};
		for(const auto& frame : seq->frames)
		{
			WriteFrame(file, &frame, info);
		}
		pds2[1] = info.totalBoxes;
		pds2[6] = info.totalAses;
		pds2[4] = info.totalAts;

		auto curPos = file.tellp();
		file.seekp(dataBlockPos);
		file.write(PTR(pds2), pds2Size);
		file.seekp(curPos);
	}
}