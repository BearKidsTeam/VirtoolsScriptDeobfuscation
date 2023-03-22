#include "precomp.h"
#include "interfaceData.h"
#include "ckall.h"
using namespace std;

class Generator
{
private:
	char* genbuffer;
	size_t genbuffer_length;
	size_t genbuffer_currsor;
	void buffer_ensure_space(size_t size_in_char) 
	{
		size_t needed = this->genbuffer_currsor + size_in_char;
		if (needed > this->genbuffer_length) {
			// if ordered buffer too low, allocated 512 DWORD directly
			needed = std::max(needed, 1024u * sizeof(DWORD));

			// create new buffer
			char* newbuf = new char[needed];
			// if current buffer is not null, copy existed data
			if (genbuffer != nullptr) 
			{
				std::memcpy(newbuf, genbuffer, genbuffer_length);
			}
			// free old one and assign new one
			delete[] genbuffer;
			genbuffer = newbuf;
			genbuffer_length = needed;
		}
	}
	void buffer_write_data(const void* data, size_t datasize_in_char) {
		buffer_ensure_space(datasize_in_char);
		std::memcpy(genbuffer + genbuffer_currsor, data, datasize_in_char);
		genbuffer_currsor += datasize_in_char;
	}
	void* buffer_get_dataptr(size_t off) 
	{
		assert(off < genbuffer_length);
		return genbuffer + off;
	}
	size_t buffer_tell(void) 
	{
		return genbuffer_currsor;
	}
public:
	char* buffer_getter() 
	{
		return genbuffer;
	}
private:
	interface_t &m_data;
	void generate_int(int data)
	{
		buffer_write_data(&data, sizeof(data));
	}
	void generate_float(float data)
	{
		buffer_write_data(&data, sizeof(data));
	}
	void generate_char(char data)
	{
		buffer_write_data(&data, sizeof(data));
	}
	void generate_char_array(char* pArray, size_t nArraySize) 
	{
		buffer_write_data(pArray, nArraySize);
	}
	void generate_rect(rect_t &data)
	{
		generate_float(data.h_pos);
		generate_float(data.v_pos);
		generate_float(data.h_size);
		generate_float(data.v_size);
	}
public:
	Generator(interface_t &data) : 
		m_data(data),
		genbuffer(nullptr), genbuffer_length(0u), genbuffer_currsor(0u)
	{
		// allocate buffer first
		buffer_ensure_space(1048576u);
	}
	~Generator() 
	{
		if (genbuffer != nullptr) delete[] genbuffer;
	}
	void generate_point(point_t &pnt)
	{
		generate_int(pnt.h);
		generate_int(pnt.v);
	}
	void generate_link(link_t &lnk)
	{
		generate_int(lnk.type);
		generate_int(lnk.id);
		generate_int(lnk.start.id);
		generate_int(lnk.start.index);
		generate_int(lnk.start.type);
		generate_int(lnk.point_count);
		for(int i=0;i<lnk.point_count;++i)
		generate_point(lnk.points[i]);
		generate_int(lnk.end.id);
		generate_int(lnk.end.index);
		generate_int(lnk.end.type);
	}
	void generate_op(op_t &op)
	{
		generate_int(op.id);
		generate_float(op.h_pos);
		generate_float(op.v_pos);
	}
	void generate_io(int v,int isin)
	{
		generate_int(v);
		generate_int(isin?-1:1);
	}
	void generate_link_op_comment_localp_sharedp(bb_t &bb)
	{
		generate_int(bb.links.size());
		for(size_t i=0;i<bb.links.size();++i)
			generate_link(bb.links[i]);
		generate_int(bb.ops.size());
		for(size_t i=0;i<bb.ops.size();++i)
			generate_op(bb.ops[i]);
		generate_int(0);//no comments anyway
		generate_int(bb.local_params.size());
		for(size_t i=0;i<bb.local_params.size();++i)
		{
			generate_int(bb.local_params[i].h_pos);
			generate_int(bb.local_params[i].v_pos);
		}
		for(size_t i=0;i<bb.local_params.size();++i)
			generate_int(bb.local_params[i].style);
		generate_int(bb.shared_params.size());
		for(size_t i=0;i<bb.shared_params.size();++i)
		{
			generate_int(bb.shared_params[i].h_pos);
			generate_int(bb.shared_params[i].v_pos);
		}
		for(size_t i=0;i<bb.shared_params.size();++i)
			generate_int(bb.shared_params[i].style);
		for(size_t i=0;i<bb.shared_params.size();++i)
			generate_int(bb.shared_params[i].source_id);
	}
	void generate_start()
	{
		generate_int(m_data.start.id);
		for (int i = 0; i < 3; ++i)
			generate_int(0);
		generate_float(m_data.start.v_start);
		generate_float(m_data.start.h_start_pos);
		generate_float(m_data.start.v_start_pos);
		generate_float(m_data.start.v_size);
		for (int i = 0; i < 2; ++i)
			generate_int(0);
	}
	void generate_bb(bb_t &bb)
	{
		generate_int(bb.id);
		generate_int(bb.folded ? 0x200 : 0x0);
		generate_int(bb.depth);
		generate_rect(bb.size);
		generate_float(bb.h_expand_size);
		generate_float(bb.v_expand_size);
		if (!bb.is_bg)
		{
			generate_int(0);
			generate_int(0);
			generate_int(0);
		}
		else
		{
			generate_link_op_comment_localp_sharedp(bb);
			generate_int(bb.inward_inputs.size());
			for(size_t i=0;i<bb.inward_inputs.size();++i)
			generate_io(bb.inward_inputs[i],1);
			generate_int(bb.outward_inputs.size());
			for(size_t i=0;i<bb.outward_inputs.size();++i)
			generate_io(bb.outward_inputs[i],1);
			generate_int(bb.inward_outputs.size());
			for(size_t i=0;i<bb.inward_outputs.size();++i)
			generate_io(bb.inward_outputs[i],0);
			generate_int(bb.outward_outputs.size());
			for(size_t i=0;i<bb.outward_outputs.size();++i)
			generate_io(bb.outward_outputs[i],0);
		}
	}
	void generate_pos_info()
	{
		generate_int(m_data.n_bb + 1 +
			m_data.script_root.n_links + m_data.script_root.n_ops +
			m_data.script_root.n_comments + m_data.script_root.n_local_param +
			m_data.script_root.n_shared_param); // tot obj count
		generate_int(0);
		generate_int(0);
		generate_int(1);
		generate_int(0); // size-related-thing, unknown yet
		generate_int(0x16);
		generate_int(m_data.n_bb + 1);
		generate_start();
		generate_int(0xc8c8c8);
		generate_link_op_comment_localp_sharedp(m_data.script_root);
		for (int i = 0; i < m_data.n_bb; ++i)
		{
			generate_bb(m_data.bbs[i]);
		}
	}
	void generate_interface()
	{
		generate_int(0xFFFFFFFF);
		generate_int(0x70000);
		//int *length = (int *)p;
		size_t offLength = buffer_tell();
		generate_int(-1);
		generate_int(0);
		//char *begin = p;
		size_t offBegin = buffer_tell();
		generate_pos_info();
		//*length = (p - begin) / 4;
		*((int*)buffer_get_dataptr(offLength)) = (buffer_tell() - offBegin) / 4;
		generate_int(0x54543B94);
		generate_int(0);
		generate_int(0);
		int n_obj_fake = m_data.n_bb + 1 +
			m_data.script_root.n_links + m_data.script_root.n_ops + 
			m_data.script_root.n_comments + m_data.script_root.n_local_param +
			m_data.script_root.n_shared_param;
		for (int i = 0; i < n_obj_fake; ++i)
		{
			generate_int(0);
		}
	}
	void generate_body()
	{
		//int *length = (int *)p;
		size_t offLength = buffer_tell();
		generate_int(-1);
		//char *begin = p;
		size_t offBegin = buffer_tell();
		generate_interface();
		//*length = (p - begin) / 4;
		*((int*)buffer_get_dataptr(offLength)) = (buffer_tell() - offBegin) / 4;
		generate_int(0x20);
		generate_int(0);
	}
	void generate_behavior_data()
	{
		generate_int(0x10);
		//int *length = (int *)p;
		size_t offLength = buffer_tell();
		generate_int(-1);
		//char *begin = p;
		size_t offBegin = buffer_tell();
		generate_body();
		//*length = (p - begin) / 4;
		*((int*)buffer_get_dataptr(offLength)) = (buffer_tell() - offBegin) / 4;
		//for (int i = 0; i < m_data.tail_length; ++i)
		//{
		//	generate_char(m_data.tail[i]);
		//}
		generate_char_array(m_data.tail, m_data.tail_length);
	}
	void generate_all()
	{
		char *none = 0;
		generate_int(m_data.class_id);
		//int *length = (int *)p;
		size_t offLength = buffer_tell();
		generate_int(-1);
		//char *begin = p;
		size_t offBegin = buffer_tell();
		generate_behavior_data();
		//*length = (p - begin) / 4;
		*((int*)buffer_get_dataptr(offLength)) = (buffer_tell() - offBegin) / 4;
		generate_int(0x1);
		generate_int(0x2); // Some padding
	}

	int Generate()
	{
		generate_all();
		return buffer_tell() / 4;
	}
};

void generate_bb_test(interface_t &interface_data, CKBehavior *bb, CKFile *file)
{
	Generator generator = Generator(interface_data);
	//char *buffer = new char[1048576];
	int length = generator.Generate();

#if defined(VSD_ENABLE_LOG)		// yyc mark: only enable logger on Debug config
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
	sprintf(filename, "%s/generator_out_%s.log", VSDTempFolderGenerator, name);
	FILE *fout = fopen(filename, "wb");
	for (int i = 0; i < length * 4; ++i)
	{
		fputc(buffer[i], fout);
	}
	fclose(fout);
#endif

	extern void parse_string_test(CKBehavior *bb, char *buffer);
	parse_string_test(bb, generator.buffer_getter());
	
	CKStateChunk *chunk = CreateCKStateChunk((CK_CLASSID)0x0);
	chunk->Clear();
	chunk->ConvertFromBuffer(generator.buffer_getter());
	bb->Load(chunk, file);
	bb->PostLoad();
	//delete[]buffer;

}