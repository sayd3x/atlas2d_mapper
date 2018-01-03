#include "chain_node.hpp"

using namespace ::std;

namespace {
    
    /// The class safely forwards any calls to a specific node.
    class safe_forwarder: public atlas_builder {
    public:
        bool begin_atlas(atlas_props const& props) {
            return node ? node->begin_atlas(props) : true;
        }
        
        bool add_atlas_item(atlas_item const& item) {
            return node ? node->add_atlas_item(item) : true;
        }
        
        bool end_atlas(bool finalize) {
            return node ? node->end_atlas(finalize) : true;
        }
        
        void reset() {
            if(node)
                node->reset();
        }
        
    public:
        chain_node_ptr node;
    };
    
}

struct chain_node::Pimpl {
    safe_forwarder forwarder;
};

chain_node::chain_node(): _pimpl(new Pimpl) {
    ;;
}

chain_node::~chain_node() {
    ;;
}

atlas_builder& chain_node::safe_fwd() {
    return _pimpl->forwarder;
}

chain_node_ptr chain_node::set_child(chain_node_ptr child) {
    _pimpl->forwarder.node = move(child);
    return _pimpl->forwarder.node;
}

chain_node_ptr chain_node::child_node() const {
    return _pimpl->forwarder.node;
}

