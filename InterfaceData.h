#pragma once
#include <vector>

struct rect_t {
    float h_pos = 300.0f;
    float v_pos = 100.0f;
    float h_size = 100.0f;
    float v_size = 40.0f;

    rect_t() = default;
    rect_t(float input_h_pos, float input_v_pos, float input_h_size, float input_v_size) {
        h_pos = input_h_pos;
        v_pos = input_v_pos;
        h_size = input_h_size;
        v_size = input_v_size;
    }
};

struct point_t {
    float h = 0.0f;
    float v = 0.0f;
};

struct start_t {
    CK_ID id = -1;
    float v_start = 0.0f;
    float h_start_pos = 140.0f;
    float v_start_pos = 0.0f;
    float v_size = 0.0f;
};

struct link_endpoint_t {
    CK_ID id = -1;
    int index = 0;
    int type = 0;
    //types:
    // 5 - pOut link (shortcut) (has the same id as the link target)
    // 7 - pIn
    // 8 - pOut
    // 9 - pLocal
    //10 - target pIn
    //12 - bIn
    //13 - bOut
    //26 - "Start" bIn

    link_endpoint_t() = default;
    link_endpoint_t(CK_ID id, int idx, int i) : id(id), index(idx), type(i) {}
};

struct link_t {
    CK_ID id = -1;
    int type = 0; // 1: g_ColorIndexBlack, 2: g_ColorIndexRed
    link_endpoint_t start;
    int point_count;
    std::vector<point_t> points;
    link_endpoint_t end;
};

struct op_t {
    CK_ID id = -1;
    float h_pos = 0.0f;
    float v_pos = 0.0f;
};

struct comment_t {
    // Ignored
};

enum param_style_enum {
    param_style_name = 0x200,
    param_style_closed = 0x400,
    param_style_namevalue = 0x1000,
    param_style_value = 0x2000
};

struct param_t {
    CK_ID id = -1;
    int h_pos = 0;
    int v_pos = 0;
    param_style_enum style = param_style_name;
    CK_ID source_id = -1;
};

struct bb_t {
    CK_ID id = -1;
    bool folded = false;
    int depth = 0;
    rect_t size;
    float h_expand_size = 0.0f;
    float v_expand_size = 0.0f;
    bool is_bg = false;
    int n_links = 0;
    std::vector<link_t> links;
    int n_ops = 0;
    std::vector<op_t> ops;
    int n_comments = 0;
    std::vector<comment_t> comments;
    int n_local_param = 0;
    std::vector<param_t> local_params;
    int n_shared_param = 0;
    std::vector<param_t> shared_params;
    int input_count = 0;
    std::vector<int> inward_inputs;
    std::vector<int> outward_inputs;
    int output_count = 0;
    std::vector<int> inward_outputs;
    std::vector<int> outward_outputs;
};

struct interface_t {
    start_t start;
    bb_t script_root;
    int n_bb = 0;
    std::vector<bb_t> bbs;
};
