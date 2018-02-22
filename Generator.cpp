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
	void generate_rect(char *&p, rect_t &data)
	{
		generate_float(p, data.h_pos);
		generate_float(p, data.v_pos);
		generate_float(p, data.h_size);
		generate_float(p, data.v_size);
	}
public:
	Generator(interface_t &data):m_data(data)
	{

	}
	void generate_link_op_comment_localp_sharedp(char *&p)
	{
		generate_int(p, 0);
		generate_int(p, 0);
		generate_int(p, 0);
		generate_int(p, 0);
		generate_int(p, 0);
	}
	void generate_start(char *&p)
	{
		generate_int(p, m_data.start.id);
		for (int i = 0; i < 3; ++i)
			generate_int(p, 0);
		generate_rect(p, m_data.start.pos);
		for (int i = 0; i < 2; ++i)
			generate_int(p, 0);
	}
	void generate_bb(char *&p,bb_t &bb)
	{
		generate_int(p, bb.id);
		generate_int(p, bb.folded ? 0x200 : 0x0);
		generate_int(p, bb.depth);
		generate_rect(p, bb.size);
		generate_float(p, bb.h_expand_size);
		generate_float(p, bb.v_expand_size);
		if (!bb.is_bg)
		{
			generate_int(p, 0);
			generate_int(p, 0);
			generate_int(p, 0);
		}
		else
		{
			throw "not implemented";
		}
	}
	void generate_pos_info(char *&p)
	{
		generate_int(p, m_data.n_bb + 1 +
			m_data.n_links + m_data.n_ops + m_data.n_comments + m_data.n_local_param + m_data.n_shared_param); // tot obj count
		generate_int(p, 0);
		generate_int(p, 0);
		generate_int(p, 1);
		generate_int(p, 0); // size-related-thing, unknown yet
		generate_int(p, 0x16);
		generate_int(p, m_data.n_bb + 1);
		generate_start(p);
		generate_int(p, 0xc8c8c8);
		generate_link_op_comment_localp_sharedp(p);
		for (int i = 0; i < m_data.n_bb; ++i)
		{
			generate_bb(p, m_data.bbs[i]);
		}
	}
	void generate_interface(char *&p)
	{
		generate_int(p, 0xFFFFFFFF);
		generate_int(p, 0x70000);
		int *length = (int *)p;
		generate_int(p, -1);
		generate_int(p, 0);
		char *begin = p;
		generate_pos_info(p);
		*length = (p - begin) / 4;
		generate_int(p, 0x54543B94);
		generate_int(p, 0);
		generate_int(p, 0);
		int n_obj_fake = m_data.n_bb + 1 +
			m_data.n_links + m_data.n_ops + m_data.n_comments + m_data.n_local_param + m_data.n_shared_param;
		for (int i = 0; i < n_obj_fake; ++i)
		{
			generate_int(p, 0);
		}
	}
	void generate_body(char *&p)
	{
		int *length = (int *)p;
		generate_int(p, -1);
		char *begin = p;
		generate_interface(p);
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
		generate_int(p, m_data.class_id);
		int *length = (int *)p;
		generate_int(p, -1);
		char *begin = p;
		generate_behavior_data(p);
		*length = (p - begin) / 4;
		generate_int(p, 0x1);
		generate_int(p, 0x2); // Some padding
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
	extern void parse_string_test(CKBehavior *bb, char *buffer);
	parse_string_test(bb, buffer);
	
	CKStateChunk *chunk = CreateCKStateChunk((CK_CLASSID)0x0);
	chunk->Clear();
	chunk->ConvertFromBuffer(buffer);
	bb->Load(chunk, file);
	bb->PostLoad();
	delete[]buffer;

}