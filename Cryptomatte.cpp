// Copyright (c) 2012 The Foundry Visionmongers Ltd.  All Rights Reserved.

// Standard plug-in include files.
#include "DDImage/PixelIop.h"
#include "DDImage/Row.h"
#include "DDImage/Knobs.h"
#include "DDImage/NukeWrapper.h"
#include "DDImage/DDMath.h"
#include "DDImage/Pixel.h"
#include "DDImage/Metadata.h"
#include "DDImage/Enumeration_KnobI.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <map>
#include <utility>
#include <algorithm>

using namespace DD::Image;

typedef std::map<std::string, std::map<std::string, std::string>> matte_info;

const int ADD = 0;
const int RM = 1;

static const char* const CLASS = "CryptomatteCpp";
static const char* const HELP = "cpp Cryptomatte node";
static const char* const MODE_NAMES[] = {
	"Colors", "Edges", "None", NULL
};
static const char* const LAYER_NAMES[] = {
	NULL
};

class CryptomatteIop : public Iop
{
	bool enable_preview;
	int	 preview_mode;
	bool matte_only;
	bool remove_channels;
	bool enable_unpremultiply;
	bool stop_auto_update;
	int	 crypto_layer_index;
	const char *crypto_layer_name;
	bool crypto_layer_lock;
	float add_id_sample[8];
	float rm_id_sample[8];
	bool single_selection;
	const char *matte_list_string;
	const char *id_string;
	int  id_cnt;
	
	std::set<std::string> matte_list;
	std::vector<float> id_list;
	std::map<std::string, std::string> cryptomattes;
	std::map<std::string, float> name_to_id;
	std::map<float, std::string> id_to_name;

	Knob *cryptolayer_knob;
	Knob *layername_knob;
	Knob *matteliststring_knob;
	Knob *idstring_knob;
	Knob *idcnt_knob;

public:

  //! Constructor. Initialize user controls to their default values.
	CryptomatteIop(Node *node) :
		Iop(node),
		enable_preview(true),
		preview_mode(0),
		matte_only(false),
		remove_channels(false),
		enable_unpremultiply(false),
		stop_auto_update(false),
		crypto_layer_index(0),
		crypto_layer_name(""),
		crypto_layer_lock(false),
		single_selection(false),
		matte_list_string(""),
		id_string(""),
		id_cnt(0),
		cryptolayer_knob(nullptr),
		layername_knob(nullptr),
		matteliststring_knob(nullptr),
		idstring_knob(nullptr),
		idcnt_knob(nullptr)
	{

	}

	~CryptomatteIop () {}
  
	void engine(int y, int x, int r, ChannelMask channels, Row &row);
	virtual void knobs(Knob_Callback f);
	virtual int knob_changed(Knob *k);

	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }
	static const Iop::Description description;
	void _validate(bool for_real);
	void _request(int x, int y, int r, int t, ChannelMask channels, int count);
	int maximum_inputs() const { return 1; }
	int minimum_inputs() const { return 1; }

	void updateMapping();
	void updatePicker(int mode);
	std::vector<std::string> fetchMetadata();
	void updateLayers();
	void defaultOutput(Row& row, int x, int r, Row& out);
}; 

void CryptomatteIop::_validate(bool for_real)
{
	copy_info();
	if (strlen(crypto_layer_name) != 0)
	{
		set_out_channels(Mask_All);
		info_.turn_on(Mask_All);
	}
	else
		set_out_channels(Mask_None);
}

void CryptomatteIop::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{
	input(0)->request(x, y, r, t, input0().channels(), count);
}

inline static float mantissa(float v)
{
	int n;
	return frexp(v, &n);
}

inline static float preview_expr(float v1, float v2, float v3)
{
	return fmod(mantissa(fabs(v1)) * v2, 0.25f) * v3;
}

void CryptomatteIop::engine(int y, int x, int r, ChannelMask channels, Row& out)
{
	ChannelMask chlsetall = input0().channels();
	Row row(x, r);
	row.get(input0(), y, x, r, chlsetall);

	std::string crypto_layer = crypto_layer_name;
	if (crypto_layer.size() == 0)
	{
		defaultOutput(row, x, r, out);
		return;
	}

	Channel r00_chan = findChannel((crypto_layer + "00.red").c_str());
	Channel g00_chan = findChannel((crypto_layer + "00.green").c_str());
	Channel b00_chan = findChannel((crypto_layer + "00.blue").c_str());
	Channel a00_chan = findChannel((crypto_layer + "00.alpha").c_str());
	Channel r01_chan = findChannel((crypto_layer + "01.red").c_str());
	Channel g01_chan = findChannel((crypto_layer + "01.green").c_str());
	Channel b01_chan = findChannel((crypto_layer + "01.blue").c_str());
	Channel a01_chan = findChannel((crypto_layer + "01.alpha").c_str());
	Channel r02_chan = findChannel((crypto_layer + "02.red").c_str());
	Channel g02_chan = findChannel((crypto_layer + "02.green").c_str());
	Channel b02_chan = findChannel((crypto_layer + "02.blue").c_str());
	Channel a02_chan = findChannel((crypto_layer + "02.alpha").c_str());
	
	const float *r00_in = row[r00_chan];
	const float *g00_in = row[g00_chan];
	const float *b00_in = row[b00_chan];
	const float *a00_in = row[a00_chan];
	const float *r01_in = row[r01_chan];
	const float *g01_in = row[g01_chan];
	const float *b01_in = row[b01_chan];
	const float *a01_in = row[a01_chan];
	const float *r02_in = row[r02_chan];
	const float *g02_in = row[g02_chan];
	const float *b02_in = row[b02_chan];
	const float *a02_in = row[a02_chan];

	const float *r_in = row[Chan_Red];
	const float *g_in = row[Chan_Green];
	const float *b_in = row[Chan_Blue];
	const float *a_in = row[Chan_Alpha];

	if (!(r00_in && g00_in && b00_in && a00_in && r01_in && g01_in && b01_in && a01_in && r02_in && g02_in && b02_in && a02_in))
	{
		defaultOutput(row, x, r, out);
		return;
	}

	float *r_out = out.writable(Chan_Red);
	float *g_out = out.writable(Chan_Green);
	float *b_out = out.writable(Chan_Blue);
	float *a_out = out.writable(Chan_Alpha);

	for (int i = x; i < r; i++)
	{
		float r00 = *(r00_in + i);
		float g00 = *(g00_in + i);
		float b00 = *(b00_in + i);
		float a00 = *(a00_in + i);
		float r01 = *(r01_in + i);
		float g01 = *(g01_in + i);
		float b01 = *(b01_in + i);
		float a01 = *(a01_in + i);
		float r02 = *(r02_in + i);
		float g02 = *(g02_in + i);
		float b02 = *(b02_in + i);
		float a02 = *(a02_in + i);
		float r   = *(r_in + i);
		float g   = *(g_in + i);
		float b   = *(b_in + i);
		float a   = *(a_in + i);

		float shuffle_r = preview_expr(r00, 1.f, g00) + preview_expr(b00, 1.f, a00) + preview_expr(r01, 1.f, g01) + preview_expr(b01, 1.f, a01);
		float shuffle_g = preview_expr(r00, 16.f, g00) + preview_expr(b00, 16.f, a00) + preview_expr(r01, 16.f, g01) + preview_expr(b01, 16.f, a01);
		float shuffle_b = preview_expr(r00, 64.f, g00) + preview_expr(b00, 64.f, a00) + preview_expr(r01, 64.f, g01) + preview_expr(b01, 64.f, a01);
		float shuffle_a = 0.f;

		float shuffle_mask = 0.f;
		float ids[6] = { r00, b00, r01, b01, r02, b02 };
		bool  acc[6] = { false, false, false, false, false, false };
		float convs[6] = { g00, a00, g01, a01, g02, a02 };

		for (int j = 0; j < 6; j++)
		{
			float *id_ptr = (float*)id_string;
			for (int k = 0; k < id_cnt; k++)
			{
				if (ids[j] == id_ptr[k])
				{
					acc[j] = true;
					break;
				}
			}
		}
		for (int j = 0; j < 6; j++)
			if (acc[j])
				shuffle_mask += convs[j];

		float shuffle_blackalpha[4] = { shuffle_r, shuffle_g, shuffle_b, 0.f };
		float mask_r = enable_unpremultiply ? shuffle_mask / a : shuffle_mask;
		shuffle_blackalpha[0] = shuffle_mask + (1 - shuffle_mask) * shuffle_r;
		shuffle_blackalpha[1] = shuffle_mask + (1 - shuffle_mask) * shuffle_g;
		shuffle_blackalpha[2] = shuffle_b;
		shuffle_blackalpha[3] = a;

		float output[4];
		if (matte_only)
		{
			float tmp = mask_r;
			output[0] = tmp;
			output[1] = tmp;
			output[2] = tmp;
		}
		else
		{
			if (enable_preview)
			{
				output[0] = shuffle_blackalpha[0];
				output[1] = shuffle_blackalpha[1];
				output[2] = shuffle_blackalpha[2];
			}
			else
			{
				output[0] = r;
				output[1] = g;
				output[2] = b;
			}
		}
		output[3] = mask_r;

		*(r_out + i) = output[0];
		*(g_out + i) = output[1];
		*(b_out + i) = output[2];
		*(a_out + i) = output[3];
	}


	foreach (z, chlsetall)
	{
		if (z == Chan_Red || z == Chan_Green || z == Chan_Blue || z == Chan_Alpha)
			continue;
		const float *inptr = row[z] + x;
		const float *end = inptr + (r - x);
		float *outptr = out.writable(z) + x;
		while (inptr < end)
			*outptr++ = *inptr++;
	}
}

void CryptomatteIop::knobs(Knob_Callback f)
{
	Knob *k;

	k = Eyedropper_knob(f, add_id_sample, "pickeradd", "Picker Add");
	k = Text_knob(f, "Picker Add");
	ClearFlags(f, Knob::STARTLINE);

	k = Eyedropper_knob(f, rm_id_sample, "pickerrm", "Picker Remove");	
	k = Text_knob(f, "Picker Remove");
	ClearFlags(f, Knob::STARTLINE);

	k = Spacer(f, 3);
	ClearFlags(f, Knob::STARTLINE);
	
	k = Bool_knob(f, &enable_preview, "preview");
	SetFlags(f, Knob::STARTLINE);
	SetFlags(f, Knob::ALWAYS_SAVE);
	k = Enumeration_knob(f, &preview_mode, MODE_NAMES, "Preview");
	ClearFlags(f, Knob::STARTLINE);
	SetFlags(f, Knob::ALWAYS_SAVE);

	k = Bool_knob(f, &matte_only, "matteonly", "Matte Only");
	SetFlags(f, Knob::STARTLINE);
	SetFlags(f, Knob::ALWAYS_SAVE);
	k = Bool_knob(f, &single_selection, "singleselection", "Single Selection");
	SetFlags(f, Knob::ALWAYS_SAVE);
	k = Bool_knob(f, &remove_channels, "removechannels", "Remove Channels");
	SetFlags(f, Knob::ALWAYS_SAVE);
	k = Bool_knob(f, &enable_unpremultiply, "unpremultiply", "Unpremultiply");
	SetFlags(f, Knob::ALWAYS_SAVE);
	SetFlags(f, Knob::STARTLINE);

	matteliststring_knob = String_knob(f, &matte_list_string, "MatteListString", "Matte List");
	SetFlags(f, Knob::ALWAYS_SAVE);
	SetFlags(f, Knob::STARTLINE);
	idstring_knob = String_knob(f, &id_string, "IdString");
	SetFlags(f, Knob::ALWAYS_SAVE);
	SetFlags(f, Knob::INVISIBLE);
	idcnt_knob = Int_knob(f, &id_cnt, "IdCnt");
	SetFlags(f, Knob::ALWAYS_SAVE);
	SetFlags(f, Knob::INVISIBLE);

	k = Spacer(f, 3);
	SetFlags(f, Knob::STARTLINE);
	
	k = Button(f, "Clear");
	ClearFlags(f, Knob::STARTLINE);
	k = Button(f, "Force Update");

	k = Spacer(f, 3);
	SetFlags(f, Knob::STARTLINE);

	cryptolayer_knob = Enumeration_knob(f, &crypto_layer_index, LAYER_NAMES, "cryptoLayerChoice", "Layer Selection");
	ClearFlags(f, Knob::STARTLINE);
	layername_knob = String_knob(f, &crypto_layer_name, "cryptoLayerName");
	SetFlags(f, Knob::INVISIBLE);
	SetFlags(f, Knob::ALWAYS_SAVE);
	k = Bool_knob(f, &crypto_layer_lock, "cryptoLayerLock", "Lock Layer Selection");
	SetFlags(f, Knob::ALWAYS_SAVE);
}

int CryptomatteIop::knob_changed(Knob *k)
{
	if (k->name() == std::string("inputChange"))
	{
		updateLayers();
		layername_knob->set_text(cryptolayer_knob->enumerationKnob()->getSelectedItemString().c_str());
		return true;
	}

	if (k->is("pickeradd"))
	{
		updatePicker(ADD);
		return true;
	}

	if (k->is("pickerrm"))
	{
		updatePicker(RM);
		return true;
	}

	if (k->is("cryptoLayerChoice"))
	{
		updateMapping();
		layername_knob->set_text(cryptolayer_knob->enumerationKnob()->getSelectedItemString().c_str());
		return true;
	}

	if (k->is("cryptoLayerLock"))
	{
		Knob *choice_knob = knob("cryptoLayerChoice");
		if (crypto_layer_lock)
			choice_knob->disable();
		else
			choice_knob->enable();

		return true;
	}

	if (k->is("Clear"))
	{
		id_list.clear();
		idstring_knob->set_text("");
		idcnt_knob->set_value(0);
	}

	if (k->is("MatteListString"))
	{
		std::string tmp(matte_list_string);
		char *ptr;
		ptr = strtok(const_cast<char*>(tmp.c_str()), ",");
	}

	return true;
}

void CryptomatteIop::updateMapping()
{
	Enumeration_KnobI *layerknob = cryptolayer_knob->enumerationKnob();
	std::string curr_layer = layerknob->getSelectedItemString();

	rapidjson::Document d;
	d.Parse(cryptomattes[curr_layer].c_str());
	id_to_name.clear();
	name_to_id.clear();

	for (rapidjson::Value::ConstMemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it)
	{
		std::string name = it->name.GetString();
		float value;
		std::string value_string = d[name.c_str()].GetString();
		memcpy(&value, value_string.c_str(), sizeof(float));
		name_to_id[name] = value;
		id_to_name[value] = name;
	}
}

void CryptomatteIop::updatePicker(int mode)
{
	if (id_list.size() == 0 && id_cnt != 0)
	{
		std::cout << "recover from id string" << std::endl;
		float *id_ptr = (float*)id_string;
		for (int i = 0; i < id_cnt; i++)
			id_list.push_back(id_ptr[i]);
	}

	if (mode == ADD && single_selection)
		id_list.clear();
	Pixel pixel(input0().channels());
	float x, y;
	if (mode == ADD)
	{
		x = add_id_sample[4];
		y = add_id_sample[5];
	}
	else
	{
		x = rm_id_sample[4];
		y = rm_id_sample[5];
	}

	input0().sample(x, y, 1.f, 1.f, pixel);
	float c[4];
	std::string postfix[3] = {"0", "1", "2"};

	std::string crypto_layer = layername_knob->get_text();
	for (int i = 0; i < 3; i++)
	{
		std::string layer_name = crypto_layer + "0" + postfix[i];
		c[0] = pixel.chan[findChannel((layer_name + ".red").c_str())];
		c[1] = pixel.chan[findChannel((layer_name + ".green").c_str())];
		c[2] = pixel.chan[findChannel((layer_name + ".blue").c_str())];
		c[3] = pixel.chan[findChannel((layer_name + ".alpha").c_str())];
		bool saw_bg = false;
		for (int i = 0; i < 2; i++)
		{
			if (c[i << 1] == 0.f || c[(i << 1) + 1] == 0.f)
			{
				if (saw_bg)
				{
					return;
				}
				saw_bg = true;
				continue;
			}
			float id = c[i << 1];
			std::vector<float>::iterator it = std::find(id_list.begin(), id_list.end(), id);
			if (it == id_list.end())
			{
				if (mode == ADD)
				{
					id_list.push_back(id);
					idcnt_knob->set_value(id_cnt + 1);
				}
			}
			else
			{
				if (mode == RM)
				{
					id_list.erase(it);
					idcnt_knob->set_value(id_cnt - 1);
				}
			}
			
			//std::string name = id_to_name[id];
			//if (matte_list.find(name) == matte_list.end())
			//	matte_list.insert(name);
			char *ptr = (char*)&id_list[0];
			std::string dump_string(ptr, id_list.size() * sizeof(float));
			idstring_knob->set_text(dump_string.c_str());

			std::cout << "id cnt is " << id_cnt << std::endl;

			return;
		}
	}
}

std::vector<std::string> CryptomatteIop::fetchMetadata()
{
	std::vector<std::string> names;

	if (input0().valid())
	{
		MetaData::Bundle bdl = _fetchMetaData(NULL);
			
		matte_info tmp;
		for (MetaData::Bundle::iterator it = bdl.begin(); it != bdl.end(); ++it)
		{
			std::string metadata_key(it->first);
				
			if (it->first.find("exr/cryptomatte") != std::string::npos)
			{
				std::string numbered_key(metadata_key.substr(16));
				size_t slash_pos = numbered_key.find('/');
				std::string metadata_id = numbered_key.substr(0, slash_pos);
				std::string partial_key = numbered_key.substr(slash_pos + 1);
				tmp[metadata_id][partial_key] = bdl.getString(metadata_key.c_str());
			}
		}
		
		for (matte_info::iterator it = tmp.begin(); it != tmp.end(); ++it)
		{
			std::string name = it->second[std::string("name")];
			cryptomattes[name] = it->second[std::string("manifest")];
			names.push_back(name);
		}
	}

	return names;
}

void CryptomatteIop::updateLayers()
{
	std::vector<std::string> names = fetchMetadata();
			
	Enumeration_KnobI *layerknob = cryptolayer_knob->enumerationKnob();
	layerknob->menu(names);
	cryptolayer_knob->set_value(0);
	//updateMapping();
}

void CryptomatteIop::defaultOutput(Row& row, int x, int r, Row& out)
{
	ChannelMask chlsetall = input0().channels();
	foreach(z, chlsetall)
	{
		const float *inptr = row[z] + x;
		const float *end = inptr + (r - x);
		float *outptr = out.writable(z) + x;
		while (inptr < end)
			*outptr++ = *inptr++;
	}
}

static Iop* build(Node* node)
{
	return new CryptomatteIop(node);
}

const Iop::Description CryptomatteIop::description("CryptomatteCpp", "Color/Matte/CtyptomatteCpp", build);
