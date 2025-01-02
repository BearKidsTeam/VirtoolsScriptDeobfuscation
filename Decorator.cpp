#include <map>
#include <queue>
#include <set>
#include <utility>

#include <CKAll.h>

#include "InterfaceData.h"

#undef max
#undef min

class Decorator {
    interface_t &data;
    CKContext *ctx;
    const int MAX_FIX_STACK_OPS = 3;

public:
    Decorator(interface_t &target_data, CKContext *context) : data(target_data), ctx(context) {}

    void decorate_start(bb_t &script, float v_start_pos, float v_size) {
        data.start.id = script.id;
        data.start.v_size = v_size;
        data.start.v_start_pos = v_start_pos;
        data.start.v_start = 0;
    }

    std::map<CK_ID, int> bmap;
    std::map<CK_ID, std::pair<int, int> > opmap;
    std::set<CK_ID> pins;
    std::set<CK_ID> pouts;
    std::set<CK_ID> moved_ops;
#define mappedb(id) (~bmap[id]?data.bbs[bmap[id]]:data.script_root)
#define mappedop(id) (~opmap[id].first?data.bbs[opmap[id].first].ops[opmap[id].second]:data.script_root.ops[opmap[id].second])

    struct pio_pos_t {
        CK_ID id;
        int idx;
        CK_ID link_within;
    };

    pio_pos_t GetpInPos(CKParameterIn *pin, CKBehavior **owner) {
        pio_pos_t ret = {};
        CKObject *ob = pin->GetOwner();
        ret.id = ob->GetID();
        if (ob->GetClassID() == CKCID_BEHAVIOR) {
            CKBehavior *obb = (CKBehavior *) ob;
            ret.idx = obb->GetInputParameterPosition(pin);
            if (obb->IsUsingTarget() && obb->GetTargetParameter()->GetID() == pin->GetID())
                ret.idx = -2;
            *owner = obb->GetParent();
            ret.link_within = (*owner)->GetID();
            return ret;
        }
        if (ob->GetClassID() == CKCID_PARAMETEROPERATION) {
            CKParameterOperation *obop = (CKParameterOperation *) ob;
            ret.idx = obop->GetInParameter1()->GetID() == pin->GetID() ? 0 : 1;
            *owner = obop->GetOwner();
            ret.link_within = (*owner)->GetID();
            return ret;
        }

        throw;
    }

    pio_pos_t GetpOutPos(CKParameterOut *pout, CKBehavior **owningbeh) {
        pio_pos_t ret = {};
        CKObject *ob = pout->GetOwner();
        ret.id = ob->GetID();
        if (ob->GetClassID() == CKCID_BEHAVIOR) {
            CKBehavior *obb = (CKBehavior *) ob;
            ret.idx = obb->GetOutputParameterPosition(pout);
            *owningbeh = obb->GetParent();
            ret.link_within = (*owningbeh)->GetID();
            return ret;
        }
        if (ob->GetClassID() == CKCID_PARAMETEROPERATION) {
            CKParameterOperation *obop = (CKParameterOperation *) ob;
            ret.idx = 0;
            *owningbeh = obop->GetOwner();
            ret.link_within = (*owningbeh)->GetID();
            return ret;
        }

        throw;
    }

    pio_pos_t GetpLocalPos(CKParameterLocal *plocal) {
        pio_pos_t ret = {};
        CKObject *ob = plocal->GetOwner();
        assert(ob->GetClassID() == CKCID_BEHAVIOR);
        CKBehavior *obb = (CKBehavior *) ob;
        ret.id = obb->GetID();
        ret.idx = obb->GetLocalParameterPosition(plocal);
        ret.link_within = ret.id;
        return ret;
    }

    link_endpoint_t GetParameterEndpoint(CKParameter *p) {
        if (p->GetClassID() == CKCID_PARAMETERLOCAL) {
            pio_pos_t t = GetpLocalPos((CKParameterLocal *) p);
            return {t.id, t.idx, 9};
        }
        CKBehavior *dummy;
        pio_pos_t t = GetpOutPos((CKParameterOut *) p, &dummy);
        return {t.id, t.idx, 8};
    }

    CKBehavior *GetParameterOwnerBehavior(CKParameter *p) {
        if (p->GetClassID() == CKCID_PARAMETERLOCAL)
            return (CKBehavior *) p->GetOwner();
        if (p->GetClassID() == CKCID_PARAMETEROUT) {
            CKObject *owner = p->GetOwner();
            if (owner->GetClassID() == CKCID_BEHAVIOR)
                return ((CKBehavior *) owner)->GetParent();
            if (owner->GetClassID() == CKCID_PARAMETEROPERATION)
                return ((CKParameterOperation *) owner)->GetOwner();
            throw;
        }
        throw;
    }

    // Get a shortcut for `source` within the behavior `within`.
    // If such shortcut doesn't exist, create it.
    pio_pos_t GetShortcutParamPos(CK_ID within, CK_ID source) {
        for (int i = 0, c = mappedb(within).n_shared_param; i < c; ++i) {
            if (mappedb(within).shared_params[i].source_id == source) {
                return {within, i, within};
            }
        }

        param_t p;
        p.source_id = source;
        p.h_pos = p.v_pos = 0;
        p.id = source;
        p.style = param_style_closed;
        mappedb(within).shared_params.push_back(p);
        ++mappedb(within).n_shared_param;
        return {within,mappedb(within).n_shared_param - 1, within};
    }

    //try reconstructing reasonable plinks...
    //just iterate through all pIns and pOuts of all subbehaviors
    //WARNING: this will look very ugly
    //**UNTESTED**
    //Q: Why we have to configure these outside?
    //A: Because a pLink may not belong to parent of the ends.
    //   And cross-behavior pLinks may exist.
    void configure_plink(CKBehavior *root) {
        std::map<CK_ID, std::vector<pio_pos_t>> pin_chain;
        std::map<CK_ID, std::vector<pio_pos_t>> pout_chain;
        CKBehavior *cb;

        for (auto &id: pins) {
            auto &vp = pin_chain[id] = {};
            auto *pin = (CKParameterIn *) ctx->GetObject(id);
            vp.push_back(GetpInPos(pin, &cb));
            link_endpoint_t last = {vp.back().id, vp.back().idx, vp.back().idx == -2 ? 10 : 7};
            for (; cb && cb->GetInputParameterPosition(pin) != -1; cb = cb->GetParent()) {
                vp.push_back({cb->GetID(), cb->GetInputParameterPosition(pin), cb->GetParent()->GetID()});
                link_t link_exp;
                link_exp.id = 0;
                link_exp.type = 0x10002;
                link_exp.point_count = 0;
                link_exp.start = {vp.back().id, vp.back().idx, 7};
                link_exp.end = last;
                last = link_exp.start;
                mappedb(cb->GetID()).links.push_back(link_exp);
                ++mappedb(cb->GetID()).n_links;
            }
        }

        for (auto &id: pouts) {
            std::vector<pio_pos_t> &vp = pout_chain[id] = std::vector<pio_pos_t>();
            CKParameterOut *pout = (CKParameterOut *) ctx->GetObject(id);
            vp.push_back(GetpOutPos(pout, &cb));
            link_endpoint_t last = {vp.back().id, vp.back().idx, 8};
            for (; cb && cb->GetOutputParameterPosition(pout) != -1; cb = cb->GetParent()) {
                vp.push_back(pio_pos_t{cb->GetID(), cb->GetOutputParameterPosition(pout), cb->GetParent()->GetID()});
                link_t link_exp;
                link_exp.id = 0;
                link_exp.type = 0x10002;
                link_exp.point_count = 0;
                link_exp.end = {vp.back().id, vp.back().idx, 8};
                link_exp.start = last;
                last = link_exp.end;
                mappedb(cb->GetID()).links.push_back(link_exp);
                ++mappedb(cb->GetID()).n_links;
            }
        }

        for (auto &id: pins) {
            CKParameterIn *pin = (CKParameterIn *) ctx->GetObject(id);
            pio_pos_t pos = GetpInPos(pin, &cb);
            std::vector<pio_pos_t> &vpin = pin_chain[pin->GetID()];

            if (pin->GetDirectSource()) {
                CKParameter *src = pin->GetDirectSource();
                std::vector<pio_pos_t> &vpsrc = pout_chain[src->GetID()];
                if (src->GetClassID() == CKCID_PARAMETERLOCAL && vpsrc.empty())
                    vpsrc.push_back(GetpLocalPos((CKParameterLocal *) src));

                bool conn = false;
                for (auto &aa: vpin) {
                    for (auto &bb: vpsrc) {
                        // direct connection within this behavior
                        if (aa.link_within == bb.link_within) {
                            link_t link;
                            link.id = 0;
                            link.type = 2;
                            link.point_count = 0;
                            link.start = {bb.id, bb.idx, src->GetClassID() == CKCID_PARAMETERLOCAL ? 9 : 8};
                            link.end = {aa.id, aa.idx, aa.idx == -2 ? 10 : 7};
                            mappedb(aa.link_within).links.push_back(link);
                            ++mappedb(aa.link_within).n_links;
                            conn = true;
                            break;
                        }
                    }
                    if (conn)
                        break;
                }
                // still not connected, use a shortcut instead
                if (!conn) {
                    link_t link;
                    link.id = 0;
                    link.type = 2;
                    link.point_count = 0;
                    pio_pos_t sshp = GetShortcutParamPos(pos.link_within, src->GetID());
                    link.start = {pos.link_within, sshp.idx, 5};
                    link.end = {pos.id, pos.idx, pos.idx == -2 ? 10 : 7};
                    mappedb(pos.link_within).links.push_back(link);
                    ++mappedb(pos.link_within).n_links;
                }
            } else if (pin->GetSharedSource()) {
                CKParameterIn *shpin = pin->GetSharedSource();
                assert(shpin->GetOwner()->GetClassID() == CKCID_BEHAVIOR);
                std::vector<pio_pos_t> &vshpin = pin_chain[shpin->GetID()];
                // no shortcut here!
                bool conn = false;
                for (auto &aa: vpin) {
                    for (auto &bb: vshpin) {
                        if (aa.link_within == bb.id) {
                            link_t link;
                            link.id = 0;
                            link.type = 2;
                            link.point_count = 0;
                            link.start = {bb.id, bb.idx, 7};
                            link.end = {aa.id, aa.idx, aa.idx == -2 ? 10 : 7};
                            mappedb(aa.link_within).links.push_back(link);
                            ++mappedb(aa.link_within).n_links;
                            conn = true;
                            break;
                        }
                    }
                    if (conn)
                        break;
                }
                if (!conn)
                    ctx->OutputToConsoleEx("pin: can't connect %d <-> %d, source type is %d", pin->GetID(), shpin->GetID(), shpin->GetClassID());
            }
        }

        // up to here we only have pOut->pOut and pOut->pLocal missing
        // so we iterate through all pOuts
        for (auto &i: pouts) {
            CKParameterOut *pout = (CKParameterOut *) ctx->GetObject(i);
            std::vector<pio_pos_t> &vpout = pout_chain[pout->GetID()];
            for (int j = 0, cd = pout->GetDestinationCount(); j < cd; ++j) {
                CKParameter *dest = pout->GetDestination(j);
                link_endpoint_t dendp = GetParameterEndpoint(dest);
                CK_ID dest_within = dest->GetOwner()->GetID();

                bool conn = false;
                for (auto &aa: vpout)
                    if (aa.link_within == dest_within) {
                        link_t link;
                        link.id = 0;
                        link.type = 2;
                        link.point_count = 0;
                        link.start = {aa.id, aa.idx, 8};
                        link.end = dendp;
                        mappedb(aa.link_within).links.push_back(link);
                        ++mappedb(aa.link_within).n_links;
                        conn = true;
                        break;
                    }
                if (!conn) {
                    //when the pOut connects to a shortcut
                    if (dest->GetClassID() == CKCID_PARAMETERLOCAL) {
                        link_t link;
                        link.id = 0;
                        link.type = 2;
                        link.point_count = 0;
                        pio_pos_t ssp = vpout.front();
                        pio_pos_t sshp = GetShortcutParamPos(ssp.link_within, dest->GetID());
                        link.start = {ssp.id, ssp.idx, 8};
                        link.end = {sshp.id, sshp.idx, 5};
                        mappedb(ssp.link_within).links.push_back(link);
                        ++mappedb(ssp.link_within).n_links;
                    } else {
                        ctx->OutputToConsoleEx("pout: can't connect %d <-> %d, dest type is %d", pout->GetID(), dest->GetID(), dest->GetClassID());
                    }
                }
            }
        }
    }

    struct vertex_t {
        int degree = 0;
        int first = -1;
    };

    std::map<CK_ID, vertex_t> vertexes;
    std::map<CK_ID, int> min_dist;
    std::map<CK_ID, rect_t> req_size;
    std::map<CK_ID, int> pre;
    std::map<CK_ID, std::vector<int> > bridges;

    struct edge_t {
        CK_ID from;
        CK_ID to;
        int next = -1;
    };

    std::vector<edge_t> edges;

    void add_edge(CK_ID from, CK_ID to) {
        edge_t edge = {};
        edge.from = from;
        edge.to = to;
        edge.next = vertexes[edge.from].first;
        vertexes[edge.from].first = edges.size();
        vertexes[edge.to].degree++;
        edges.push_back(edge);
    }

    void construct_graph(bb_t &bg, CKBehavior *beh) {
        vertexes.clear();
        edges.clear();

        vertexes[beh->GetID()] = vertex_t();
        const int count = beh->GetSubBehaviorCount();
        for (int i = 0; i < count; ++i) {
            CKBehavior *subBeh = beh->GetSubBehavior(i);
            vertexes[subBeh->GetID()] = vertex_t();
        }

        // reversed edge insertion
        for (int i = bg.links.size() - 1; i >= 0; --i) {
            link_t &link = bg.links[i];
            if (link.type == 1) // blink
                add_edge(link.start.id, link.end.id);
        }

        CK_ID from = beh->GetID();
        for (auto &kv: vertexes) {
            // A node without input, unconnected graph
            if (kv.first != beh->GetID() && kv.second.degree == 0) {
                // Add a virtual edge and make them a chain (in case there are many)
                add_edge(from, kv.first);
                from = kv.first;
            }
        }
    }

    void get_min_dist_inner(std::queue<CK_ID> &myq) {
        while (!myq.empty()) {
            CK_ID from = myq.front();
            myq.pop();
            for (int p = vertexes[from].first; p != -1; p = edges[p].next) {
                CK_ID to = edges[p].to;
                if (min_dist.find(to) == min_dist.end()) {
                    min_dist[to] = min_dist[from] + 1;
                    pre[to] = p;
                    myq.push(to);
                }
            }
        }
    }

    void get_min_dist(bb_t &bg) {
        min_dist.clear();
        pre.clear();
        min_dist[bg.id] = 0;
        std::queue<CK_ID> myq;
        myq.push(bg.id);
        get_min_dist_inner(myq);
        for (auto &kv: vertexes) {
            if (min_dist.find(kv.first) == min_dist.end()) {
                // still not a connected graph
                // we ignore the case because it's really rare
            }
        }
    }

    rect_t calc_bb_subgraph_size(bb_t &bb, bool root) {
        CK_ID from = bb.id;
        rect_t size = bb.size;
        if (root) {
            size.h_size = 0.0f;
            size.v_size = 0.0f;
        }
        int count = 0;
        float all_v_size = 0;
        float all_h_size = 0;
        for (int p = vertexes[from].first; p != -1; p = edges[p].next) {
            CK_ID to = edges[p].to;
            if (pre.find(to) != pre.end() && pre[to] == p) {
                rect_t sub_size = calc_bb_subgraph_size(mappedb(to), false);
                all_v_size += sub_size.v_size + 20.0f * 2;
                all_h_size = std::max(all_h_size, sub_size.h_size);
                count++;
            }
        }
        all_v_size -= 20.0f * 2;
        size.v_size = std::max(size.v_size, all_v_size);
        size.h_size = size.h_size + (all_h_size > 0.0f ? all_h_size + 20.0f * 2 : 0.0f);
        return req_size[bb.id] = size;
    }

    void place_bb_within(bb_t &bb, float h_pos, float v_pos, bool root) {
        if (!root) {
            bb.size.h_pos = h_pos;
            bb.size.v_pos = v_pos + (req_size[bb.id].v_size - bb.size.v_size) / 2;
        }
        int count = 0;
        float all_v_size = 0;
        CK_ID from = bb.id;
        for (int p = vertexes[from].first; p != -1; p = edges[p].next) {
            CK_ID to = edges[p].to;
            if (pre.find(to) != pre.end() && pre[to] == p) {
                rect_t sub_size = req_size[to];
                place_bb_within(mappedb(to), h_pos + (root ? 20.0f : bb.size.h_size + 20.0f * 2), v_pos + all_v_size, false);
                all_v_size += sub_size.v_size + 20.0f * 2;
                count++;
            }
        }
        all_v_size -= 20.0f * 2;
    }

    float calc_bb_positions(bb_t &bg, CKBehavior *beh, bool is_script) {
        construct_graph(bg, beh);
        get_min_dist(bg);
        rect_t size = calc_bb_subgraph_size(bg, true);
        bg.h_expand_size = size.h_size + 20.0f * 4;
        bg.v_expand_size = size.v_size + 20.0f * 4;
        place_bb_within(bg, (is_script ? 140.0f : 0.0f) + 20.0f * 2, 20.0f * 2, true);
        return size.v_size / 2;
    }

    void param_move_to(param_t &p, point_t position) {
        p.h_pos = (int) roundf(position.h);
        p.v_pos = (int) roundf(position.v);
    }

    void op_move_to(op_t &p, point_t position) {
        p.h_pos = (position.h - 1) * 20;
        p.v_pos = (position.v - 2) * 20;
    }

    point_t get_interface_input_pos(CK_ID target, int input_pos) {
        point_t p = {};
        if (is_op(target)) {
            op_t &op = mappedop(target);
            p.h = roundf(op.h_pos / 20.0f) + input_pos * 2;
            p.v = roundf(op.v_pos / 20.0f);
        } else {
            bb_t &bb = mappedb(target);
            float bb_h_pos = roundf(bb.size.h_pos / 20.0f);
            float bb_v_pos = roundf(bb.size.v_pos / 20.0f);
            p.h = bb_h_pos + (float) input_pos;
            p.v = bb_v_pos - 1.0f;
        }
        return p;
    }

    point_t get_interface_output_pos(CK_ID target, int output_pos) {
        point_t p = {};
        if (is_op(target)) {
            op_t &op = mappedop(target);
            p.h = roundf(op.h_pos / 20.0f) + 1;
            p.v = roundf(op.v_pos / 20.0f) + 2;
        } else {
            bb_t &bb = mappedb(target);
            float bb_h_pos = roundf(bb.size.h_pos / 20.0f);
            float bb_v_pos = roundf(bb.size.v_pos / 20.0f);
            p.h = bb_h_pos + (float) output_pos;
            p.v = bb_v_pos + roundf(bb.size.v_size / 20.0f) + 1;
        }
        return p;
    }

    bool is_op(CK_ID id) {
        return ctx->GetObjectA(id)->GetClassID() == CKCID_PARAMETEROPERATION;
    }

    void calc_op_positions(bb_t &bg, CKBehavior *beh) {
        for (auto &plink: bg.links) {
            if (plink.type == 2) {
                if (plink.start.type == 8 && is_op(plink.start.id)) {
                    op_t *start = &mappedop(plink.start.id);
                    if (plink.end.type == 7)
                        op_move_to(*start, get_interface_input_pos(plink.end.id, plink.end.index));
                    else if (plink.end.type == 10)
                        op_move_to(*start, get_interface_input_pos(plink.end.id, -1));
                }
            }
        }
    }

    void calc_param_local_positions(bb_t &bg, CKBehavior *beh, bool direction) {
        for (auto &plink: bg.links) {
            if (plink.type == 2) {
                if (direction == true) {
                    param_t *start = nullptr;
                    if (plink.start.type == 9)
                        start = &bg.local_params[plink.start.index];
                    else if (plink.start.type == 5)
                        start = &bg.shared_params[plink.start.index];
                    if (start) {
                        if (plink.end.type == 7)
                            param_move_to(*start, get_interface_input_pos(plink.end.id, plink.end.index));
                        else if (plink.end.type == 10)
                            param_move_to(*start, get_interface_input_pos(plink.end.id, -1));
                    }
                } else {
                    param_t *end = nullptr;
                    if (plink.end.type == 9)
                        end = &bg.local_params[plink.end.index];
                    if (end) {
                        if (plink.start.type == 8)
                            param_move_to(*end, get_interface_output_pos(plink.start.id, plink.start.index));
                    }
                }
            }
        }
    }

    void calc_bb_size(bb_t &bb, CKBehavior *beh) {
        if (bb.depth > 0) {
            int height = std::max(beh->GetOutputCount(), beh->GetInputCount());
            height = std::max(height, 1);

            int width = std::max(beh->GetOutputParameterCount(), beh->GetInputParameterCount());
            width = std::max(width, int((strlen(beh->GetName()) - 1) / 2.5) + 1);
            width = std::max(width, 2);

            bb.size.h_size = (float) width * 20.0f;
            bb.size.v_size = (float) height * 20.0f;
            if (bb.is_bg) {
                bb.h_expand_size = bb.size.h_size * 10;
                bb.v_expand_size = bb.size.v_size * 10;
            }
        }
    }

    void recalc_absolute_bb_pos(bb_t &bb, CKBehavior *beh, float start_h, float start_v) {
        if (bb.depth == 0) {
            bb.size.h_pos = 0;
            bb.size.v_pos = 0;
        }
        bb.size.h_pos += start_h;
        bb.size.v_pos += start_v;
        if (bb.is_bg) {
            const int count = beh->GetSubBehaviorCount();
            for (int i = 0; i < count; ++i) {
                CKBehavior *sub_beh = beh->GetSubBehavior(i);
                recalc_absolute_bb_pos(mappedb(sub_beh->GetID()), sub_beh, bb.size.h_pos, bb.size.v_pos);
            }

            const int opCount = beh->GetParameterOperationCount();
            for (int i = 0; i < opCount; ++i) {
                op_t &op = mappedop(beh->GetParameterOperation(i)->GetID());
                op.h_pos += bb.size.h_pos;
                op.v_pos += bb.size.v_pos;
            }
        }
    }

    void decorate_bb(bb_t &bb, CKBehavior *beh, int depth) {
        bb.id = beh->GetID();
        bb.folded = true;
        bb.depth = depth;
        bb.is_bg = beh->GetType() != CKBEHAVIORTYPE_BASE;

        calc_bb_size(bb, beh);

        for (int i = 0, c = beh->GetInputParameterCount(); i < c; ++i)
            pins.insert(beh->GetInputParameter(i)->GetID());

        for (int i = 0, c = beh->GetOutputParameterCount(); i < c; ++i)
            pouts.insert(beh->GetOutputParameter(i)->GetID());

        if (beh->IsUsingTarget())
            pins.insert(beh->GetTargetParameter()->GetID());

        for (int i = 0, c = beh->GetParameterOperationCount(); i < c; ++i) {
            CKParameterOperation *op = beh->GetParameterOperation(i);
            pins.insert(op->GetInParameter1()->GetID());
            pins.insert(op->GetInParameter2()->GetID());
            pouts.insert(op->GetOutParameter()->GetID());
        }

        if (bb.is_bg) {
            for (int i = 0, c = beh->GetSubBehaviorLinkCount(); i < c; ++i) {
                CKBehaviorLink *blink = beh->GetSubBehaviorLink(i);

                link_t link;
                link.type = 1;
                link.id = blink->GetID();
                link.point_count = 0;
                link.start = link.end = link_endpoint_t();

                CKBehaviorIO *inIO = blink->GetInBehaviorIO();
                CKBehavior *inBeh = inIO->GetOwner();
                link.start.id = inBeh->GetID();
                link.start.type = 13;
                link.start.index = inBeh->GetOutputPosition(inIO);
                if (!~link.start.index) {
                    link.start.index = inBeh->GetInputPosition(inIO);
                    link.start.type = 12;
                    if (inBeh->GetType() == CKBEHAVIORTYPE_SCRIPT)
                        link.start.type = 26;
                }

                CKBehaviorIO *outIO = blink->GetOutBehaviorIO();
                CKBehavior *outBeh = outIO->GetOwner();
                link.end.id = outBeh->GetID();
                link.end.type = 12;
                link.end.index = outBeh->GetInputPosition(outIO);
                if (!~link.end.index) {
                    link.end.index = outBeh->GetOutputPosition(outIO);
                    link.end.type = 13;
                }

                bb.links.push_back(link);
            }

            for (int i = 0, c = beh->GetParameterOperationCount(); i < c; ++i) {
                CKParameterOperation *pop = beh->GetParameterOperation(i);

                op_t op;
                op.id = pop->GetID();
                bb.ops.push_back(op);
            }
            bb.n_ops = bb.ops.size();

            for (int i = 0, c = beh->GetLocalParameterCount(); i < c; ++i) {
                CKParameterLocal *pl = beh->GetLocalParameter(i);

                param_t p;
                p.id = pl->GetID();
                p.style = param_style_closed;
                bb.local_params.push_back(p);
            }
            bb.n_local_param = bb.local_params.size();
        }
    }

    void decorate_bbs(CKBehavior *beh) {
        data.n_bb = 0;
        data.bbs.clear();
        bmap.clear();
        opmap.clear();
        pins.clear();
        pouts.clear();

        std::queue<std::pair<CKBehavior *, int>> bq;
        bq.emplace(beh, 0);
        while (!bq.empty()) {
            CKBehavior *beh = bq.front().first;
            const int depth = bq.front().second;
            bq.pop();

            if (depth > 0) {
                data.bbs.emplace_back();
                ++data.n_bb;
            }

            bmap[beh->GetID()] = depth > 0 ? data.bbs.size() - 1 : -1;
            const int opCount = beh->GetParameterOperationCount();
            for (int i = 0; i < opCount; ++i) {
                CKParameterOperation *op = beh->GetParameterOperation(i);
                opmap[op->GetID()] = std::make_pair(depth > 0 ? data.bbs.size() - 1 : -1, i);
            }

            bb_t &bb = depth > 0 ? data.bbs.back() : data.script_root;
            decorate_bb(bb, beh, depth);

            const int count = beh->GetSubBehaviorCount();
            for (int i = 0; i < count; ++i) {
                CKBehavior *subBeh = beh->GetSubBehavior(i);
                bq.emplace(subBeh, depth + 1);
            }
        }

        configure_plink(beh);

        for (auto &kv: bmap) {
            bb_t &sub_bb = mappedb(kv.first);
            if (sub_bb.is_bg) {
                calc_bb_positions(mappedb(kv.first), (CKBehavior *) ctx->GetObjectA(sub_bb.id), sub_bb.depth == 0);
            }
        }

        for (auto &kv: bmap) {
            bb_t &sub_bb = mappedb(kv.first);
            if (sub_bb.is_bg) {
                for (int i = 0; i < MAX_FIX_STACK_OPS; ++i)
                    calc_op_positions(mappedb(kv.first), (CKBehavior *) ctx->GetObjectA(sub_bb.id));
                calc_param_local_positions(mappedb(kv.first), (CKBehavior *) ctx->GetObjectA(sub_bb.id), false);
                calc_param_local_positions(mappedb(kv.first), (CKBehavior *) ctx->GetObjectA(sub_bb.id), true);
            }
        }
    }

    // To create missing entries in interface_t
    void decorate(CKBehavior *script) {
        decorate_bbs(script);
        float bb_height = std::max(req_size[script->GetID()].v_size + 4 * 20.0f, 200.0f);
        float bb_start_v = bb_height / 2.0f;
        decorate_start(data.script_root, bb_start_v, bb_height);
        recalc_absolute_bb_pos(data.script_root, script, 0.0f, 0.0f);
    }
};

void decorate(interface_t &data, CKBehavior *beh) {
    Decorator decorator = Decorator(data, beh->GetCKContext());
    decorator.decorate(beh);
}
