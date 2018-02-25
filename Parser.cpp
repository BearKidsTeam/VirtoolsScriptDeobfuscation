#include "ckall.h"
#include "precomp.h"
#include <map>
#include "interfaceData.h"
using namespace std;

class Parser
{
public:
	interface_t interface_data;
private:
	char *m_data;
	char *m_filename;
	FILE *m_logger;
	map<int, int> m_bb_type_map;
	int m_depth;
#define into(statement) do{ \
		for (int i = 0; i < m_depth * 2; ++i) \
			fputc(' ', m_logger); \
		++m_depth; \
		fprintf(m_logger,"%s:\n",#statement); \
		(statement); \
		--m_depth; \
	}while(0)
	void _assert(bool expression, char *message)
	{
		if (!expression)
		{
			throw message;
		}
	}
#define xstr(s) str(s)
#define str(s) #s
#define SLINE xstr(__LINE__)
#undef assert
#define assert(exp) _assert((exp),"Line " SLINE ": assert failed: " #exp)
	int ClaimInt(char *&position, char *format, ...)
	{
		static char m_buffer[1024];
		va_list arg;
		va_start(arg, format);
		for (int i = 0; i < m_depth * 2; ++i)
			fputc(' ', m_logger);
		vfprintf(m_logger, format, arg);
		int value = *(int *)position;
		fprintf(m_logger, " = %d (0x%x)\n", value, value);
		fflush(m_logger);
		position += 4;
		va_end(arg);
		return value;
	}
	float ClaimFloat(char *&position, char *format, ...)
	{
		static char m_buffer[1024];
		va_list arg;
		va_start(arg, format);
		for (int i = 0; i < m_depth * 2; ++i)
			fputc(' ', m_logger);
		vfprintf(m_logger, format, arg);
		float value = *(float *)position;
		fprintf(m_logger, " = %.2f\n", value);
		fflush(m_logger);
		position += 4;
		va_end(arg);
		return value;
	}
	void parse_chunk(char *&p)
	{
		int class_id = ClaimInt(p, "class_id");
		interface_data.class_id = class_id;
		int length = ClaimInt(p, "length");
		into(parse_behavior_data(p, p + length * 4));
	}
	void parse_behavior_data(char *&p, char *end)
	{
		assert(0x10 == ClaimInt(p, "fixed_var1"));
		int body_length = ClaimInt(p, "body_length");
		into(parse_body(p, p + body_length * 4));
		interface_data.tail = p;
		interface_data.tail_length = end - p;
	}
	void parse_body(char *&p, char *end)
	{
		int interface_length = ClaimInt(p, "interface_length");
		if(interface_length)
			into(parse_interface_data(p, p + interface_length * 4));
		assert(0x20 == ClaimInt(p, "fixed_var3"));
		assert(0 == ClaimInt(p, "fixed_var4"));
		assert(p == end);
	}
	void parse_interface_data(char *&p, char *end)
	{
		assert(0xFFFFFFFF == ClaimInt(p, "fixed_var1"));
		assert(0x70000 == ClaimInt(p, "fixed_var2"));
		int pos_info_length = ClaimInt(p, "pos_info_length");
		pos_info_length += 3;
		assert(0 == ClaimInt(p, "zero"));
		int n_obj_fake;
		into(n_obj_fake = parse_pos_info(p, p + pos_info_length * 4));
		// assert(0x54543B94 == ClaimInt(p, "wtf"));
		// assert(0 == ClaimInt(p, "zero"));
		// assert(0 == ClaimInt(p, "zero"));
		for (int i = 0; i < n_obj_fake; ++i)
		{
			ClaimInt(p, "obj %d unknown attr", i);
		}
		assert(p == end);
	}
	int parse_pos_info(char *&p, char *end)
	{
		int n_obj_fake = ClaimInt(p, "n_obj_fake");
		assert(0 == ClaimInt(p, "zero"));
		assert(0 == ClaimInt(p, "zero"));
		assert(1 == ClaimInt(p, "one"));
		ClaimInt(p, "size_related_thing");
		ClaimInt(p, "fixed_var1");
		int n_bb_and_start = ClaimInt(p, "n_bb_and_start");
		into(parse_start(p));
		assert(0xc8c8c8 == ClaimInt(p, "c8c8c8"));
		parse_link_op_comment_localp_sharedp(p);
		for (int i = 0; i < n_bb_and_start - 1; ++i)
		{
			into(parse_bb(p));
		}
		assert(p <= end && end - p <= 3 * 4);
		while (p < end)
		{
			ClaimInt(p, "(ignored)");
		}
		return n_obj_fake;
	}
	void parse_link_op_comment_localp_sharedp(char *&p)
	{
		int link_count = ClaimInt(p, "link_count");
		for (int i = 0; i < link_count; ++i)
		{
			into(parse_link(p));
		}
		int op_count = ClaimInt(p, "op_count");
		for (int i = 0; i < op_count; ++i)
		{
			into(parse_op_pos(p));
		}
		int comment_count = ClaimInt(p, "comment_count");
		for (int i = 0; i < comment_count; ++i)
		{
			into(parse_comment(p));
		}
		int local_param_count = ClaimInt(p, "local_param_count");
		into(parse_int_points(p, local_param_count));
		for (int i = 0; i < local_param_count; ++i)
		{
			ClaimInt(p, "local_param_style %d", i); 
			//0x200 - name
			//0x400 - symbolic (closed)
			//0x1000 - name & value
			//0x2000 - value
		}
		int shared_param_count = ClaimInt(p, "shared_param_count");
		into(parse_int_points(p, shared_param_count));
		for (int i = 0; i < shared_param_count; ++i)
		{
			ClaimInt(p, "shared_param_style %d", i); // one in 0x200 0x400 0x1000 0x2000
		}
		for (int i = 0; i < shared_param_count; ++i)
		{
			ClaimInt(p, "shared_param_source_id %d", i);
		}
	}
	void parse_start(char *&p)
	{
		ClaimInt(p, "id");
		int var1 = ClaimInt(p, "unknown var1");
		assert(var1 == 0x0 || var1 == 0x200);
		int var2 = ClaimInt(p, "unknown var2");
		for (int i = 0; i < 1; ++i)
			assert(0 == ClaimInt(p, "zero %d", i + 1));
		ClaimFloat(p, "vertical script begin");
		ClaimFloat(p, "horizontal start position");
		ClaimFloat(p, "vertical start position");
		ClaimFloat(p, "vertical script end");
		for (int i = 0; i < 2; ++i)
			assert(0 == ClaimInt(p, "zero %d", i + 1));
	}
	void parse_link(char *&p)
	{
		int link_type = ClaimInt(p, "link_type");//1=blink 2=plink 10002=exported plink
		assert(link_type == 1 || link_type == 2 || link_type == 0x10002);
		ClaimInt(p, "id");
		ClaimInt(p, "out_obj_id");
		ClaimInt(p, "out_index");
		ClaimInt(p, "out_type");
		int points_count = ClaimInt(p, "points_count");
		into(parse_points(p, points_count));
		ClaimInt(p, "in_obj_id");
		ClaimInt(p, "in_index");//-2 = target
		ClaimInt(p, "in_type");
	}
	void parse_bb(char *&p)
	{
		int id = ClaimInt(p, "id");
		int folded = ClaimInt(p, "folded");
		int depth = ClaimInt(p, "depth");
		into(parse_rect_pos(p));
		int next_int = *(int *)p;
		assert(0x200 == folded || 0x0 == folded); // when 0x200: single block or folded bg

		into(parse_expand_shape(p));
		if (m_bb_type_map[id] == 0x0) // single block
		{
			for (int i = 0; i < 3; ++i)
				assert(0 == ClaimInt(p, "zero %d", i + 1));
		}
		else // bg
		{
			parse_link_op_comment_localp_sharedp(p);
			int inward_input_count = ClaimInt(p, "inward_input_count");
			for (int i = 0; i < inward_input_count; ++i)
			{
				into(parse_input(p)); // Describe input position inward
			}
			int outward_input_count = ClaimInt(p, "outward_input_count");
			for (int i = 0; i < outward_input_count; ++i)
			{
				into(parse_input(p)); // Describe input position outward
			}
			int inward_output_count = ClaimInt(p, "inward_output_count");
			for (int i = 0; i < inward_output_count; ++i)
			{
				into(parse_output(p)); // Describe output position inward
			}
			int outward_output_count = ClaimInt(p, "outward_output_count");
			for (int i = 0; i < outward_output_count; ++i)
			{
				into(parse_output(p)); // Describe output position outward
			}
		}
	}
	void parse_rect_pos(char *&p)
	{
		ClaimFloat(p, "h_pos");
		ClaimFloat(p, "v_pos");
		ClaimFloat(p, "h_size");
		ClaimFloat(p, "v_size");
	}
	void parse_expand_shape(char *&p)
	{
		ClaimFloat(p, "h_expand_size");
		ClaimFloat(p, "v_expand_size");
	}
	void parse_input(char *&p)
	{
		ClaimInt(p, "position");
		ClaimInt(p, "minus1");
	}
	void parse_output(char *&p)
	{
		ClaimInt(p, "position");
		ClaimInt(p, "positive1");
	}
	void parse_op_pos(char *&p)
	{
		ClaimInt(p, "id");
		ClaimFloat(p, "h_pos");
		ClaimFloat(p, "v_pos");
	}
	void parse_comment(char *&p)
	{
		into(parse_rect_pos(p));
		int sz = ClaimInt(p, "comment_length");
		fprintf(m_logger, "% *scomment = ", 2 * m_depth, "");
		for (int i = 0; i<sz - 1; ++i)putc(*(p++), m_logger);
		p++;
		fprintf(m_logger, "\n% *s(%d padding bytes)\n", 2 * m_depth, "", (4 - sz % 4) % 4);
		p += (4 - sz % 4) % 4;
		int style_flag = ClaimInt(p, "flag");
	}
	void parse_points(char *&p,int count)
	{
		for (int i = 0; i < count; ++i)
		{
			ClaimFloat(p, "point %d_h", i);
			ClaimFloat(p, "point %d_v", i);
		}
	}
	void parse_int_points(char *&p, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			ClaimInt(p, "point %d_h", i);
			ClaimInt(p, "point %d_v", i);
		}
	}
public:
	Parser(char *data, char *filename, map<int,int> bb_type_map)
	{
		m_data = data;
		m_filename = filename;
		m_bb_type_map = bb_type_map;
		interface_data = interface_t();
	}
	void parse()
	{
		m_logger = fopen(m_filename, "w");
		m_depth = 0;
		char *p = m_data;
		parse_chunk(p);
		fclose(m_logger);
	}
};
void output_obj(CKContext *context, CKObject *obj, char *type)
{
	if(obj!=NULL)
		context->OutputToConsoleEx("[0x%x(%d)](%d)%s:%s", obj->GetID(), obj->GetID(), obj->GetClassID(), type, obj->GetName());
}
void scan_bb(CKBehavior *bb, CKContext *context)
{
	context->OutputToConsoleEx("0x%x",bb->GetType());
	output_obj(context, bb, "bb");
	int cnt;
	cnt = bb->GetInputCount();
	for (int i = 0; i < cnt; ++i)
	{
		output_obj(context, bb->GetInput(i), "bIn");
	}
	cnt = bb->GetOutputCount();
	for (int i = 0; i < cnt; ++i)
	{
		output_obj(context, bb->GetOutput(i), "bOut");
	}
	cnt = bb->GetInputParameterCount();
	for (int i = 0; i < cnt; ++i)
	{
		output_obj(context, bb->GetInputParameter(i), "pIn");
		output_obj(context, bb->GetInputParameterObject(i), "pIn obj");
	}
	cnt = bb->GetOutputParameterCount();
	for (int i = 0; i < cnt; ++i)
	{
		output_obj(context, bb->GetOutputParameter(i), "pOut");
		output_obj(context, bb->GetOutputParameterObject(i), "pOut obj");
	}
	cnt = bb->GetLocalParameterCount();
	for (int i = 0; i < cnt; ++i)
	{
		output_obj(context, bb->GetLocalParameter(i), "pLocal");
		output_obj(context, bb->GetLocalParameterObject(i), "pLocal obj");
	}
	cnt = bb->GetSubBehaviorCount();
	for (int i = 0; i < cnt; ++i)
	{
		CKBehavior* sub_bb = bb->GetSubBehavior(i);
		scan_bb(sub_bb, context);
	}
}
void pre_scan(CKBehavior *bb, map<int,int> &mym)
{
	mym[bb->GetID()] = bb->GetType();
	int cnt = bb->GetSubBehaviorCount();
	for (int i = 0; i < cnt; ++i)
	{
		CKBehavior* sub_bb = bb->GetSubBehavior(i);
		pre_scan(sub_bb, mym);
	}
}

void parse_string_test(CKBehavior *bb,char *buffer)
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
	map<int, int> bbTypeMap;
	pre_scan(bb, bbTypeMap);
	sprintf(filename, base_path "/generator/parser_out_%s.log", name);
	Parser parser = Parser(buffer, filename, bbTypeMap);
	parser.parse();
}

interface_t parse_bb_test(CKBehavior *bb, CKFile *file)
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
	map<int, int> bbTypeMap;
	pre_scan(bb, bbTypeMap);
	sprintf(filename, base_path "/parser/parser_out_%s.log", name);
	//bb->PreSave(file, 0);
	CKStateChunk *chunk = bb->Save(file, 0);
	int length = chunk->ConvertToBuffer(NULL);
	char *buffer = new char[length + 1];
	chunk->ConvertToBuffer(buffer);
	buffer[length] = 0;
	Parser parser = Parser(buffer, filename, bbTypeMap);
	parser.parse();
	return parser.interface_data;
}