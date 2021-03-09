/****************************************************************************
Copyright (c) 2021 pietrofeng

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************/

#include "SpineExporter.h"
#include <string>
#include <vector>
#include "Json.h"
#include <iostream>
#include <map>
#include <set>

using namespace std;

const int ATTACHMENT_REGION = 0;
const int ATTACHMENT_BOUNDING_BOX = 1;
const int ATTACHMENT_MESH = 2;
const int ATTACHMENT_LINKED_MESH = 3;
const int ATTACHMENT_PATH = 4;
const int ATTACHMENT_POINT = 5;
const int ATTACHMENT_CLIPPING = 6;

const int BLEND_MODE_NORMAL = 0;
const int BLEND_MODE_ADDITIVE = 1;
const int BLEND_MODE_MULTIPLY = 2;
const int BLEND_MODE_SCREEN = 3;

const int CURVE_LINEAR = 0;
const int CURVE_STEPPED = 1;
const int CURVE_BEZIER = 2;

const int BONE_ROTATE = 0;
const int BONE_TRANSLATE = 1;
const int BONE_SCALE = 2;
const int BONE_SHEAR = 3;

const int TRANSFORM_NORMAL = 0;
const int TRANSFORM_ONLY_TRANSLATION = 1;
const int TRANSFORM_NO_ROTATION_OR_REFLECTION = 2;
const int TRANSFORM_NO_SCALE = 3;
const int TRANSFORM_NO_SCALE_OR_REFLECTION = 4;

const int SLOT_ATTACHMENT = 0;
const int SLOT_COLOR = 1;
const int SLOT_TWO_COLOR = 2;

const int PATH_POSITION = 0;
const int PATH_SPACING = 1;
const int PATH_MIX = 2;

const int PATH_POSITION_FIXED = 0;
const int PATH_POSITION_PERCENT = 1;

const int PATH_SPACING_LENGTH = 0;
const int PATH_SPACING_FIXED = 1;
const int PATH_SPACING_PERCENT = 2;

const int PATH_ROTATE_TANGENT = 0;
const int PATH_ROTATE_CHAIN = 1;
const int PATH_ROTATE_CHAIN_SCALE = 2;

static unsigned char *buff_data = nullptr;
static unsigned int buff_pos = 0;

static set<string> all_atlas;

static void push_byte(unsigned char c)
{
	buff_data[buff_pos++] = c;
}

static void push_float(float v)
{
	unsigned char cTemp[4];
	memcpy(cTemp, &v, 4);

	push_byte(cTemp[3]);
	push_byte(cTemp[2]);
	push_byte(cTemp[1]);
	push_byte(cTemp[0]);
}

static void push_varint(int value, int optimizePositive)
{
	unsigned int v = value;
	if (!optimizePositive)
		v = (unsigned int)((value << 1) ^ (value >> 31));

	for (int i = 0; i < 5; ++i)
	{
		buff_data[buff_pos++] = (v >> i * 7) & 0x7F;
		if (v >> (i + 1) * 7)
			buff_data[buff_pos - 1] |= 0x80;
		else
			break;
	}
}

static void push_boolen(unsigned char v)
{
	buff_data[buff_pos++] = v;
}

static void push_string(const char *str)
{
	if (!str)
	{
		push_varint(0, 1);
		return;
	}
	int len = strlen(str);
	push_varint(len+1, 1);
	memcpy(buff_data + buff_pos, str, len);
	buff_pos += len;
}

unsigned char hex_value(const char c)
{
	unsigned char res = 0;
	switch (c)
	{
	case 'a':
	case 'A': res = 10; break;
	case 'b':
	case 'B': res = 11; break;
	case 'c':
	case 'C': res = 12; break;
	case 'd':
	case 'D': res = 13; break;
	case 'e':
	case 'E': res = 14; break;
	case 'f':
	case 'F': res = 15; break;
	default: res = c - '0'; break;
	}
	return res;
}

static void push_color(const char * color)
{
	if (color)
	{
		push_byte(hex_value(color[0]) << 4 | hex_value(color[1]));
		push_byte(hex_value(color[2]) << 4 | hex_value(color[3]));
		push_byte(hex_value(color[4]) << 4 | hex_value(color[5]));
		push_byte(hex_value(color[6]) << 4 | hex_value(color[7]));
	}
	else
	{
		push_byte(255);
		push_byte(255);
		push_byte(255);
		push_byte(255);
	}
}

struct BoneData {
	string name;
	int parent;
	string parent_name;
	float rotation;
	float x;
	float y;
	float scaleX;
	float scaleY;
	float shearX;
	float shearY;
	float length;
	int mode;
	BoneData() {
		parent = .0f;
		rotation = .0f;
		x = .0f;
		y = .0f;
		scaleX = .0f;
		scaleY = .0f;
		shearX = .0f;
		shearY = .0f;
		length = .0f;
		mode = 0;
	}
};

struct EventData {
	string name;
	int intValue;
	float floatValue;
	string stringValue;
};

static vector<BoneData> process_bones(Json* bones)
{
	vector<BoneData> res;
	for (Json* bone = bones->child; bone; bone = bone->next)
	{
		BoneData bd;
		bd.name = Json_getString(bone, "name", "");
		bd.parent_name = Json_getString(bone, "parent", "");
		bd.rotation = Json_getFloat(bone, "rotation", .0f);
		bd.x = Json_getFloat(bone, "x", .0f);
		bd.y = Json_getFloat(bone, "y", .0f);
		bd.scaleX = Json_getFloat(bone, "scaleX", 1.0f);
		bd.scaleY = Json_getFloat(bone, "scaleY", 1.0f);
		bd.shearX = Json_getFloat(bone, "shearX", .0f);
		bd.shearY = Json_getFloat(bone, "shearY", .0f);
		bd.length = Json_getFloat(bone, "length", .0f);
		const char *transform = Json_getString(bone, "transform", "normal");
		if (strcmp(transform, "normal") == 0)
			bd.mode = 0;
		if (strcmp(transform, "onlyTranslation") == 0)
			bd.mode = 1;
		if (strcmp(transform, "noRotationOrReflection") == 0)
			bd.mode = 2;
		if (strcmp(transform, "noScale") == 0)
			bd.mode = 3;
		if (strcmp(transform, "noScaleOrReflection") == 0)
			bd.mode = 4;
		res.push_back(std::move(bd));
	}

	for (size_t i = 0; i < res.size(); ++i)
	{
		if (res[i].parent_name.length() > 0)
		{
			for (size_t j = 0; j < res.size(); ++j)
			{
				if (res[i].parent_name == res[j].name)
				{
					res[i].parent = j;
					break;
				}
			}
		}
	}
	return res;
}

static string get_line(const char *str, int &pos)
{
	int start = -1;
	while (str[pos])
	{
		if (start == -1)
		{
			if (str[pos] == '\r' || str[pos] == '\n')
				pos++;
			else
				start = pos;
		}
		else
		{
			if (str[pos] == '\r' || str[pos] == '\n')
				return string(str+start, pos-start);
			
			pos++;
		}
	}
	if (start != -1)
		return string(str + start, pos - start);
	return "";
}

static void parse_atlas(const char *atlas)
{
	int pos = 0;
	do
	{
		string line = get_line(atlas, pos);
		if (line.length() > 0 && line.find(":") == string::npos)
			all_atlas.insert(line);

	} while (atlas[pos]);
}

static int find_bone(vector<BoneData> &bones, string name)
{
	for (int i = 0; i < bones.size(); ++i)
	{
		if (bones[i].name == name)
		{
			return i;
		}
	}
	return -1;
}

static int find_string(vector<string> vcStr, string str)
{
	for (int i = 0; i < vcStr.size(); ++i)
	{
		if (vcStr[i] == str)
			return i;
	}
	return -1;
}

static void push_vertices(Json *vertices, int verticesLength)
{
	int size = vertices->size;
	if (size <= 0)
		return;
	float *vert = (float *)malloc(sizeof(float)*size);
	
	int i = 0;
	for (Json *entry = vertices->child; entry; entry = entry->next)
		vert[i++] = entry->valueFloat;

	if (verticesLength == size)
	{
		push_boolen(false);
		for (i = 0; i < size; ++i)
			push_float(vert[i]);
	}
	else
	{
		push_boolen(true);
		for (int i = 0; i < size;)
		{
			int boneCount = (int)vert[i++];
			push_varint(boneCount, 1);
			for (int nn = i + boneCount * 4; i < nn; i += 4)
			{
				push_varint((int)vert[i], 1);
				push_float(vert[i + 1]);
				push_float(vert[i + 2]);
				push_float(vert[i + 3]);
			}
		}
	}
	free(vert);
}

static int parse_skin(Json *skin, vector<string> slots)
{
	push_varint(skin->size, 1);
	if (skin->size == 0)
		return 0;

	for (Json *attachments = skin->child; attachments; attachments = attachments->next)
	{
		int slot = find_string(slots, attachments->name);
		if (slot == -1)
			return -1;
		push_varint(slot, 1);

		vector<Json *> validAttachment;
		for (Json *attachment = attachments->child; attachment; attachment = attachment->next)
		{
			if (all_atlas.empty())
				validAttachment.push_back(attachment);
			else
			{
				string typeString = Json_getString(attachment, "type", "region");
				if (typeString == "region" || typeString == "mesh" || typeString == "linkedmesh")
				{
					const char *attachmentName = Json_getString(attachment, "name", attachment->name);
					if (all_atlas.find(string(attachmentName)) != all_atlas.end())
						validAttachment.push_back(attachment);
				}
				else
					validAttachment.push_back(attachment);
			}
		}
		push_varint(validAttachment.size(), 1);

		for (Json *attachment : validAttachment)
		{
			const char *attachmentName = Json_getString(attachment, "name", attachment->name);
			push_string(attachment->name);
			push_string(attachmentName);

			const char* attachmentPath = Json_getString(attachment, "path", NULL);

			string typeString = Json_getString(attachment, "type", "region");
			int spAttachmentType = 0;
			if (typeString == "mesh")
				spAttachmentType = 2; // SP_ATTACHMENT_MESH
			else if (typeString == "linkedmesh")
				spAttachmentType = 3; // SP_ATTACHMENT_LINKED_MESH
			else if (typeString == "boundingbox")
				spAttachmentType = 1; // SP_ATTACHMENT_BOUNDING_BOX
			else if (typeString == "path")
				spAttachmentType = 4; // SP_ATTACHMENT_PATH
			else if (typeString == "clipping")
				spAttachmentType = 6; // SP_ATTACHMENT_CLIPPING
			else
				spAttachmentType = 0; // SP_ATTACHMENT_REGION

			push_byte(spAttachmentType);
			if (spAttachmentType == 0) // SP_ATTACHMENT_REGION
			{
				if (attachmentPath)
					push_string(attachmentPath);
				else
					push_varint(0, 1);
				push_float(Json_getFloat(attachment, "rotation", 0));
				push_float(Json_getFloat(attachment, "x", 0));
				push_float(Json_getFloat(attachment, "y", 0));
				push_float(Json_getFloat(attachment, "scaleX", 1));
				push_float(Json_getFloat(attachment, "scaleY", 1));
				push_float(Json_getFloat(attachment, "width", 32));
				push_float(Json_getFloat(attachment, "height", 32));
				push_color(Json_getString(attachment, "color", 0));
			}
			else if (spAttachmentType == 1) // SP_ATTACHMENT_BOUNDING_BOX
			{
				int vertexCount = Json_getInt(attachment, "vertexCount", 0) << 1;
				push_varint(vertexCount, 1);
				Json *vertices = Json_getItem(attachment, "vertices");
				push_vertices(vertices, vertexCount);
			}
			else if (spAttachmentType == 2) // SP_ATTACHMENT_MESH
			{
				if (attachmentPath)
					push_string(attachmentPath);
				else
					push_varint(0, 1);

				push_color(Json_getString(attachment, "color", 0));

				Json *uvs = Json_getItem(attachment, "uvs");
				int verticesLength = uvs->size;
				push_varint(verticesLength >> 1, 1);

				for (Json *uv = uvs->child; uv; uv = uv->next)
					push_float(uv->valueFloat);

				Json *triangles = Json_getItem(attachment, "triangles");
				push_varint(triangles->size, 1);
				for (Json *triangle = triangles->child; triangle; triangle = triangle->next)
				{
					unsigned short v = triangle->valueInt;
					push_byte(v >> 8);
					push_byte(v & 0xff);
				}

				Json *vertices = Json_getItem(attachment, "vertices");
				push_vertices(vertices, verticesLength);

				push_varint(Json_getInt(attachment, "hull", 0) >> 1, 1);
			}
			else if (spAttachmentType == 3) // SP_ATTACHMENT_LINKED_MESH
			{
				if (attachmentPath)
					push_string(attachmentPath);
				else
					push_varint(0, 1);

				push_color(Json_getString(attachment, "color", 0));

				const char *skin = Json_getString(attachment, "skin", 0);
				if (skin)
					push_string(skin);
				else
					push_varint(0, 1);
				push_string(attachment->valueString);

				int deform = Json_getInt(attachment, "deform", 1);
				push_boolen(deform);
			}
			else if (spAttachmentType == 4) // SP_ATTACHMENT_PATH
			{
				push_boolen(Json_getInt(attachment, "closed", 0));
				push_boolen(Json_getInt(attachment, "constantSpeed", 0));

				int vertexCount = Json_getInt(attachment, "vertexCount", 0);
				push_varint(vertexCount, 1);
				Json *vertices = Json_getItem(attachment, "vertices");
				push_vertices(vertices, vertexCount << 1);

				Json *lengths = Json_getItem(attachment, "lengths");
				for (Json *length = lengths->child; length; length = length->next)
				{
					push_float(length->valueFloat);
				}
			}
			else if (spAttachmentType == 5) // SP_ATTACHMENT_POINT
			{
				push_float(Json_getFloat(attachment, "x", 0));
				push_float(Json_getFloat(attachment, "y", 0));
				push_float(Json_getFloat(attachment, "rotation", 0));
			}
			else if (spAttachmentType == 6) // SP_ATTACHMENT_CLIPPING
			{
				const char* end = Json_getString(attachment, "end", 0);
				if (end && find_string(slots, end) != -1) {
					push_varint(find_string(slots, end), 1);
				}
				else {
					push_varint(0, 1);
				}
				int vertexCount = Json_getInt(attachment, "vertexCount", 0);
				push_varint(vertexCount, 1);
				Json *vertices = Json_getItem(attachment, "vertices");
				push_vertices(vertices, vertexCount<<1);
			}
		}
	}
	return 0;
}

static void push_curve(Json* curve)
{
	if (!curve) {
		push_byte(0);
		return;
	}
		
	if (curve->type == Json_String && string(curve->valueString) == "stepped")
	{
		push_byte(1);
	}
	else if (curve->type == Json_Array)
	{
		push_byte(2);
		Json* child0 = curve->child;
		Json* child1 = child0->next;
		Json* child2 = child1->next;
		Json* child3 = child2->next;
		push_float(child0->valueFloat);
		push_float(child1->valueFloat);
		push_float(child2->valueFloat);
		push_float(child3->valueFloat);
	}
	else
	{
		push_byte(0);
	}
}

static int parse_animation(
	Json *animation, 
	vector<string> &vcSlots, 
	vector<BoneData> &vcBones, 
	vector<string> &vcIK,
	vector<string> &vcTransform,
	vector<string> &vcPaths,
	vector<string> &vcSkins,
	vector<EventData> &vcEvents)
{
	/* Slot timelines. */
	Json* slots = Json_getItem(animation, "slots");
	push_varint(slots ? slots->size : 0, 1);
	for (Json *slotMap = slots ? slots->child : 0; slotMap; slotMap = slotMap->next)
	{
		int slotIndex = find_string(vcSlots, slotMap->name);
		if (slotIndex == -1)
			return -1;
		push_varint(slotIndex, 1);

		push_varint(slotMap->size, 1);
		for (Json *timelineMap = slotMap->child; timelineMap; timelineMap = timelineMap->next)
		{
			string name = timelineMap->name;
			if (name == "attachment")
			{
				push_byte(0);
				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_string(Json_getString(valueMap, "name", ""));
				}
			}
			else if (name == "color")
			{
				push_byte(1);
				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_color(Json_getString(valueMap, "color", 0));
					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
			else if (name == "twoColor")
			{
				push_byte(2);
				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_color(Json_getString(valueMap, "light", 0));
					push_color(Json_getString(valueMap, "dark", 0));
					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
			else
			{
				return -2;
			}
		}
	}

	/* Bone timelines. */
	Json* bones = Json_getItem(animation, "bones");
	push_varint(bones ? bones->size : 0,  1);
	for (Json *boneMap = bones ? bones->child : 0; boneMap; boneMap = boneMap->next)
	{
		int boneIndex = find_bone(vcBones, boneMap->name);
		if (boneIndex == -1)
			return -3;
		push_varint(boneIndex, 1);

		push_varint(boneMap->size, 1);
		for (Json *timelineMap = boneMap->child; timelineMap; timelineMap = timelineMap->next)
		{
			if (string(timelineMap->name) == "rotate") 
			{
				push_byte(0);
				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_float(Json_getFloat(valueMap, "angle", 0));
					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
			else
			{
				if (string(timelineMap->name) == "scale")
					push_byte(2);
				else if (string(timelineMap->name) == "translate")
					push_byte(1);
				else if (string(timelineMap->name) == "shear")
					push_byte(3);
				else
					return -4;

				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_float(Json_getFloat(valueMap, "x", 0));
					push_float(Json_getFloat(valueMap, "y", 0));
					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
		}
	}

	/* IK constraint timelines. */
	Json* ik = Json_getItem(animation, "ik");
	push_varint(ik ? ik->size : 0, 1);
	for (Json *ikMap = ik ? ik->child : 0; ikMap; ikMap = ikMap->next)
	{
		int ikIndex = find_string(vcIK, ikMap->name);
		if (ikIndex == -1)
			return -5;
		push_varint(ikIndex, 1);

		push_varint(ikMap->size, 1);
		for (Json *valueMap = ikMap->child; valueMap; valueMap = valueMap->next)
		{
			push_float(Json_getFloat(valueMap, "time", 0));
			push_float(Json_getFloat(valueMap, "mix", 1));
			push_byte(Json_getInt(valueMap, "bendPositive", 1) ? 1 : -1);
			if (valueMap->next)
				push_curve(Json_getItem(valueMap, "curve"));
		}
	}

	/* Transform constraint timelines. */
	Json* transform = Json_getItem(animation, "transform");
	push_varint(transform ? transform->size : 0, 1);
	for (Json *transMap = transform ? transform->child : 0; transMap; transMap = transMap->next)
	{
		int index = find_string(vcTransform, transMap->name);
		if (index == -1)
			return -6;
		push_varint(index, 1);

		push_varint(transMap->size, 1);
		for (Json *valueMap = transMap->child; valueMap; valueMap = valueMap->next)
		{
			push_float(Json_getFloat(valueMap, "time", 0));
			push_float(Json_getFloat(valueMap, "rotateMix", 1));
			push_float(Json_getFloat(valueMap, "translateMix", 1));
			push_float(Json_getFloat(valueMap, "scaleMix", 1));
			push_float(Json_getFloat(valueMap, "shearMix", 1));
			if (valueMap->next)
				push_curve(Json_getItem(valueMap, "curve"));
		}
	}

	/* Path constraint timelines. */
	Json* paths = Json_getItem(animation, "paths");
	push_varint(paths ? paths->size : 0, 1);
	for (Json *pathMap = paths ? paths->child : 0; pathMap; pathMap = pathMap->next)
	{
		int pathIndex = find_string(vcPaths, pathMap->name);
		if (pathIndex == -1)
			return -7;
		push_varint(pathIndex, 1);

		push_varint(pathMap->size, 1);
		for (Json *timelineMap = pathMap->child; timelineMap; timelineMap = timelineMap->next)
		{
			string timelineName = timelineMap->name;
			if (timelineName == "position" || timelineName == "spacing")
			{
				if (timelineName == "position")
					push_byte(0);
				else
					push_byte(1);

				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_float(Json_getFloat(valueMap, timelineName.c_str(), 0));
					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
			else if (string(timelineMap->name) == "mix")
			{
				push_byte(2);
				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					push_float(Json_getFloat(valueMap, "rotateMix", 1));
					push_float(Json_getFloat(valueMap, "translateMix", 1));

					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
			else
				return -8;
		}
	}


	/* Deform timelines. */
	Json* deform = Json_getItem(animation, "deform");
	push_varint(deform ? deform->size : 0, 1);
	for (Json *deformMap = deform ? deform->child : 0; deformMap; deformMap = deformMap->next)
	{
		int skinIndex = find_string(vcSkins, deformMap->name);
		if (skinIndex == -1)
			return -9;
		push_varint(skinIndex, 1);

		push_varint(deformMap->size, 1);
		for (Json *slotMap = deformMap->child; slotMap; slotMap = slotMap->next)
		{
			int slotIndex = find_string(vcSlots, slotMap->name);
			if (slotIndex == -1)
				return -10;
			push_varint(slotIndex, 1);

			push_varint(slotMap->size, 1);
			for (Json *timelineMap = slotMap->child; timelineMap; timelineMap = timelineMap->next)
			{
				push_string(timelineMap->name);

				push_varint(timelineMap->size, 1);
				for (Json *valueMap = timelineMap->child; valueMap; valueMap = valueMap->next)
				{
					push_float(Json_getFloat(valueMap, "time", 0));
					Json* vertices = Json_getItem(valueMap, "vertices");
					if (!vertices)
					{
						push_varint(0, 1);
					}
					else
					{
						push_varint(vertices->size, 1);

						int start = Json_getInt(valueMap, "offset", 0);
						push_varint(start, 1);

						for (Json *vertex = vertices->child; vertex; vertex = vertex->next)
							push_float(vertex->valueFloat);
					}

					if (valueMap->next)
						push_curve(Json_getItem(valueMap, "curve"));
				}
			}
		}
	}


	/* Draw order timeline. */
	Json* drawOrder = Json_getItem(animation, "drawOrder");
	push_varint(drawOrder ? drawOrder->size : 0, 1);
	for (Json *valueMap = drawOrder ? drawOrder->child : 0; valueMap; valueMap = valueMap->next)
	{
		push_float(Json_getFloat(valueMap, "time", 0));

		Json* offsets = Json_getItem(valueMap, "offsets");
		push_varint(offsets ? offsets->size : 0, 1);
		for (Json *offsetMap = offsets ? offsets->child : 0; offsetMap; offsetMap = offsetMap->next)
		{
			int slotIndex = find_string(vcSlots, Json_getString(offsetMap, "slot", 0));
			if (slotIndex == -1)
				return -11;
			push_varint(slotIndex, 1);
			push_varint(Json_getInt(offsetMap, "offset", 0), 1);
		}
	}


	/* Event timeline. */
	Json* events = Json_getItem(animation, "events");
	push_varint(events ? events->size : 0, 1);
	for (Json *valueMap = events ? events->child : 0; valueMap; valueMap = valueMap->next)
	{
		const char * name = Json_getString(valueMap, "name", 0);
		if (!name)
			return -12;
		push_float(Json_getFloat(valueMap, "time", 0));
		int eventIndex = -1;
		for (int i = 0; i < vcEvents.size(); ++i)
		{
			if (vcEvents[i].name == name)
			{
				eventIndex = i;
				break;
			}
		}
		if (eventIndex == -1)
			return -13;

		push_varint(eventIndex, 1);

		push_varint(Json_getInt(valueMap, "int", vcEvents[eventIndex].intValue), 0);
		push_float(Json_getFloat(valueMap, "float", vcEvents[eventIndex].floatValue));
		const char * str = Json_getString(valueMap, "string", 0);
		push_boolen(str ? 1 : 0);
		if(str)
			push_string(str);
	}

	/*string log = "animation:"+string(animation->name);
	log += " slots:" + to_string(slots ? slots->size : 0);
	log += " bones:" + to_string(bones ? bones->size : 0);
	log += " ik:" + to_string(ik ? ik->size : 0);
	log += " transform:" + to_string(transform ? transform->size : 0);
	log += " paths:" + to_string(paths ? paths->size : 0);
	log += " deform:" + to_string(deform ? deform->size : 0);
	log += " drawOrder:" + to_string(drawOrder ? drawOrder->size : 0);
	log += " events:" + to_string(events ? events->size : 0);
	cout << log << endl;*/

	cout << "     \"" << animation->name<<"\",";

	return 0;
}


int convert_json_to_binary(const char *json, size_t len, unsigned char *outBuff, const char *atlas)
{
	buff_data = outBuff;
	buff_pos = 0;
	
	if (len < 16)
		return -1;

	if (json[0] != '{')
		return -2;
	
	string head(json, 18);
	if (head.find("\"skeleton\"") == head.npos)
		return -3;

	Json *root = Json_create(json);
	if (!root)
		return -4;

	all_atlas.clear();
	if (atlas)
		parse_atlas(atlas);

	int rt = 0;
	
	// skeleton
	Json* skeleton = Json_getItem(root, "skeleton");
	if (!skeleton) {
		Json_dispose(root);
		return -5;
	}

	const char *hash = Json_getString(skeleton, "hash", "");
	if (!hash) {
		Json_dispose(root);
		return -6;
	}
	push_string(hash);

	const char *version = Json_getString(skeleton, "spine", "");
	if (!version) {
		Json_dispose(root);
		return -7;
	}
	push_string(version);

	float width = Json_getFloat(skeleton, "width", 0);
	push_float(width);

	float height = Json_getFloat(skeleton, "height", 0);
	push_float(height);

	push_boolen(false);
		
	// bones
	Json* bones = Json_getItem(root, "bones");
	if (!bones || bones->size == 0) {
		Json_dispose(root);
		return -8;
	}
	auto vcBones = process_bones(bones);
	push_varint(vcBones.size(), 1);
	for (size_t i=0;i<vcBones.size();++i)
	{
		BoneData bone = vcBones[i];
		push_string(bone.name.c_str());
		if (i > 0)
			push_varint(bone.parent, 1);
		push_float(bone.rotation);
		push_float(bone.x);
		push_float(bone.y);
		push_float(bone.scaleX);
		push_float(bone.scaleY);
		push_float(bone.shearX);
		push_float(bone.shearY);
		push_float(bone.length);
		push_varint(bone.mode, 1);
	}

	// Slots
	Json* slots = Json_getItem(root, "slots");
	if (!slots || slots->size == 0) {
		Json_dispose(root);
		return -9;
	}
	push_varint(slots->size, 1);

	vector<string> vcSlots;
	for (Json *slot = slots->child; slot; slot = slot->next)
	{
		const char *name = Json_getString(slot, "name", "");
		push_string(name);
		vcSlots.push_back(name);

		string boneName = Json_getString(slot, "bone", "");
		int boneIndex = find_bone(vcBones, boneName);
		if (boneIndex == -1) {
			Json_dispose(root);
			return -10;
		}
		push_varint(boneIndex, 1);
			
		const char *color = Json_getString(slot, "color", 0);
		push_color(color);

		const char *dark = Json_getString(slot, "dark", 0);
		push_color(dark);

		const char *attachment = Json_getString(slot, "attachment", "");
		push_string(attachment);

		Json *blend = Json_getItem(slot, "blend");
		int blendMode = 0;
		if (blend) {
			if (string(blend->valueString) == "additive")
				blendMode = 1;
			else if (string(blend->valueString) == "multiply")
				blendMode = 2;
			else if (string(blend->valueString) == "screen")
				blendMode = 3;
		}
		push_varint(blendMode, 1);
	}


	/* IK constraints. */
	vector<string> vcIK;
	Json *ik = Json_getItem(root, "ik");
	push_varint(ik ? ik->size : 0, 1);
	for (Json *ikMap = ik ? ik->child : 0; ikMap; ikMap = ikMap->next)
	{
		push_string(Json_getString(ikMap, "name", ""));
		vcIK.push_back(Json_getString(ikMap, "name", ""));

		push_varint(Json_getInt(ikMap, "order", 0), 1);
				
		Json *bones = Json_getItem(ikMap, "bones");
		if (!bones)
		{
			Json_dispose(root);
			return -11;
		}
		push_varint(bones->size, 1);
		for (Json *bone = bones->child; bone; bone = bone->next)
		{
			int boneIndex = find_bone(vcBones, bone->valueString);
			if (boneIndex == -1) {
				Json_dispose(root);
				return -12;
			}
			push_varint(boneIndex, 1);
		}
				
		string targetName = Json_getString(ikMap, "target", "");
		int boneIndex = find_bone(vcBones, targetName);
		if (boneIndex == -1)
		{
			Json_dispose(root);
			return -13;
		}
		push_varint(boneIndex, 1);

		push_float(Json_getFloat(ikMap, "mix", 1));
		push_float(Json_getInt(ikMap, "bendPositive", 1) ? 1 : -1);
	}

		
	/* Transform constraints. */
	vector<string> vcTransform;
	Json *transform = Json_getItem(root, "transform");
	push_varint(transform ? transform->size : 0, 1);
	for (Json *transformMap = transform ? transform->child : 0; transformMap; transformMap = transformMap->next)
	{
		push_string(Json_getString(transformMap, "name", ""));
		vcTransform.push_back(Json_getString(transformMap, "name", ""));

		push_varint(Json_getInt(transformMap, "order", 0), 1);

		Json *bones = Json_getItem(transformMap, "bones");
		if (!bones)
		{
			Json_dispose(root);
			return -15;
		}
		push_varint(bones->size, 1);
		for (Json *bone = bones->child; bone; bone = bone->next)
		{
			int boneIndex = find_bone(vcBones, bone->valueString);
			if (boneIndex == -1) {
				Json_dispose(root);
				return -15;
			}
			push_varint(boneIndex, 1);
		}
		string targetName = Json_getString(transformMap, "target", "");
		int boneIndex = find_bone(vcBones, targetName);
		if (boneIndex == -1) {
			Json_dispose(root);
			return -16;
		}
		push_varint(boneIndex, 1);

		push_boolen(Json_getInt(transformMap, "local", 0));
		push_boolen(Json_getInt(transformMap, "relative", 0));

		push_float(Json_getFloat(transformMap, "rotation", 0));
		push_float(Json_getFloat(transformMap, "x", 0));
		push_float(Json_getFloat(transformMap, "y", 0));
		push_float(Json_getFloat(transformMap, "scaleX", 0));
		push_float(Json_getFloat(transformMap, "scaleY", 0));
		push_float(Json_getFloat(transformMap, "shearY", 0));
		push_float(Json_getFloat(transformMap, "rotateMix", 1));
		push_float(Json_getFloat(transformMap, "translateMix", 1));
		push_float(Json_getFloat(transformMap, "scaleMix", 1));
		push_float(Json_getFloat(transformMap, "shearMix", 1));
	}

	/* Path constraints */
	vector<string> vcPaths;
	Json *path = Json_getItem(root, "path");
	push_varint(path ? path->size : 0, 1);
	for (Json *pathMap = path ? path->child : 0; pathMap; pathMap = pathMap->next)
	{
		push_string(Json_getString(pathMap, "name", ""));
		vcPaths.push_back(Json_getString(pathMap, "name", ""));

		push_varint(Json_getInt(pathMap, "order", 0), 1);

		Json *bones = Json_getItem(pathMap, "bones");
		if (!bones)
		{
			Json_dispose(root);
			return -17;
		}
		push_varint(bones->size, 1);
		for (Json *bone = bones->child; bone; bone = bone->next)
		{
			int boneIndex = find_bone(vcBones, bone->valueString);
			if (boneIndex == -1) {
				Json_dispose(root);
				return -18;
			}
			push_varint(boneIndex, 1);
		}

		string targetName = Json_getString(pathMap, "target", "");
		int slotIndex = find_string(vcSlots, targetName);
		if (slotIndex == -1) {
			Json_dispose(root);
			return -19;
		}
		push_varint(slotIndex, 1);

		string positionMode = Json_getString(pathMap, "positionMode", "percent");
		if (positionMode == "fixed")
			push_varint(0, 1);
		else
			push_varint(1, 1);

		string spacingMode = Json_getString(pathMap, "spacingMode", "length");
		if (spacingMode == "fixed")
			push_varint(1, 1);
		else if (spacingMode == "percent")
			push_varint(2, 1);
		else
			push_varint(0, 1);

		string rotateMode = Json_getString(pathMap, "rotateMode", "tangent");
		if (rotateMode == "chain")
			push_varint(1, 1);
		else if (rotateMode == "chainScale")
			push_varint(2, 1);
		else
			push_varint(0, 1);

		push_float(Json_getFloat(pathMap, "rotation", 0));
		push_float(Json_getFloat(pathMap, "position", 0));
		push_float(Json_getFloat(pathMap, "spacing", 0));
		push_float(Json_getFloat(pathMap, "rotateMix", 1));
		push_float(Json_getFloat(pathMap, "translateMix", 1));
	}


	/* Skins. */
	vector<string> vcSkins;
	Json *skins = Json_getItem(root, "skins");
	if (!skins || skins->size <= 0) {
		Json_dispose(root);
		return -20;
	}
	Json *defaultSkin = NULL;
	for (Json *skin = skins->child; skin; skin = skin->next)
	{ 
		if (string(skin->name) == "default")
		{
			defaultSkin = skin;
			break;
		}
	}
	if (defaultSkin)
	{
		int rt = parse_skin(defaultSkin, vcSlots);
		if (rt != 0)
		{
			Json_dispose(root);
			return -100+ rt;
		}
		vcSkins.push_back("default");
	}

	push_varint(skins->size-1, 1);
	for (Json *skin = skins->child; skin; skin = skin->next)
	{
		if (string(skin->name) != "default")
		{
			vcSkins.push_back(skin->name);
			push_string(skin->name);
			int rt = parse_skin(skin, vcSlots);
			if (rt != 0)
			{
				Json_dispose(root);
				return -200 + rt;
			}
		}
	}


	/* Events. */
	vector<EventData> vcEvents;
	Json *events = Json_getItem(root, "events");
	push_varint(events ? events->size : 0, 1);
	for (Json *eventMap = events ? events->child : 0; eventMap; eventMap = eventMap->next)
	{
		EventData ed;
		ed.name = eventMap->name;
		ed.intValue = Json_getInt(eventMap, "int", 0);
		ed.floatValue = Json_getFloat(eventMap, "float", 0);
		ed.stringValue = Json_getString(eventMap, "string", 0);
		vcEvents.push_back(ed);

		push_string(ed.name.c_str());
		push_varint(ed.intValue, 0);
		push_float(ed.floatValue);
		push_string(ed.stringValue.c_str());
	}


	/* Animations. */
	Json  *animations = Json_getItem(root, "animations");
	push_varint(animations ? animations->size : 0, 1);	
	for (Json *aniMap = animations ? animations->child : 0; aniMap; aniMap = aniMap->next) {
		push_string(aniMap->name);
		int rt = parse_animation(aniMap, vcSlots, vcBones, vcIK, vcTransform, vcPaths, vcSkins, vcEvents);
		if (rt != 0)
		{
			Json_dispose(root);
			return -300 + rt;
		}
	}

	Json_dispose(root);
	return buff_pos;
}
