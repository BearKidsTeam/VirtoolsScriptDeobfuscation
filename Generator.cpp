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
	void generate_point(char *&p,point_t &pnt)
	{
		generate_int(p,pnt.h);
		generate_int(p,pnt.v);
	}
	void generate_link(char *&p,link_t &lnk)
	{
		generate_int(p,lnk.type);
		generate_int(p,lnk.id);
		generate_int(p,lnk.start.id);
		generate_int(p,lnk.start.index);
		generate_int(p,lnk.start.type);
		generate_int(p,lnk.point_count);
		for(int i=0;i<lnk.point_count;++i)
		generate_point(p,lnk.points[i]);
		generate_int(p,lnk.end.id);
		generate_int(p,lnk.end.index);
		generate_int(p,lnk.end.type);
	}
	void generate_op(char *&p,op_t &op)
	{
		generate_int(p,op.id);
		generate_int(p,op.h_pos);
		generate_int(p,op.v_pos);
	}
	void generate_io(char *&p,int v,int isin)
	{
		generate_int(p,v);
		generate_int(p,isin?-1:1);
	}
	void generate_link_op_comment_localp_sharedp(char *&p,bb_t &bb)
	{
		generate_int(p, bb.links.size());
		for(size_t i=0;i<bb.links.size();++i)
			generate_link(p,bb.links[i]);
		generate_int(p, bb.ops.size());
		for(size_t i=0;i<bb.ops.size();++i)
			generate_op(p,bb.ops[i]);
		generate_int(p, 0);//no comments anyway
		generate_int(p, bb.local_params.size());
		for(size_t i=0;i<bb.local_params.size();++i)
		{
			generate_int(p,bb.local_params[i].h_pos);
			generate_int(p,bb.local_params[i].v_pos);
		}
		for(size_t i=0;i<bb.local_params.size();++i)
			generate_int(p,bb.local_params[i].style);
		generate_int(p, bb.shared_params.size());
		for(size_t i=0;i<bb.shared_params.size();++i)
		{
			generate_int(p,bb.shared_params[i].h_pos);
			generate_int(p,bb.shared_params[i].v_pos);
		}
		for(size_t i=0;i<bb.shared_params.size();++i)
			generate_int(p,bb.shared_params[i].style);
		for(size_t i=0;i<bb.shared_params.size();++i)
			generate_int(p,bb.shared_params[i].source_id);
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
			generate_link_op_comment_localp_sharedp(p,bb);
			generate_int(p,bb.inward_inputs.size());
			for(size_t i=0;i<bb.inward_inputs.size();++i)
			generate_io(p,bb.inward_inputs[i],1);
			generate_int(p,bb.outward_inputs.size());
			for(size_t i=0;i<bb.outward_inputs.size();++i)
			generate_io(p,bb.outward_inputs[i],1);
			generate_int(p,bb.inward_outputs.size());
			for(size_t i=0;i<bb.inward_outputs.size();++i)
			generate_io(p,bb.inward_outputs[i],0);
			generate_int(p,bb.outward_outputs.size());
			for(size_t i=0;i<bb.outward_outputs.size();++i)
			generate_io(p,bb.outward_outputs[i],0);
		}
	}
	void generate_pos_info(char *&p)
	{
		generate_int(p, m_data.n_bb + 1 +
			m_data.script_root.n_links + m_data.script_root.n_ops +
			m_data.script_root.n_comments + m_data.script_root.n_local_param +
			m_data.script_root.n_shared_param); // tot obj count
		generate_int(p, 0);
		generate_int(p, 0);
		generate_int(p, 1);
		generate_int(p, 0); // size-related-thing, unknown yet
		generate_int(p, 0x16);
		generate_int(p, m_data.n_bb + 1);
		generate_start(p);
		generate_int(p, 0xc8c8c8);
		generate_link_op_comment_localp_sharedp(p,m_data.script_root);
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
			m_data.script_root.n_links + m_data.script_root.n_ops + 
			m_data.script_root.n_comments + m_data.script_root.n_local_param +
			m_data.script_root.n_shared_param;
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
	sprintf(filename, base_path "/generator/generator_out_%s.log", name);
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