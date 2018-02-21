#include "precomp.h"
#include "interfaceData.h"
#include "ckall.h"
using namespace std;

class Generator
{
private:
	interface_t &m_data;
	void generate_int(char *&p, int data)
	{
		*(int *)p = data;
		p += 4;
	}
	void generate_float(char *&p, float data)
	{
		*(float *)p = data;
		p += 4;
	}
	void generate_char(char *&p, char data)
	{
		*(char *)p = data;
		p += 1;
	}
public:
	Generator(interface_t &data):m_data(data)
	{

	}
	void generate_body(char *&p)
	{
		int *length = (int *)p;
		generate_int(p, -1);
		char *begin = p;
		// generate_interface(p);
		*length = (p - begin) / 4;
		generate_int(p, 0x20);
		generate_int(p, 0);
	}
	void generate_behavior_data(char *&p)
	{
		generate_int(p, 0x10);
		int *length = (int *)p;
		generate_int(p, -1);
		char *begin = p;
		generate_body(p);
		*length = (p - begin) / 4;
		for (int i = 0; i < m_data.tail_length; ++i)
		{
			generate_char(p, m_data.tail[i]);
		}
	}
	void generate_all(char *&p)
	{
		char *none = 0;
		generate_int(p, 0xcabba9e);
		int *length = (int *)p;
		generate_int(p, -1);
		char *begin = p;
		generate_behavior_data(p);
		*length = (p - begin) / 4;
	}

	int Generate(char *output)
	{
		char *p = output;
		generate_all(p);
		return (p - output) / 4;
	}
};

void generate_bb_test(interface_t &interface_data, CKBehavior *bb, CKFile *file)
{
	char filename[1024];
	char name[1024];
	strcpy(name, bb->GetName());
	for (int i = strlen(name) - 1; i >= 0; --i)
	{
		if (name[i] >= 'a' && name[i] <= 'z') continue;
		if (name[i] >= 'A' && name[i] <= 'Z') continue;
		if (name[i] >= '0' && name[i] <= '9') continue;
		if (name[i] == '.' || name[i] == '_') continue;
		name[i] = '_';
	}
	sprintf(filename, "C:\\Users\\jjy\\Desktop\\test\\generator\\generator_out_%s.log", name);
	Generator generator = Generator(interface_data);
	char *buffer = new char[1048576];
	int length = generator.Generate(buffer);
	FILE *fout = fopen(filename, "wb");
	for (int i = 0; i < length * 4; ++i)
	{
		fputc(buffer[i], fout);
	}
	fclose(fout);
	CKStateChunk *chunk = CreateCKStateChunk((CK_CLASSID)0x0);
	chunk->Clear();
	chunk->ConvertFromBuffer(buffer);
	bb->Load(chunk, file);
	bb->PostLoad();
	delete[]buffer;

}