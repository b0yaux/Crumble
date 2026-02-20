#include "Node.h"

ofJson Node::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void Node::deserialize(const ofJson& json) {
    ofDeserialize(json, parameters);
}
