#pragma once

#include "atlas_builder.hpp"

/**
 * @brief Builder chaining node.
 * Extends the atlas_builder class by chaining it with each other.
 * It helps to build more complex rules.
 */
class chain_node: public atlas_builder {
protected:
    chain_node();
    
    /**
     * @brief Returns an object which safely forwards any calls to the binded child node.
     * Using this object you don't have to worry whether the child node is null or not.
     */
    atlas_builder& safe_fwd();

public:
    virtual ~chain_node();

    /**
     * @brief Binds specific node as a child node and returns the node.
     * It helps to simplify building a chain of builders.
     * @code
     *  node->set_child(node1)->set_child(node2)->set_child(node3);
     * @endcode
     * The next example will bind all the nodes together in node-node1-node2-node3 chain.
     */
    chain_node_ptr set_child(chain_node_ptr child);
    
    /// Returns binded child node
    chain_node_ptr child_node() const;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> _pimpl;
};
