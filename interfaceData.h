#pragma once
#include <vector>
using namespace std;
struct rect_t
{
	float h_pos;
	float v_pos;
	float h_size;
	float v_size;
	rect_t()
	{
		h_pos = 300;
		v_pos = 100;
		h_size = 100;
		v_size = 40;
	}
	rect_t(float input_h_pos, float input_v_pos, float input_h_size, float input_v_size)
	{
		h_pos = input_h_pos;
		v_pos = input_v_pos;
		h_size = input_h_size;
		v_size = input_v_size;
	}
};
struct point_t
{
	float h;
	float v;
};
struct start_t
{
	int id;
	rect_t pos = rect_t(0.0, 140.0, 100.0, 380.0);
};
struct link_endpoint_t
{
	unsigned int id;
	int index;
	int type;
	//types:
	// 5 - pOut link (shortcut) (has the same id as the link target)
	// 7 - pIn
	// 8 - pOut
	// 9 - pLocal
	//10 - target pIn
	//12 - bIn
	//13 - bOut
	//26 - "Start" bIn
};

struct link_t
{
	int id;
	int type;
	link_endpoint_t start;
	int point_count;
	vector<point_t> points;
	link_endpoint_t end;
};

struct op_t
{
	int id;
	int h_pos;
	int v_pos;
};
struct comment_t
{
	// Ignored
};
enum param_style_enum
{
	param_style_name = 0x200,
	param_style_closed = 0x400,
	param_style_namevalue = 0x1000,
	param_style_value = 0x2000
};
struct param_t
{
	int id;
	int h_pos;
	int v_pos;
	param_style_enum style = param_style_name;
	int source_id;
};
struct bb_t
{
	int id;
	bool folded;
	int depth;
	rect_t size;
	float h_expand_size;
	float v_expand_size;
	bool is_bg;
	int n_links;
	vector<link_t> links;
	int n_ops;
	vector<op_t> ops;
	int n_comments;
	vector<comment_t> comments;
	int n_local_param;
	vector<param_t> local_params;
	int n_shared_param;
	vector<param_t> shared_params;
	int input_count;
	vector<int> inward_inputs;
	vector<int> outward_inputs;
	int output_count;
	vector<int> inward_outputs;
	vector<int> outward_outputs;
};
struct interface_t
{
	int class_id;
	// int n_obj_fake;
	start_t start;
	bb_t script_root;
	int n_bb;
	vector<bb_t> bbs;

	char *tail;
	int tail_length;
};